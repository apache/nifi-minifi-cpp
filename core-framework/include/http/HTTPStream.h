// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
    if (http_client_read_future_.valid()) {
      http_client_read_future_.get();
    }
    if (http_client_write_future_.valid()) {
      http_client_write_future_.get();
    }
    return http_client_;
  }

  void forceClose() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (read_started_ || write_started_) {
      close();
      http_client_->forceClose();
      read_started_ = false;
      write_started_ = false;
    }

    if (http_client_read_future_.valid()) {
      http_client_read_future_.get();
    }
    if (http_client_write_future_.valid()) {
      http_client_write_future_.get();
    }
  }

  void seek(size_t offset) override;

  [[nodiscard]] size_t tell() const override;

  [[nodiscard]] size_t size() const override;

  using BaseStream::write;
  using BaseStream::read;

  size_t read(std::span<std::byte> buf) override;
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
    if (!http_client_read_future_.valid()) {
      return false;
    }
    return http_client_read_future_.wait_for(std::chrono::seconds(seconds)) == std::future_status::ready
        && getByteOutputReadCallback()
        && getByteOutputReadCallback()->getSize() == 0
        && getByteOutputReadCallback()->waitingOps();
  }

  bool waitForDataAvailable() {
    if (!http_client_read_future_.valid()) {
      return false;
    }
    do {
      logger_->log_trace("Waiting for more data");
    } while (http_client_read_future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready
        && getByteOutputReadCallback()
        && getByteOutputReadCallback()->getSize() == 0);

    return getByteOutputReadCallback()
        && getByteOutputReadCallback()->getSize() > 0;
  }

 protected:
  std::shared_ptr<HTTPClient> http_client_;
  std::future<bool> http_client_read_future_;
  std::future<bool> http_client_write_future_;

  std::mutex mutex_;

  std::atomic<bool> read_started_{false};
  std::atomic<bool> write_started_{false};

 private:
  utils::ByteOutputCallback* getByteOutputReadCallback() {
    return dynamic_cast<utils::ByteOutputCallback*>(http_client_->getReadCallback());
  }

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<HttpStream>::getLogger();
};
}  // namespace org::apache::nifi::minifi::http
