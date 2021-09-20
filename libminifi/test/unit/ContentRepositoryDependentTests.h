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

struct ReadFlowFileIntoString : public minifi::InputStreamCallback {
  std::string value_;

  int64_t process(const std::shared_ptr<minifi::io::BaseStream> &stream) override {
    value_.clear();
    std::vector<uint8_t> buffer;
    size_t bytes_read = stream->read(buffer, stream->size());
    value_.assign(buffer.begin(), buffer.end());
    return minifi::io::isError(bytes_read) ? -1 : gsl::narrow<int64_t>(bytes_read);
  }
};

class DummyProcessor : public core::Processor {
  using core::Processor::Processor;
};

REGISTER_RESOURCE(DummyProcessor, "A processor that does nothing.");

template<class ContentRepositoryClass>
class Fixture {
 public:
  const core::Relationship Success{"success", "everything is fine"};
  const core::Relationship Failure{"failure", "something has gone awry"};

  Fixture() {
    test_plan_ = test_controller_.createPlan(nullptr, nullptr, std::make_shared<ContentRepositoryClass>());
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

 private:
  TestController test_controller_;

  std::shared_ptr<TestPlan> test_plan_;
  std::shared_ptr<core::Processor> dummy_processor_;
  std::shared_ptr<core::ProcessContext> context_;
  std::unique_ptr<core::ProcessSession> process_session_;
};

template<class ContentRepositoryClass>
void testReadOnSmallerClonedFlowFiles() {
  Fixture<ContentRepositoryClass> fixture;
  core::ProcessSession& process_session = fixture.processSession();
  fixture.commitFlowFile("foobar");
  const auto original_ff = process_session.get();
  REQUIRE(original_ff);
  auto clone_first_half = process_session.clone(original_ff, 0, 3);
  auto clone_second_half = process_session.clone(original_ff, 3, 3);
  REQUIRE(clone_first_half != nullptr);
  REQUIRE(clone_second_half != nullptr);
  ReadFlowFileIntoString read_callback;
  process_session.read(original_ff, &read_callback);
  CHECK(original_ff->getSize() == 6);
  CHECK(read_callback.value_ == "foobar");
  process_session.read(clone_first_half, &read_callback);
  CHECK(clone_first_half->getSize() == 3);
  CHECK(read_callback.value_ == "foo");
  process_session.read(clone_second_half, &read_callback);
  CHECK(clone_second_half->getSize() == 3);
  CHECK(read_callback.value_ == "bar");
}
}  // namespace ContentRepositoryDependentTests
