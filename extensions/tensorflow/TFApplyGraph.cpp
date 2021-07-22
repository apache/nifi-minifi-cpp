/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "TFApplyGraph.h"
#include <tensorflow/cc/ops/standard_ops.h>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property TFApplyGraph::InputNode(
    core::PropertyBuilder::createProperty("Input Node")
        ->withDescription(
            "The node of the TensorFlow graph to feed tensor inputs to")
        ->withDefaultValue("")
        ->build());

core::Property TFApplyGraph::OutputNode(
    core::PropertyBuilder::createProperty("Output Node")
        ->withDescription(
            "The node of the TensorFlow graph to read tensor outputs from")
        ->withDefaultValue("")
        ->build());

core::Relationship TFApplyGraph::Success(  // NOLINT
    "success",
    "Successful graph application outputs");
core::Relationship TFApplyGraph::Retry(  // NOLINT
    "retry",
    "Inputs which fail graph application but may work if sent again");
core::Relationship TFApplyGraph::Failure(  // NOLINT
    "failure",
    "Failures which will not work if retried");

void TFApplyGraph::initialize() {
  std::set<core::Property> properties;
  properties.insert(InputNode);
  properties.insert(OutputNode);
  setSupportedProperties(std::move(properties));

  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Retry);
  relationships.insert(Failure);
  setSupportedRelationships(std::move(relationships));
}

void TFApplyGraph::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  context->getProperty(InputNode.getName(), input_node_);

  if (input_node_.empty()) {
    logger_->log_error("Invalid input node");
  }

  context->getProperty(OutputNode.getName(), output_node_);

  if (output_node_.empty()) {
    logger_->log_error("Invalid output node");
  }
}

void TFApplyGraph::onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/,
                             const std::shared_ptr<core::ProcessSession>& session) {
  auto flow_file = session->get();

  if (!flow_file) {
    return;
  }

  try {
    // Read graph
    std::string tf_type;
    flow_file->getAttribute("tf.type", tf_type);

    std::shared_ptr<tensorflow::GraphDef> graph_def;
    uint32_t graph_version;

    {
      std::lock_guard<std::mutex> guard(graph_def_mtx_);

      if ("graph" == tf_type) {
        logger_->log_info("Reading new graph def");
        graph_def_ = std::make_shared<tensorflow::GraphDef>();
        GraphReadCallback graph_cb(graph_def_);
        session->read(flow_file, &graph_cb);
        graph_version_++;
        logger_->log_info("Read graph version: %i", graph_version_);
        session->remove(flow_file);
        return;
      }

      graph_version = graph_version_;
      graph_def = graph_def_;
    }

    if (!graph_def) {
      logger_->log_error("Cannot process input because no graph has been defined");
      session->transfer(flow_file, Retry);
      return;
    }

    // Use an existing context, if one is available
    std::shared_ptr<TFContext> ctx;

    if (tf_context_q_.try_dequeue(ctx)) {
      logger_->log_debug("Using available TensorFlow context");

      if (ctx->graph_version != graph_version) {
        logger_->log_info("Allowing session with stale graph to expire");
        ctx = nullptr;
      }
    }

    if (!ctx) {
      logger_->log_info("Creating new TensorFlow context");
      tensorflow::SessionOptions options;
      ctx = std::make_shared<TFContext>();
      ctx->tf_session.reset(tensorflow::NewSession(options));
      ctx->graph_version = graph_version;
      auto status = ctx->tf_session->Create(*graph_def);

      if (!status.ok()) {
        std::string msg = "Failed to create TensorFlow session: ";
        msg.append(status.ToString());
        throw std::runtime_error(msg);
      }
    }

    // Apply graph
    // Read input tensor from flow file
    auto input_tensor_proto = std::make_shared<tensorflow::TensorProto>();
    TensorReadCallback tensor_cb(input_tensor_proto);
    session->read(flow_file, &tensor_cb);
    tensorflow::Tensor input;
    if (!input.FromProto(*input_tensor_proto)) {
      // failure deliberately ignored at this time
      // added to avoid warn_unused_result build errors
    }
    std::vector<tensorflow::Tensor> outputs;
    auto status = ctx->tf_session->Run({{input_node_, input}}, {output_node_}, {}, &outputs);

    if (!status.ok()) {
      std::string msg = "Failed to apply TensorFlow graph: ";
      msg.append(status.ToString());
      throw std::runtime_error(msg);
    }

    // Create output flow file for each output tensor
    for (const auto &output : outputs) {
      auto tensor_proto = std::make_shared<tensorflow::TensorProto>();
      output.AsProtoTensorContent(tensor_proto.get());
      logger_->log_info("Writing output tensor flow file");
      TensorWriteCallback write_cb(tensor_proto);
      session->write(flow_file, &write_cb);
      session->transfer(flow_file, Success);
    }

    // Make context available for use again
    if (tf_context_q_.size_approx() < getMaxConcurrentTasks()) {
      logger_->log_debug("Releasing TensorFlow context");
      tf_context_q_.enqueue(ctx);
    } else {
      logger_->log_info("Destroying TensorFlow context because it is no longer needed");
    }
  } catch (std::exception &exception) {
    logger_->log_error("Caught Exception %s", exception.what());
    session->transfer(flow_file, Failure);
    this->yield();
  } catch (...) {
    logger_->log_error("Caught Exception");
    session->transfer(flow_file, Failure);
    this->yield();
  }
}

int64_t TFApplyGraph::GraphReadCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  std::string graph_proto_buf;
  graph_proto_buf.resize(stream->size());
  const auto num_read = stream->read(reinterpret_cast<uint8_t *>(&graph_proto_buf[0]), stream->size());
  if (num_read != stream->size()) {
    throw std::runtime_error("GraphReadCallback failed to fully read flow file input stream");
  }
  graph_def_->ParseFromString(graph_proto_buf);
  return gsl::narrow<int64_t>(num_read);
}

int64_t TFApplyGraph::TensorReadCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  std::string tensor_proto_buf;
  tensor_proto_buf.resize(stream->size());
  const auto num_read = stream->read(reinterpret_cast<uint8_t *>(&tensor_proto_buf[0]), stream->size());
  if (num_read != stream->size()) {
    throw std::runtime_error("TensorReadCallback failed to fully read flow file input stream");
  }
  tensor_proto_->ParseFromString(tensor_proto_buf);
  return gsl::narrow<int64_t>(num_read);
}

int64_t TFApplyGraph::TensorWriteCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  auto tensor_proto_buf = tensor_proto_->SerializeAsString();
  auto num_wrote = stream->write(reinterpret_cast<uint8_t *>(&tensor_proto_buf[0]),
                                     static_cast<int>(tensor_proto_buf.size()));

  if (num_wrote != tensor_proto_buf.size()) {
    throw std::runtime_error("TensorWriteCallback failed to fully write flow file output stream");
  }

  return num_wrote;
}

REGISTER_RESOURCE(TFApplyGraph, "Applies a TensorFlow graph to the tensor protobuf supplied as input. The tensor is fed into the node specified by the Input Node property. "
    "The output FlowFile is a tensor protobuf extracted from the node specified by the Output Node property. TensorFlow graphs are read dynamically by feeding a graph "
    "protobuf to the processor with the tf.type property set to graph."); // NOLINT

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
