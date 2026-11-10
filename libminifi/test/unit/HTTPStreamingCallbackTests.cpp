// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <chrono>
#include <cstring>
#include <cstdint>

#include "http/HTTPCallback.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

class HttpStreamingCallbackTestsFixture {
 public:
  HttpStreamingCallbackTestsFixture() {
    LogTestController::getInstance().setTrace<http::HttpStreamingCallback>();
  }

  HttpStreamingCallbackTestsFixture(HttpStreamingCallbackTestsFixture&&) = delete;
  HttpStreamingCallbackTestsFixture(const HttpStreamingCallbackTestsFixture&) = delete;
  HttpStreamingCallbackTestsFixture& operator=(HttpStreamingCallbackTestsFixture&&) = delete;
  HttpStreamingCallbackTestsFixture& operator=(const HttpStreamingCallbackTestsFixture&) = delete;

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
    consumer_thread_ = std::thread([this]() {
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
  http::HttpStreamingCallback callback_;
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

  REQUIRE(content.empty());
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
}  // namespace org::apache::nifi::minifi::test
