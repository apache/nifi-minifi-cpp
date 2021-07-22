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

#include <set>
#include <utility>
#include <vector>

#include "MotionDetector.h"
#include "FrameIO.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property MotionDetector::ImageEncoding(
    core::PropertyBuilder::createProperty("Image Encoding")
        ->withDescription("The encoding that should be applied to the output")
        ->isRequired(true)
        ->withAllowableValues<std::string>({".jpg", ".png"})
        ->withDefaultValue(".jpg")->build());
core::Property MotionDetector::MinInterestArea(
    core::PropertyBuilder::createProperty("Minimum Area")
        ->withDescription("We only consider the movement regions with area greater than this.")
        ->isRequired(true)
        ->withDefaultValue<uint32_t>(100)->build());
core::Property MotionDetector::Threshold(
    core::PropertyBuilder::createProperty("Threshold for segmentation")
        ->withDescription("Pixel greater than this will be white, otherwise black.")
        ->isRequired(true)
        ->withDefaultValue<uint32_t>(42)->build());
core::Property MotionDetector::BackgroundFrame(
    core::PropertyBuilder::createProperty("Path to background frame")
        ->withDescription("If not provided then the processor will take the first input frame as background")
        ->isRequired(true)
        ->build());
core::Property MotionDetector::DilateIter(
    core::PropertyBuilder::createProperty("Dilate iteration")
        ->withDescription("For image processing, if an object is detected as 2 separate objects, increase this value")
        ->isRequired(true)
        ->withDefaultValue<uint32_t>(10)->build());

core::Relationship MotionDetector::Success("success", "Successful to detect motion");
core::Relationship MotionDetector::Failure("failure", "Failure to detect motion");

void MotionDetector::initialize() {
  std::set<core::Property> properties;
  properties.insert(ImageEncoding);
  properties.insert(MinInterestArea);
  properties.insert(Threshold);
  properties.insert(BackgroundFrame);
  properties.insert(DilateIter);
  setSupportedProperties(std::move(properties));

  setSupportedRelationships({Success, Failure});
}

void MotionDetector::onSchedule(const std::shared_ptr<core::ProcessContext> &context,
                                  const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  std::string value;

  if (context->getProperty(ImageEncoding.getName(), value)) {
    image_encoding_ = value;
  }

  if (context->getProperty(MinInterestArea.getName(), value)) {
    core::Property::StringToInt(value, min_area_);
  }

  if (context->getProperty(Threshold.getName(), value)) {
    core::Property::StringToInt(value, threshold_);
  }

  if (context->getProperty(DilateIter.getName(), value)) {
    core::Property::StringToInt(value, dil_iter_);
  }

  if (context->getProperty(BackgroundFrame.getName(), value) && !value.empty()) {
    bg_img_ = cv::imread(value, cv::IMREAD_GRAYSCALE);
    double scale = IMG_WIDTH / bg_img_.size().width;
    cv::resize(bg_img_, bg_img_, cv::Size(0, 0), scale, scale);
    cv::GaussianBlur(bg_img_, bg_img_, cv::Size(21, 21), 0, 0);
    bg_img_.convertTo(background_, CV_32F);
  }

  logger_->log_trace("MotionDetector processor scheduled");
}

bool MotionDetector::detectAndDraw(cv::Mat &frame) {
  cv::Mat gray;
  cv::Mat img_diff, thresh;
  std::vector<cv::Mat> contours;

  logger_->log_trace("Detect and Draw");

  cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
  cv::GaussianBlur(gray, gray, cv::Size(21, 21), 0, 0);

  // Get difference between current frame and background
  logger_->log_trace("Get difference [%d x %d] [%d x %d]", bg_img_.rows, bg_img_.cols, gray.rows, gray.cols);
  cv::absdiff(gray, bg_img_, img_diff);
  logger_->log_trace("Apply threshold");
  cv::threshold(img_diff, thresh, threshold_, 255, cv::THRESH_BINARY);
  // Image processing.
  logger_->log_trace("Dilation");
  cv::dilate(thresh, thresh, cv::Mat(), cv::Point(-1, -1), dil_iter_);
  cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  // Finish process
  logger_->log_debug("Draw contours");
  bool moved = false;
  for (const auto &contour : contours) {
    auto area = cv::contourArea(contour);
    if (area < min_area_) {
      continue;
    }
    moved = true;
    cv::Rect bbox = cv::boundingRect(contour);
    cv::rectangle(frame, bbox.tl(), bbox.br(), cv::Scalar(0, 255, 0), 2, 8, 0);
  }
  logger_->log_trace("Updating background");
  if (!moved) {
    logger_->log_debug("Not moved");
    // Adaptive background, update background so that the illumnation does not affect that much.
    cv::accumulateWeighted(gray, background_, 0.5);
    cv::convertScaleAbs(background_, bg_img_);
  }
  logger_->log_trace("Finish Detect and Draw");
  return moved;
}

void MotionDetector::onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                                 const std::shared_ptr<core::ProcessSession> &session) {
  std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    logger_->log_info("Cannot process due to an unfinished onTrigger");
    context->yield();
    return;
  }

  auto flow_file = session->get();
  if (flow_file->getSize() == 0) {
    logger_->log_info("Empty flow file");
    return;
  }
  cv::Mat frame;

  opencv::FrameReadCallback cb(frame);
  session->read(flow_file, &cb);

  if (frame.empty()) {
    logger_->log_error("Empty frame.");
    session->transfer(flow_file, Failure);
  }

  double scale = IMG_WIDTH / frame.size().width;
  cv::resize(frame, frame, cv::Size(0, 0), scale, scale);

  if (background_.empty()) {
    logger_->log_info("Background is missing, update and yield.");
    cv::cvtColor(frame, bg_img_, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(bg_img_, bg_img_, cv::Size(21, 21), 0, 0);
    bg_img_.convertTo(background_, CV_32F);
    return;
  }
  logger_->log_trace("Start motion detecting");

  auto t = std::time(nullptr);
  auto tm = *std::localtime(&t);
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H-%M-%S");
  auto filename = oss.str();
  filename.append(image_encoding_);

  detectAndDraw(frame);

  opencv::FrameWriteCallback write_cb(frame, image_encoding_);

  session->putAttribute(flow_file, "filename", filename);

  session->write(flow_file, &write_cb);
  session->transfer(flow_file, Success);
  logger_->log_trace("Finish motion detecting");
}

void MotionDetector::notifyStop() {
}

REGISTER_RESOURCE(MotionDetector, "Detect motion from captured images.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
