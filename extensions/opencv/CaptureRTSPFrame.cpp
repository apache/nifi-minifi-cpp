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

#include "CaptureRTSPFrame.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

const core::Property CaptureRTSPFrame::RTSPUsername(
    "RTSP Username",
    "The username for connecting to the RTSP stream", "");
const core::Property CaptureRTSPFrame::RTSPPassword(
    "RTSP Password",
    "Password used to connect to the RTSP stream", "");
const core::Property CaptureRTSPFrame::RTSPHostname(
    "RTSP Hostname",
    "Hostname of the RTSP stream we are trying to connect to", "");
const core::Property CaptureRTSPFrame::RTSPURI(
    "RTSP URI",
    "URI that should be appended to the RTSP stream hostname", "");
const core::Property CaptureRTSPFrame::RTSPPort(
    "RTSP Port",
    "Port that should be connected to to receive RTSP Frames",
    "");
const core::Property CaptureRTSPFrame::ImageEncoding(
    "Image Encoding",
    "The encoding that should be applied the the frame images captured from the RTSP stream",
    ".jpg");

const core::Relationship CaptureRTSPFrame::Success(
    "success",
    "Successful capture of RTSP frame");
const core::Relationship CaptureRTSPFrame::Failure(
    "failure",
    "Failures to capture RTSP frame");

void CaptureRTSPFrame::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void CaptureRTSPFrame::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  std::string value;

  if (context->getProperty(RTSPUsername.getName(), value)) {
    rtsp_username_ = value;
  }
  if (context->getProperty(RTSPPassword.getName(), value)) {
    rtsp_password_ = value;
  }
  if (context->getProperty(RTSPHostname.getName(), value)) {
    rtsp_host_ = value;
  }
  if (context->getProperty(RTSPPort.getName(), value)) {
    rtsp_port_ = value;
  }
  if (context->getProperty(RTSPURI.getName(), value)) {
    rtsp_uri_ = value;
  }
  if (context->getProperty(ImageEncoding.getName(), value)) {
    image_encoding_ = value;
  }

  logger_->log_trace("CaptureRTSPFrame processor scheduled");

  std::string rtspURI = "rtsp://";
  rtspURI.append(rtsp_username_);
  rtspURI.append(":");
  rtspURI.append(rtsp_password_);
  rtspURI.append("@");
  rtspURI.append(rtsp_host_);
  if (!rtsp_port_.empty()) {
    rtspURI.append(":");
    rtspURI.append(rtsp_port_);
  }

  if (!rtsp_uri_.empty()) {
    rtspURI.append("/");
    rtspURI.append(rtsp_uri_);
  }

  rtsp_url_ = rtspURI;
}

void CaptureRTSPFrame::onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                                 const std::shared_ptr<core::ProcessSession> &session) {
  std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    logger_->log_info("Cannot process due to an unfinished onTrigger");
    context->yield();
    return;
  }

  try {
    video_capture_.open(rtsp_url_);
    video_backend_driver_ = video_capture_.getBackendName();
  } catch (const cv::Exception &e) {
    logger_->log_error("Unable to open RTSP stream: %s", e.what());
    context->yield();
    return;
  } catch (...) {
    logger_->log_error("Unable to open RTSP stream: unhandled exception");
    context->yield();
    return;
  }

  auto flow_file = session->create();
  cv::Mat frame;
  // retrieve a frame of your source
  if (video_capture_.read(frame)) {
    if (!frame.empty()) {
      auto t = std::time(nullptr);
      auto tm = *std::localtime(&t);

      std::ostringstream oss;
      oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S");
      auto filename = oss.str();
      filename.append(image_encoding_);

      session->putAttribute(flow_file, "filename", filename);
      session->putAttribute(flow_file, "video.backend.driver", video_backend_driver_);

      session->write(flow_file, [&frame, this](const std::shared_ptr<io::OutputStream>& output_stream) -> int64_t {
        std::vector<uchar> image_buf;
        imencode(image_encoding_, frame, image_buf);
        const auto ret = output_stream->write(image_buf.data(), image_buf.size());
        return io::isError(ret) ? -1 : gsl::narrow<int64_t>(ret);
      });
      session->transfer(flow_file, Success);
      logger_->log_info("A frame is captured");
    } else {
      logger_->log_error("Empty Mat frame received from capture");
      session->transfer(flow_file, Failure);
    }
  } else {
    logger_->log_error("Unable to read from capture handle on RTSP stream");
    session->transfer(flow_file, Failure);
  }
}

void CaptureRTSPFrame::notifyStop() {
}

REGISTER_RESOURCE(CaptureRTSPFrame, Processor);

}  // namespace org::apache::nifi::minifi::processors
