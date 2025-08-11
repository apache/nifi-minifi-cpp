/**
 *
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

#include <memory>
#include <thread>
#include <mutex>
#include <future>
#include <vector>

#include "HTTPCallback.h"
#include "io/BaseStream.h"
#include "HTTPClient.h"

namespace org::apache::nifi::minifi::http {

class HttpStream : public io::BaseStreamImpl {
 public:
  /**
   * File Stream constructor that accepts an fstream shared pointer.
   * It must already be initialized for read and write.
   */
  explicit HttpStream(std::shared_ptr<HTTPClient> client);

  ~HttpStream() override {
    forceClose();
  }

  void close() override;

  const std::shared_ptr<HTTPClient>& getClientRef() {
    return http_client_;
  }

  const std::shared_ptr<HTTPClient>& getClient() {
    http_client_future_.get();
    return http_client_;
  }

  void forceClose() {
    if (started_) {
      // lock shouldn't be needed here as call paths currently guarantee
      // flow, but we should be safe anyway.
      std::lock_guard<std::mutex> lock(mutex_);
      close();
      http_client_->forceClose();
      if (http_client_future_.valid()) {
        http_client_future_.get();
      } else {
        logger_->log_warn("Future status already cleared for {}, continuing", http_client_->getURL());
      }

      started_ = false;
    }
  }

  /**
   * Skip to the specified offset.
   * @param offset offset to which we will skip
   */
  void seek(size_t offset) override;

  [[nodiscard]] size_t tell() const override;

  [[nodiscard]] size_t size() const override {
    return written;
  }

  using BaseStream::write;
  using BaseStream::read;

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  size_t read(std::span<std::byte> buf) override;

  /**
   * writes value to stream
   * @param value value to write
   * @param size size of value
   */
  size_t write(const uint8_t* value, size_t size) override;

  static bool submit_client(const std::shared_ptr<HTTPClient>& client) {
    if (client == nullptr)
      return false;
    bool submit_status = client->submit();
    return submit_status;
  }

  static bool submit_read_client(const std::shared_ptr<HTTPClient>& client, utils::ByteOutputCallback* callback) {
    if (client == nullptr)
      return false;
    bool submit_status = client->submit();
    callback->close();
    return submit_status;
  }

  inline bool isFinished(int seconds = 0) {
    return http_client_future_.wait_for(std::chrono::seconds(seconds)) == std::future_status::ready
        && http_client_->getReadCallback()
        && http_client_->getReadCallback()->getSize() == 0
        && http_client_->getReadCallback()->waitingOps();
  }

  /**
   * Waits for more data to become available.
   */
  bool waitForDataAvailable() {
    do {
      logger_->log_trace("Waiting for more data");
    } while (http_client_future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready
        && http_client_->getReadCallback()
        && http_client_->getReadCallback()->getSize() == 0);

    return http_client_->getReadCallback()
        && http_client_->getReadCallback()->getSize() > 0;
  }

 protected:
  std::vector<uint8_t> array;

  std::shared_ptr<HTTPClient> http_client_;
  std::future<bool> http_client_future_;

  size_t written{0};

  std::mutex mutex_;

  std::atomic<bool> started_{false};

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<HttpStream>::getLogger();
};
}  // namespace org::apache::nifi::minifi::http
