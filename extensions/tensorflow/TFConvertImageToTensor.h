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

#pragma once

#include <atomic>

#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/Resource.h"
#include "core/Processor.h"
#include "tensorflow/core/public/session.h"
#include "concurrentqueue.h"
#include "io/InputStream.h"
#include "io/OutputStream.h"

namespace org::apache::nifi::minifi::processors {

class TFConvertImageToTensor : public core::Processor {
 public:
  explicit TFConvertImageToTensor(const std::string &name, const utils::Identifier &uuid = {})
      : Processor(name, uuid),
        logger_(core::logging::LoggerFactory<TFConvertImageToTensor>::getLogger(uuid_)) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Converts the input image file into a tensor protobuf. The image will be resized to the given output tensor dimensions.";

  EXTENSIONAPI static constexpr auto ImageFormat = core::PropertyDefinitionBuilder<>::createProperty("Input Format")
      .withDescription("The format of the input image (PNG or RAW). RAW is RGB24.")
      .build();
  EXTENSIONAPI static constexpr auto NumChannels = core::PropertyDefinitionBuilder<>::createProperty("Channels")
      .withDescription("The number of channels (e.g. 3 for RGB, 4 for RGBA) in the input image")
      .withDefaultValue("3")
      .build();
  EXTENSIONAPI static constexpr auto InputWidth = core::PropertyDefinitionBuilder<>::createProperty("Input Width")
      .withDescription("The width, in pixels, of the input image.")
      .build();
  EXTENSIONAPI static constexpr auto InputHeight = core::PropertyDefinitionBuilder<>::createProperty("Input Height")
      .withDescription("The height, in pixels, of the input image.")
      .build();
  EXTENSIONAPI static constexpr auto OutputWidth = core::PropertyDefinitionBuilder<>::createProperty("Output Width")
      .withDescription("The width, in pixels, of the output image.")
      .build();
  EXTENSIONAPI static constexpr auto OutputHeight = core::PropertyDefinitionBuilder<>::createProperty("Output Height")
      .withDescription("The height, in pixels, of the output image.")
      .build();
  EXTENSIONAPI static constexpr auto CropOffsetX = core::PropertyDefinitionBuilder<>::createProperty("Crop Offset X")
      .withDescription("The X (horizontal) offset, in pixels, to crop the input image (relative to top-left corner).")
      .build();
  EXTENSIONAPI static constexpr auto CropOffsetY = core::PropertyDefinitionBuilder<>::createProperty("Crop Offset Y")
      .withDescription("The Y (vertical) offset, in pixels, to crop the input image (relative to top-left corner).")
      .build();
  EXTENSIONAPI static constexpr auto CropSizeX = core::PropertyDefinitionBuilder<>::createProperty("Crop Size X")
      .withDescription("The X (horizontal) size, in pixels, to crop the input image.")
      .build();
  EXTENSIONAPI static constexpr auto CropSizeY = core::PropertyDefinitionBuilder<>::createProperty("Crop Size Y")
      .withDescription("The Y (vertical) size, in pixels, to crop the input image.")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 10>{
      ImageFormat,
      NumChannels,
      InputWidth,
      InputHeight,
      OutputWidth,
      OutputHeight,
      CropOffsetX,
      CropOffsetY,
      CropSizeX,
      CropSizeY
  };


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successful graph application outputs"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Failures which will not work if retried"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

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
    int64_t process(const std::shared_ptr<io::InputStream>& stream) override;

   private:
    tensorflow::Tensor *tensor_;
  };

  class TensorWriteCallback : public OutputStreamCallback {
   public:
    explicit TensorWriteCallback(std::shared_ptr<tensorflow::TensorProto> tensor_proto)
        : tensor_proto_(std::move(tensor_proto)) {
    }
    ~TensorWriteCallback() override = default;
    int64_t process(const std::shared_ptr<io::OutputStream>& stream) override;

   private:
    std::shared_ptr<tensorflow::TensorProto> tensor_proto_;
  };

 private:
  std::shared_ptr<core::logging::Logger> logger_;

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

}  // namespace org::apache::nifi::minifi::processors
