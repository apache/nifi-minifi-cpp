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

#include <iomanip>
#include <memory>
#include <string>
#include <utility>

#include "core/logging/LoggerFactory.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc.hpp"

namespace org::apache::nifi::minifi::processors {

class MotionDetector : public core::ProcessorImpl {
 public:
  explicit MotionDetector(std::string name, const utils::Identifier &uuid = {})
      : Processor(std::move(name), uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Detect motion from captured images.";

  EXTENSIONAPI static constexpr auto ImageEncoding = core::PropertyDefinitionBuilder<2>::createProperty("Image Encoding")
      .withDescription("The encoding that should be applied to the output")
      .isRequired(true)
      .withAllowedValues({".jpg", ".png"})
      .withDefaultValue(".jpg")
      .build();
  EXTENSIONAPI static constexpr auto MinInterestArea = core::PropertyDefinitionBuilder<>::createProperty("Minimum Area")
      .withDescription("We only consider the movement regions with area greater than this.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("100")
      .build();
  EXTENSIONAPI static constexpr auto Threshold = core::PropertyDefinitionBuilder<>::createProperty("Threshold for segmentation")
      .withDescription("Pixel greater than this will be white, otherwise black.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("42")
      .build();
  EXTENSIONAPI static constexpr auto DilateIter = core::PropertyDefinitionBuilder<>::createProperty("Dilate iteration")
      .withDescription("For image processing, if an object is detected as 2 separate objects, increase this value")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("10")
      .build();
  EXTENSIONAPI static constexpr auto BackgroundFrame = core::PropertyDefinitionBuilder<>::createProperty("Path to background frame")
      .withDescription("If not provided then the processor will take the first input frame as background")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      ImageEncoding,
      MinInterestArea,
      Threshold,
      DilateIter,
      BackgroundFrame
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successful to detect motion"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Failure to detect motion"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  void notifyStop() override;

 private:
  bool detectAndDraw(cv::Mat &frame);

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<MotionDetector>::getLogger(uuid_);
  std::mutex mutex_;
  cv::Mat background_;
  cv::Mat bg_img_;
  std::string image_encoding_;
  int min_area_{};
  int threshold_{};
  int dil_iter_{};

  // hardcoded width to 500
  const double IMG_WIDTH = 500.0;
};

}  // namespace org::apache::nifi::minifi::processors
