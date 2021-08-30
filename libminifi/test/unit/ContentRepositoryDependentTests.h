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

#include <memory>
#include <string>
#include <vector>

#include <catch.hpp>
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "../TestBase.h"
#include "StreamPipe.h"

#pragma once

namespace ContentRepositoryDependentTests {

struct WriteStringToFlowFile : public minifi::OutputStreamCallback {
  const std::vector<uint8_t> buffer_;

  explicit WriteStringToFlowFile(const std::string& buffer) : buffer_(buffer.begin(), buffer.end()) {}

  int64_t process(const std::shared_ptr<minifi::io::BaseStream> &stream) override {
    size_t bytes_written = stream->write(buffer_, buffer_.size());
    return minifi::io::isError(bytes_written) ? -1 : gsl::narrow<int64_t>(bytes_written);
  }
};

struct ReadUntilStreamSize : public minifi::InputStreamCallback {
  std::string value_;

  int64_t process(const std::shared_ptr<minifi::io::BaseStream> &stream) override {
    value_.clear();
    std::vector<uint8_t> buffer;
    size_t bytes_read = stream->read(buffer, stream->size());
    value_.assign(buffer.begin(), buffer.end());
    return minifi::io::isError(bytes_read) ? -1 : gsl::narrow<int64_t>(bytes_read);
  }
};

struct ReadUntilItCan : public minifi::InputStreamCallback {
  std::string value_;

  int64_t process(const std::shared_ptr<minifi::io::BaseStream> &stream) override {
    value_.clear();
    std::vector<uint8_t> buffer;
    size_t bytes_read = 0;
    while (true) {
      size_t read_result = stream->read(buffer, 1024);
      if (minifi::io::isError(read_result))
        return -1;
      if (read_result == 0)
        return bytes_read;
      bytes_read += read_result;
      value_.append(buffer.begin(), buffer.end());
    }
  }
};

class DummyProcessor : public core::Processor {
  using core::Processor::Processor;
};

REGISTER_RESOURCE(DummyProcessor, "A processor that does nothing.");

class Fixture {
 public:
  const core::Relationship Success{"success", "everything is fine"};
  const core::Relationship Failure{"failure", "something has gone awry"};

  explicit Fixture(std::shared_ptr<core::ContentRepository> content_repo) {
    test_plan_ = test_controller_.createPlan(nullptr, nullptr, content_repo);
    dummy_processor_ = test_plan_->addProcessor("DummyProcessor", "dummyProcessor");
    context_ = [this] {
      test_plan_->runNextProcessor();
      return test_plan_->getCurrentContext();
    }();
    process_session_ = std::make_unique<core::ProcessSession>(context_);
  }

  core::ProcessSession &processSession() { return *process_session_; }

  void commitFlowFile(const std::string& content) {
    const auto original_ff = process_session_->create();
    WriteStringToFlowFile callback(content);
    process_session_->write(original_ff, &callback);
    process_session_->transfer(original_ff, Success);
    process_session_->commit();
  }

  void appendAndCommit(const std::shared_ptr<core::FlowFile>& flow_file, const std::string content_to_append) {
    WriteStringToFlowFile callback(content_to_append);
    process_session_->append(flow_file, &callback);
    process_session_->transfer(flow_file, Success);
    process_session_->commit();
  }

 private:
  TestController test_controller_;

  std::shared_ptr<TestPlan> test_plan_;
  std::shared_ptr<core::Processor> dummy_processor_;
  std::shared_ptr<core::ProcessContext> context_;
  std::unique_ptr<core::ProcessSession> process_session_;
};

void testReadOnSmallerClonedFlowFiles(std::shared_ptr<core::ContentRepository> content_repo) {
  Fixture fixture = Fixture(content_repo);
  core::ProcessSession& process_session = fixture.processSession();
  fixture.commitFlowFile("foobar");
  const auto original_ff = process_session.get();
  REQUIRE(original_ff);
  auto clone_first_half = process_session.clone(original_ff, 0, 3);
  auto clone_second_half = process_session.clone(original_ff, 3, 3);
  REQUIRE(clone_first_half != nullptr);
  REQUIRE(clone_second_half != nullptr);
  ReadUntilStreamSize read_until_stream_size_callback;
  ReadUntilItCan read_until_it_can_callback;
  process_session.read(original_ff, &read_until_stream_size_callback);
  process_session.read(original_ff, &read_until_it_can_callback);
  CHECK(original_ff->getSize() == 6);
  CHECK(read_until_stream_size_callback.value_ == "foobar");
  CHECK(read_until_it_can_callback.value_ == "foobar");
  process_session.read(clone_first_half, &read_until_stream_size_callback);
  process_session.read(clone_first_half, &read_until_it_can_callback);
  CHECK(clone_first_half->getSize() == 3);
  CHECK(read_until_stream_size_callback.value_ == "foo");
  CHECK(read_until_it_can_callback.value_ == "foo");
  process_session.read(clone_second_half, &read_until_stream_size_callback);
  process_session.read(clone_second_half, &read_until_it_can_callback);
  CHECK(clone_second_half->getSize() == 3);
  CHECK(read_until_stream_size_callback.value_ == "bar");
  CHECK(read_until_it_can_callback.value_ == "bar");
}

void testAppendSize(std::shared_ptr<core::ContentRepository> content_repo) {
  Fixture fixture = Fixture(content_repo);
  core::ProcessSession& process_session = fixture.processSession();
  fixture.commitFlowFile("my");
  const auto flow_file = process_session.get();
  fixture.appendAndCommit(flow_file, "foobar");
  REQUIRE(flow_file);
  CHECK(flow_file->getSize() == 8);
  ReadUntilStreamSize read_until_stream_size_callback;
  ReadUntilItCan read_until_it_can_callback;
  process_session.read(flow_file, &read_until_stream_size_callback);
  process_session.read(flow_file, &read_until_it_can_callback);
  CHECK(read_until_stream_size_callback.value_ == "myfoobar");
  CHECK(read_until_it_can_callback.value_ == "myfoobar");
}
}  // namespace ContentRepositoryDependentTests
