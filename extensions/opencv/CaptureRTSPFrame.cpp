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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

static core::Property rtspUsername;
static core::Property rtspPassword;
static core::Property rtspHostname;
static core::Property rtspURI;
static core::Property rtspPort;
static core::Property captureFrameRate;
static core::Property imageEncoding;

core::Property CaptureRTSPFrame::RTSPUsername(
    core::PropertyBuilder::createProperty("RTSP Username")
        ->withDescription("The username for connecting to the RTSP stream")
        ->isRequired(false)->build());

core::Property CaptureRTSPFrame::RTSPPassword(
    core::PropertyBuilder::createProperty("RTSP Password")
        ->withDescription("Password used to connect to the RTSP stream")
        ->isRequired(false)->build());

core::Property CaptureRTSPFrame::RTSPHostname(
    core::PropertyBuilder::createProperty("RTSP Hostname")
        ->withDescription("Hostname of the RTSP stream we are trying to connect to")
        ->isRequired(true)->build());

core::Property CaptureRTSPFrame::RTSPURI(
    core::PropertyBuilder::createProperty("RTSP URI")
        ->withDescription("URI that should be appended to the RTSP stream hostname")
        ->isRequired(true)->build());

core::Property CaptureRTSPFrame::RTSPPort(
    core::PropertyBuilder::createProperty("RTSP Port")
        ->withDescription("Port that should be connected to receive RTSP Frames")
        ->isRequired(true)
        ->withDefaultValue<uint16_t>(554)->build());

core::Property CaptureRTSPFrame::ImageEncoding(
    core::PropertyBuilder::createProperty("Image Encoding")
        ->withDescription("The encoding that should be applied the the frame images captured from the RTSP stream")
        ->isRequired(true)
        ->withAllowableValues<std::string>({".bmp", ".jpg", ".png", ".tiff"})
        ->withDefaultValue(".jpg")->build());

core::Relationship CaptureRTSPFrame::Success(  // NOLINT
    "success",
    "Successful capture of RTSP frame");
core::Relationship CaptureRTSPFrame::Failure(  // NOLINT
    "failure",
    "Failures to capture RTSP frame");

void CaptureRTSPFrame::initialize() {
  std::set<core::Property> properties;
  properties.insert(RTSPUsername);
  properties.insert(RTSPPassword);
  properties.insert(RTSPHostname);
  properties.insert(RTSPPort);
  properties.insert(RTSPURI);
  properties.insert(ImageEncoding);
  setSupportedProperties(std::move(properties));

  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Failure);
  setSupportedRelationships(std::move(relationships));
}

void CaptureRTSPFrame::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {

  std::string value;
  uint16_t portVal;

  if (context->getProperty(RTSPUsername.getName(), value)) {
    rtsp_username_ = value;
  }
  if (context->getProperty(RTSPPassword.getName(), value)) {
    rtsp_password_ = value;
  }
  if (context->getProperty(RTSPHostname.getName(), value)) {
    rtsp_host_ = value;
  }
  if (context->getProperty(RTSPPort.getName(), portVal)) {
    rtsp_port_ = portVal;
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
  if (rtsp_port > 0 && rtsp_port <= 65535) {
    rtspURI.append(":");
    rtspURI.append(std::to_string(rtsp_port));
  }

  if (!rtsp_uri_.empty()) {
    rtspURI.append("/");
    rtspURI.append(rtsp_uri_);
  }

  cv::VideoCapture capture(rtspURI.c_str());
  video_capture_ = capture;
  video_backend_driver_ = video_capture_.getBackendName();
}

void CaptureRTSPFrame::onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                                 const std::shared_ptr<core::ProcessSession> &session) {
  auto flow_file = session->create();

  cv::Mat frame;

  // retrieve a frame of your source
  if (video_capture_.read(frame)) {
    if (!frame.empty()) {
      CaptureRTSPFrameWriteCallback write_cb(frame, image_encoding_);

      auto t = std::time(nullptr);
      auto tm = *std::localtime(&t);

      std::ostringstream oss;
      oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S");
      auto filename = oss.str();
      filename.append(image_encoding_);

      session->putAttribute(flow_file, "filename", filename);
      session->putAttribute(flow_file, "video.backend.driver", video_backend_driver_);

      session->write(flow_file, &write_cb);
      session->transfer(flow_file, Success);
    } else {
      logger_->log_error("Empty Mat frame received from capture");
      session->transfer(flow_file, Failure);
    }
  } else {
    logger_->log_error("Unable to read from capture handle on RTSP stream");
    session->transfer(flow_file, Failure);
  }

  frame.release();

}

void CaptureRTSPFrame::notifyStop() {
  // Release the Capture reference and free up resources.
  video_capture_.release();
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
