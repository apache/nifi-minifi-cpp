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

#ifndef NIFI_MINIFI_CPP_TFCONVERTIMAGETOTENSOR_H
#define NIFI_MINIFI_CPP_TFCONVERTIMAGETOTENSOR_H

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

class TFConvertImageToTensor : public core::Processor {
 public:
  explicit TFConvertImageToTensor(const std::string &name, const utils::Identifier &uuid = {})
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<TFConvertImageToTensor>::getLogger()) {
  }

  static core::Property ImageFormat;
  static core::Property NumChannels;
  static core::Property InputWidth;
  static core::Property InputHeight;
  static core::Property OutputWidth;
  static core::Property OutputHeight;
  static core::Property CropOffsetX;
  static core::Property CropOffsetY;
  static core::Property CropSizeX;
  static core::Property CropSizeY;

  static core::Relationship Success;
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
  };

  class ImageReadCallback : public InputStreamCallback {
   public:
    explicit ImageReadCallback(tensorflow::Tensor *tensor)
        : tensor_(tensor) {
    }
    ~ImageReadCallback() override = default;
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override;

   private:
    tensorflow::Tensor *tensor_;
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

  std::string input_format_;
  int input_width_;
  int input_height_;
  int output_width_;
  int output_height_;
  int num_channels_;
  bool do_crop_ = false;
  int crop_offset_x_ = 0;
  int crop_offset_y_ = 0;
  int crop_size_x_ = 0;
  int crop_size_y_ = 0;

  std::shared_ptr<tensorflow::GraphDef> graph_def_;
  moodycamel::ConcurrentQueue<std::shared_ptr<TFContext>> tf_context_q_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // NIFI_MINIFI_CPP_TFCONVERTIMAGETOTENSOR_H
