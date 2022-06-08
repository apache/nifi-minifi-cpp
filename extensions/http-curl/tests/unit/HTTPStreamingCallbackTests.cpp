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

#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <chrono>
#include <cstring>
#include <cstdint>

#include "client/HTTPCallback.h"
#include "TestBase.h"
#include "Catch.h"

class HttpStreamingCallbackTestsFixture {
 public:
  HttpStreamingCallbackTestsFixture() {
    LogTestController::getInstance().setTrace<utils::HttpStreamingCallback>();
  }

  virtual ~HttpStreamingCallbackTestsFixture() {
    if (consumer_thread_.joinable()) {
      consumer_thread_.join();
    }
    LogTestController::getInstance().reset();
  }

  void startConsumerThread() {
    if (consumer_thread_.joinable()) {
      throw std::logic_error("Consumer thread already started");
    }
    consumer_thread_ = std::thread([this](){
      std::cerr << "Consumer thread started" << std::endl;

      size_t current_pos = 0U;

      while (true) {
        size_t buffer_size = callback_.getBufferSize();
        if (current_pos <= buffer_size) {
          size_t len = buffer_size - current_pos;
          auto* ptr = callback_.getBuffer(current_pos);
          if (ptr == nullptr) {
            break;
          }
          {
            std::unique_lock<std::mutex> lock(content_mutex_);
            content_.resize(content_.size() + len);
            memcpy(content_.data() + current_pos, ptr, len);
          }
          current_pos += len;
          callback_.seek(current_pos);
        }
      }
    });
  }

  std::string getContent() {
    std::unique_lock<std::mutex> lock(content_mutex_);
    return {content_.data(), content_.size()};
  }

  std::string waitForCompletionAndGetContent() {
    if (consumer_thread_.joinable()) {
      consumer_thread_.join();
    }
    return getContent();
  }

 protected:
  utils::HttpStreamingCallback callback_;
  std::mutex content_mutex_;
  std::vector<char> content_;
  std::thread consumer_thread_;
};


TEST_CASE_METHOD(HttpStreamingCallbackTestsFixture, "HttpStreamingCallback empty", "[basic]") {
  SECTION("with wait") {
    startConsumerThread();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  callback_.close();

  SECTION("without wait") {
    startConsumerThread();
  }

  std::string content = waitForCompletionAndGetContent();

  REQUIRE(0U == content.length());
}

TEST_CASE_METHOD(HttpStreamingCallbackTestsFixture, "HttpStreamingCallback one buffer", "[basic]") {
  SECTION("with wait") {
    startConsumerThread();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::string input = "foobar";
  callback_.process(reinterpret_cast<const uint8_t*>(input.c_str()), input.length());
  callback_.close();

  SECTION("without wait") {
    startConsumerThread();
  }

  std::string content = waitForCompletionAndGetContent();

  REQUIRE(input == content);
}

TEST_CASE_METHOD(HttpStreamingCallbackTestsFixture, "HttpStreamingCallback multiple buffers", "[basic]") {
  SECTION("with wait") {
    startConsumerThread();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::string input;
  for (size_t i = 0U; i < 16U; i++) {
    std::string chunk = std::to_string(i << 16);
    input += chunk;
    callback_.process(reinterpret_cast<const uint8_t*>(chunk.c_str()), chunk.length());
    if (i == 7U) {
      SECTION("with staggered wait" + std::to_string(i)) {
        startConsumerThread();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
  }
  SECTION("with wait before close") {
    startConsumerThread();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  callback_.close();

  SECTION("without wait") {
    startConsumerThread();
  }

  std::string content = waitForCompletionAndGetContent();

  REQUIRE(input == content);
}
