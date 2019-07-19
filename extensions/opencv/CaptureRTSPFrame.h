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

#ifndef NIFI_MINIFI_CPP_CAPTURERTSPFRAME_H
#define NIFI_MINIFI_CPP_CAPTURERTSPFRAME_H

#include <atomic>

#include <core/Resource.h>
#include <core/Processor.h>
#include <opencv2/opencv.hpp>

#include <iomanip>
#include <ctime>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class CaptureRTSPFrame : public core::Processor {

 public:

  explicit CaptureRTSPFrame(const std::string &name, utils::Identifier uuid = utils::Identifier())
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<CaptureRTSPFrame>::getLogger()) {
  }

  static core::Property RTSPUsername;
  static core::Property RTSPPassword;
  static core::Property RTSPHostname;
  static core::Property RTSPURI;
  static core::Property RTSPPort;
  static core::Property ImageEncoding;

  static core::Relationship Success;
  static core::Relationship Failure;

  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override {
    logger_->log_error("onTrigger invocation with raw pointers is not implemented");
  }
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                 const std::shared_ptr<core::ProcessSession> &session) override;

  void notifyStop() override;

  class CaptureRTSPFrameWriteCallback : public OutputStreamCallback {
   public:
    explicit CaptureRTSPFrameWriteCallback(cv::Mat image_mat, std::string image_encoding_)
        : image_mat_(std::move(image_mat)), image_encoding_(image_encoding_) {
    }
    ~CaptureRTSPFrameWriteCallback() override = default;

    int64_t process(std::shared_ptr<io::BaseStream> stream) override {
      int64_t ret = 0;
      imencode(image_encoding_, image_mat_, image_buf_);
      ret = stream->write(image_buf_.data(), image_buf_.size());
      return ret;
    }

   private:
    std::vector<uchar> image_buf_;
    cv::Mat image_mat_;
    std::string image_encoding_;
  };

 private:
  std::shared_ptr<logging::Logger> logger_;
  std::mutex mutex_;
  std::string rtsp_username_;
  std::string rtsp_password_;
  std::string rtsp_host_;
  std::string rtsp_port_;
  std::string rtsp_uri_;
  std::string rtsp_url_;
  cv::VideoCapture video_capture_;
  std::string image_encoding_;
  std::string video_backend_driver_;

//  std::function<int()> f_ex;
//
//  std::atomic<bool> running_;
//
//  std::unique_ptr<DataHandler> handler_;
//
//  std::vector<std::string> endpoints;
//
//  std::map<std::string, std::future<int>*> live_clients_;
//
//  utils::ThreadPool<int> client_thread_pool_;
//
//  moodycamel::ConcurrentQueue<std::unique_ptr<io::Socket>> socket_ring_buffer_;
//
//  bool stay_connected_;
//
//  uint16_t concurrent_handlers_;
//
//  int8_t endOfMessageByte;
//
//  uint64_t reconnect_interval_;
//
//  uint64_t receive_buffer_size_;
//
//  uint16_t connection_attempt_limit_;
//
//  std::shared_ptr<GetTCPMetrics> metrics_;
//
//  // Mutex for ensuring clients are running
//
//  std::mutex mutex_;
//
//  std::shared_ptr<minifi::controllers::SSLContextService> ssl_service_;

};

REGISTER_RESOURCE(CaptureRTSPFrame, "Captures a frame from the RTSP stream at specified intervals."); // NOLINT

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */


#endif //NIFI_MINIFI_CPP_CAPTURERTSPFRAME_H
