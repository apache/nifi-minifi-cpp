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

#include <utility>
#include <vector>

#include "MotionDetector.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

void MotionDetector::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void MotionDetector::onSchedule(const std::shared_ptr<core::ProcessContext> &context,
                                  const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  std::string value;

  if (context->getProperty(ImageEncoding, value)) {
    image_encoding_ = value;
  }

  if (context->getProperty(MinInterestArea, value)) {
    core::Property::StringToInt(value, min_area_);
  }

  if (context->getProperty(Threshold, value)) {
    core::Property::StringToInt(value, threshold_);
  }

  if (context->getProperty(DilateIter, value)) {
    core::Property::StringToInt(value, dil_iter_);
  }

  if (context->getProperty(BackgroundFrame, value) && !value.empty()) {
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
  cv::Mat img_diff;
  cv::Mat thresh;
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

  session->read(flow_file, [&frame](const std::shared_ptr<io::InputStream>& input_stream) -> int64_t {
    std::vector<uchar> image_buf;
    image_buf.resize(input_stream->size());
    const auto ret = input_stream->read(as_writable_bytes(std::span(image_buf)));
    if (io::isError(ret) || ret != input_stream->size()) {
      throw std::runtime_error("ImageReadCallback failed to fully read flow file input stream");
    }
    frame = cv::imdecode(image_buf, -1);
    return gsl::narrow<int64_t>(ret);
  });

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

  session->putAttribute(flow_file, "filename", filename);

  session->write(flow_file, [&frame, this](const auto& output_stream) -> int64_t {
    std::vector<uchar> image_buf;
    imencode(image_encoding_, frame, image_buf);
    const auto ret = output_stream->write(image_buf.data(), image_buf.size());
    return io::isError(ret) ? -1 : gsl::narrow<int64_t>(ret);
  });
  session->transfer(flow_file, Success);
  logger_->log_trace("Finish motion detecting");
}

void MotionDetector::notifyStop() {
}

REGISTER_RESOURCE(MotionDetector, Processor);

}  // namespace org::apache::nifi::minifi::processors
