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

#include "TFExtractTopLabels.h"

#include "tensorflow/cc/ops/standard_ops.h"

#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Relationship TFExtractTopLabels::Success(  // NOLINT
    "success",
    "Successful FlowFiles are sent here with labels as attributes");
core::Relationship TFExtractTopLabels::Retry(  // NOLINT
    "retry",
    "Failures which might work if retried");
core::Relationship TFExtractTopLabels::Failure(  // NOLINT
    "failure",
    "Failures which will not work if retried");

void TFExtractTopLabels::initialize() {
  std::set<core::Property> properties;
  setSupportedProperties(std::move(properties));

  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Retry);
  relationships.insert(Failure);
  setSupportedRelationships(std::move(relationships));
}

void TFExtractTopLabels::onSchedule(core::ProcessContext* /*context*/, core::ProcessSessionFactory* /*sessionFactory*/) {
}

void TFExtractTopLabels::onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/,
                                   const std::shared_ptr<core::ProcessSession> &session) {
  auto flow_file = session->get();

  if (!flow_file) {
    return;
  }

  try {
    // Read labels
    std::string tf_type;
    flow_file->getAttribute("tf.type", tf_type);
    std::shared_ptr<std::vector<std::string>> labels;

    {
      std::lock_guard<std::mutex> guard(labels_mtx_);

      if (tf_type == "labels") {
        logger_->log_info("Reading new labels...");
        auto new_labels = std::make_shared<std::vector<std::string>>();
        LabelsReadCallback cb(new_labels);
        session->read(flow_file, &cb);
        labels_ = new_labels;
        logger_->log_info("Read %d new labels", labels_->size());
        session->remove(flow_file);
        return;
      }

      labels = labels_;
    }

    // Read input tensor from flow file
    auto input_tensor_proto = std::make_shared<tensorflow::TensorProto>();
    TensorReadCallback tensor_cb(input_tensor_proto);
    session->read(flow_file, &tensor_cb);

    tensorflow::Tensor input;
    if (!input.FromProto(*input_tensor_proto)) {
      // failure deliberately ignored at this time
      // added to avoid warn_unused_result build errors
    }
    auto input_flat = input.flat<float>();

    std::vector<std::pair<uint64_t, float>> scores;

    for (int i = 0; i < input_flat.size(); i++) {
      scores.emplace_back(std::make_pair(i, input_flat(i)));
    }

    std::sort(scores.begin(), scores.end(), [](const std::pair<uint64_t, float> &a,
                                               const std::pair<uint64_t, float> &b) {
      return a.second > b.second;
    });

    for (std::size_t i = 0; i < 5 && i < scores.size(); i++) {
      if (!labels || scores[i].first > labels->size()) {
        logger_->log_error("Label index is out of range (are the correct labels loaded?); routing to retry...");
        session->transfer(flow_file, Retry);
        return;
      }
      flow_file->addAttribute("tf.top_label_" + std::to_string(i), labels->at(scores[i].first));
    }

    session->transfer(flow_file, Success);
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

int64_t TFExtractTopLabels::LabelsReadCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  size_t total_read = 0;
  std::string label;
  uint64_t max_label_len = 65536;
  label.resize(max_label_len);
  std::string buf;
  uint64_t label_size = 0;
  uint64_t buf_size = 8096;
  buf.resize(buf_size);

  while (total_read < stream->size()) {
    const auto read = stream->read(reinterpret_cast<uint8_t *>(&buf[0]), buf_size);
    if (io::isError(read)) break;
    for (size_t i = 0; i < read; i++) {
      if (buf[i] == '\n' || total_read + i == stream->size()) {
        labels_->emplace_back(label.substr(0, label_size));
        label_size = 0;
      } else {
        label[label_size] = buf[i];
        label_size++;
      }
    }

    total_read += read;
  }

  return gsl::narrow<int64_t>(total_read);
}

int64_t TFExtractTopLabels::TensorReadCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  std::string tensor_proto_buf;
  tensor_proto_buf.resize(stream->size());
  const auto num_read = stream->read(reinterpret_cast<uint8_t *>(&tensor_proto_buf[0]), stream->size());
  if (num_read != stream->size()) {
    throw std::runtime_error("TensorReadCallback failed to fully read flow file input stream");
  }
  tensor_proto_->ParseFromString(tensor_proto_buf);
  return gsl::narrow<int64_t>(num_read);
}

REGISTER_RESOURCE(TFExtractTopLabels, "Extracts the top 5 labels for categorical inference models"); // NOLINT

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
