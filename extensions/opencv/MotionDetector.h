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

#include "core/Resource.h"
#include "core/Processor.h"
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc.hpp"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class MotionDetector : public core::Processor {
 public:
  explicit MotionDetector(const std::string &name, const utils::Identifier &uuid = {})
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<MotionDetector>::getLogger()) {
  }

  static core::Property ImageEncoding;
  static core::Property MinInterestArea;
  static core::Property Threshold;
  static core::Property DilateIter;
  static core::Property BackgroundFrame;

  static core::Relationship Success;
  static core::Relationship Failure;

  void initialize(void) override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

  void notifyStop() override;

 private:
  bool detectAndDraw(cv::Mat &frame);

  std::shared_ptr<logging::Logger> logger_;
  std::mutex mutex_;
  cv::Mat background_;
  cv::Mat bg_img_;
  std::string image_encoding_;
  int min_area_;
  int threshold_;
  int dil_iter_;

  // hardcoded width to 500
  const double IMG_WIDTH = 500.0;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
