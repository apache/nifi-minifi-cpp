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

#ifndef NIFI_MINIFI_CPP_TFAPPLYGRAPH_H
#define NIFI_MINIFI_CPP_TFAPPLYGRAPH_H

#include <atomic>

#include <core/Resource.h>
#include <core/Processor.h>
#include <tensorflow/core/public/session.h>
#include <concurrentqueue.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class TFApplyGraph : public core::Processor {
 public:
  explicit TFApplyGraph(const std::string &name, const utils::Identifier &uuid = {})
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<TFApplyGraph>::getLogger()) {
  }

  static core::Property InputNode;
  static core::Property OutputNode;

  static core::Relationship Success;
  static core::Relationship Retry;
  static core::Relationship Failure;

  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* /*session*/) override {
    logger_->log_error("onTrigger invocation with raw pointers is not implemented");
  }
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                 const std::shared_ptr<core::ProcessSession> &session) override;

  struct TFContext {
    std::shared_ptr<tensorflow::Session> tf_session;
    uint32_t graph_version;
  };

  class GraphReadCallback : public InputStreamCallback {
   public:
    explicit GraphReadCallback(std::shared_ptr<tensorflow::GraphDef> graph_def)
        : graph_def_(std::move(graph_def)) {
    }
    ~GraphReadCallback() override = default;
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override;

   private:
    std::shared_ptr<tensorflow::GraphDef> graph_def_;
  };

  class TensorReadCallback : public InputStreamCallback {
   public:
    explicit TensorReadCallback(std::shared_ptr<tensorflow::TensorProto> tensor_proto)
        : tensor_proto_(std::move(tensor_proto)) {
    }
    ~TensorReadCallback() override = default;
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override;

   private:
    std::shared_ptr<tensorflow::TensorProto> tensor_proto_;
  };

  class TensorWriteCallback : public OutputStreamCallback {
   public:
    explicit TensorWriteCallback(std::shared_ptr<tensorflow::TensorProto> tensor_proto)
        : tensor_proto_(std::move(tensor_proto)) {
    }
    ~TensorWriteCallback() override = default;
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override;

   private:
    std::shared_ptr<tensorflow::TensorProto> tensor_proto_;
  };

 private:
  std::shared_ptr<logging::Logger> logger_;
  std::string input_node_;
  std::string output_node_;
  std::shared_ptr<tensorflow::GraphDef> graph_def_;
  std::mutex graph_def_mtx_;
  uint32_t graph_version_ = 0;
  moodycamel::ConcurrentQueue<std::shared_ptr<TFContext>> tf_context_q_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // NIFI_MINIFI_CPP_TFAPPLYGRAPH_H
