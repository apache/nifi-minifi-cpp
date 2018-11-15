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
#ifndef EXTENSIONS_HTTP_CURL_CLIENT_HTTPSTREAM_H_
#define EXTENSIONS_HTTP_CURL_CLIENT_HTTPSTREAM_H_

#include <memory>
#include <thread>
#include <mutex>
#include <future>
#include <vector>

#include "HTTPCallback.h"
#include "io/BaseStream.h"
#include "HTTPClient.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

class HttpStream : public io::BaseStream {
 public:
  /**
   * File Stream constructor that accepts an fstream shared pointer.
   * It must already be initialized for read and write.
   */
  explicit HttpStream(std::shared_ptr<utils::HTTPClient> http_client_);

  virtual ~HttpStream() {
    forceClose();
  }

  virtual void closeStream() override;

  const std::shared_ptr<utils::HTTPClient> &getClientRef() {
    return http_client_;
  }

  const std::shared_ptr<utils::HTTPClient> &getClient() {
    http_client_future_.get();
    return http_client_;
  }

  void forceClose() {
    if (started_) {
      // lock shouldn't be needed here as call paths currently guarantee
      // flow, but we should be safe anyway.
      std::lock_guard<std::mutex> lock(mutex_);
      closeStream();
      http_client_->forceClose();
      if (http_client_future_.valid()) {
        http_client_future_.get();
      } else {
        logger_->log_warn("Future status already cleared for %s, continuing", http_client_->getURL());
      }

      started_ = false;
    }
  }
  /**
   * Skip to the specified offset.
   * @param offset offset to which we will skip
   */
  virtual void seek(uint64_t offset) override;

  virtual const uint64_t getSize() const override {
    return written;
  }

  // data stream extensions
  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  virtual int readData(std::vector<uint8_t> &buf, int buflen) override;
  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  virtual int readData(uint8_t *buf, int buflen) override;

  /**
   * Write value to the stream using std::vector
   * @param buf incoming buffer
   * @param buflen buffer to write
   *
   */
  virtual int writeData(std::vector<uint8_t> &buf, int buflen);

  /**
   * writes value to stream
   * @param value value to write
   * @param size size of value
   */
  virtual int writeData(uint8_t *value, int size) override;

  /**
   * Returns the underlying buffer
   * @return vector's array
   **/
  const uint8_t *getBuffer() const {
    throw std::runtime_error("Stream does not support this operation");
  }

  static bool submit_client(std::shared_ptr<utils::HTTPClient> client) {
    if (client == nullptr)
      return false;
    bool submit_status = client->submit();
    return submit_status;
  }

  static bool submit_read_client(std::shared_ptr<utils::HTTPClient> client, utils::ByteOutputCallback *callback) {
    if (client == nullptr)
      return false;
    bool submit_status = client->submit();
    callback->close();
    return submit_status;
  }

  inline bool isFinished(int seconds = 0) {
    if (http_client_future_.wait_for(std::chrono::seconds(seconds)) == std::future_status::ready && (http_read_callback_.getSize() == 0 && http_read_callback_.waitingOps())) {
      return true;
    } else {
      return false;
    }
  }

  /**
   * Waits for more data to become available.
   */
  bool waitForDataAvailable() {
    do {
      logger_->log_trace("Waiting for more data");
    } while (http_client_future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready && http_read_callback_.getSize() == 0);

    return http_read_callback_.getSize() > 0;
  }

 protected:

  /**
   * Creates a vector and returns the vector using the provided
   * type name.
   * @param t incoming object
   * @returns vector.
   */
  template<typename T>
  std::vector<uint8_t> readBuffer(const T&);

  void reset();

  std::vector<uint8_t> array;

  std::shared_ptr<utils::HTTPClient> http_client_;
  std::future<bool> http_client_future_;

  size_t written;

  std::mutex mutex_;

  utils::HttpStreamingCallback http_callback_;

  utils::HTTPUploadCallback callback_;

  utils::ByteOutputCallback http_read_callback_;

  utils::HTTPReadCallback read_callback_;

  std::atomic<bool> started_;

 private:

  std::shared_ptr<logging::Logger> logger_;
};
} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_HTTP_CURL_CLIENT_HTTPSTREAM_H_ */
