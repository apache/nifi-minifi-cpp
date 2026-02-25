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

#include <array>
#include <memory>
#include <string>

#include "catch2/catch_test_macros.hpp"
#include "core/ProcessSession.h"
#include "core/Processor.h"
#include "io/StreamPipe.h"
#include "minifi-c/minifi-c.h"
#include "unit/DummyProcessor.h"
#include "unit/TestBase.h"

#pragma once

namespace ContentRepositoryDependentTests {

struct ReadUntilItCan {
  std::string value_;

  int64_t operator()(const std::shared_ptr<minifi::io::InputStream> &stream) {
    value_.clear();
    std::array<std::byte, 1024> buffer{};
    size_t bytes_read = 0;
    while (true) {
      const size_t read_result = stream->read(buffer);
      if (minifi::io::isError(read_result))
        return -1;
      if (read_result == 0)
        return gsl::narrow<int64_t>(bytes_read);
      bytes_read += read_result;
      const auto char_view = gsl::make_span(buffer).subspan(0, read_result).as_span<const char>();
      value_.append(std::begin(char_view), std::end(char_view));
    }
  }
};

class Fixture {
 public:
  const core::Relationship Success{"success", "everything is fine"};
  const core::Relationship Failure{"failure", "something has gone awry"};

  explicit Fixture(std::shared_ptr<core::ContentRepository> content_repo) {
    test_plan_ = test_controller_.createPlan(nullptr, std::nullopt, std::move(content_repo));
    dummy_processor_ = test_plan_->addProcessor("DummyProcessor", "dummyProcessor");
    context_ = [this] {
      test_plan_->runNextProcessor();
      return test_plan_->getCurrentContext();
    }();
    process_session_ = std::make_unique<core::ProcessSessionImpl>(context_);
  }

  [[nodiscard]] core::ProcessSession& processSession() const { return *process_session_; }

  void transferAndCommit(const std::shared_ptr<core::FlowFile>& flow_file) const {
    process_session_->transfer(flow_file, Success);
    process_session_->commit();
  }

  void writeToFlowFile(const std::shared_ptr<core::FlowFile>& flow_file, const std::string_view content) const {
    process_session_->writeBuffer(flow_file, content);
  }

  void appendToFlowFile(const std::shared_ptr<core::FlowFile>& flow_file, const std::string_view content_to_append) const {
    process_session_->add(flow_file);
    process_session_->appendBuffer(flow_file, content_to_append);
  }

 private:
  TestController test_controller_;

  std::shared_ptr<TestPlan> test_plan_;
  core::Processor* dummy_processor_;
  std::shared_ptr<core::ProcessContext> context_;
  std::unique_ptr<core::ProcessSession> process_session_;
};

inline void testReadOnSmallerClonedFlowFiles(std::shared_ptr<core::ContentRepository> content_repo) {
  auto fixture = Fixture(std::move(content_repo));
  core::ProcessSession& process_session = fixture.processSession();
  const auto original_ff = process_session.create();
  fixture.writeToFlowFile(original_ff, "foobar");
  fixture.transferAndCommit(original_ff);
  REQUIRE(original_ff);
  auto clone_first_half = process_session.clone(*original_ff, 0, 3);
  auto clone_second_half = process_session.clone(*original_ff, 3, 3);
  REQUIRE(clone_first_half != nullptr);
  REQUIRE(clone_second_half != nullptr);
  ReadUntilItCan read_until_it_can_callback;
  const auto read_result_original = process_session.readBuffer(original_ff);
  process_session.read(original_ff, std::ref(read_until_it_can_callback));
  CHECK(original_ff->getSize() == 6);
  CHECK(to_string(read_result_original) == "foobar");
  CHECK(read_until_it_can_callback.value_ == "foobar");
  const auto read_result_first_half = process_session.readBuffer(clone_first_half);
  process_session.read(clone_first_half, std::ref(read_until_it_can_callback));
  CHECK(clone_first_half->getSize() == 3);
  CHECK(to_string(read_result_first_half) == "foo");
  CHECK(read_until_it_can_callback.value_ == "foo");
  const auto read_result_second_half = process_session.readBuffer(clone_second_half);
  process_session.read(clone_second_half, std::ref(read_until_it_can_callback));
  CHECK(clone_second_half->getSize() == 3);
  CHECK(to_string(read_result_second_half) == "bar");
  CHECK(read_until_it_can_callback.value_ == "bar");
}

inline void testAppendToUnmanagedFlowFile(std::shared_ptr<core::ContentRepository> content_repo) {
  auto fixture = Fixture(std::move(content_repo));
  core::ProcessSession& process_session = fixture.processSession();
  const auto flow_file = process_session.create();
  REQUIRE(flow_file);

  fixture.writeToFlowFile(flow_file, "my");
  fixture.transferAndCommit(flow_file);
  fixture.appendToFlowFile(flow_file, "foobar");
  fixture.transferAndCommit(flow_file);

  CHECK(flow_file->getSize() == 8);
  ReadUntilItCan read_until_it_can_callback;
  const auto read_result = process_session.readBuffer(flow_file);
  process_session.read(flow_file, std::ref(read_until_it_can_callback));
  CHECK(to_string(read_result) == "myfoobar");
  CHECK(read_until_it_can_callback.value_ == "myfoobar");
}

inline void testAppendToManagedFlowFile(std::shared_ptr<core::ContentRepository> content_repo) {
  auto fixture = Fixture(std::move(content_repo));
  core::ProcessSession& process_session = fixture.processSession();
  const auto flow_file = process_session.create();
  REQUIRE(flow_file);

  fixture.writeToFlowFile(flow_file, "my");
  fixture.appendToFlowFile(flow_file, "foobar");
  fixture.transferAndCommit(flow_file);

  CHECK(flow_file->getSize() == 8);
  const auto read_result = process_session.readBuffer(flow_file);
  ReadUntilItCan read_until_it_can_callback;
  CHECK(to_string(read_result) == "myfoobar");
  process_session.read(flow_file, std::ref(read_until_it_can_callback));
  CHECK(read_until_it_can_callback.value_ == "myfoobar");
}

inline void testReadFromZeroLengthFlowFile(std::shared_ptr<core::ContentRepository> content_repo) {
  const auto fixture = Fixture(std::move(content_repo));
  core::ProcessSession& process_session = fixture.processSession();
  const auto flow_file = process_session.create();
  REQUIRE(flow_file);
  fixture.transferAndCommit(flow_file);

  CHECK(flow_file->getSize() == 0);
  REQUIRE_NOTHROW(process_session.readBuffer(flow_file));
  REQUIRE_NOTHROW(process_session.read(flow_file, ReadUntilItCan{}));
}

inline void testErrWrite(std::shared_ptr<core::ContentRepository> content_repo) {
  const auto fixture = Fixture(std::move(content_repo));
  core::ProcessSession& process_session = fixture.processSession();
  const auto flow_file = process_session.create();
  fixture.writeToFlowFile(flow_file, "original_content");
  fixture.transferAndCommit(flow_file);

  REQUIRE_THROWS(
  process_session.write(flow_file, [](const std::shared_ptr<minifi::io::OutputStream>& output_stream) {
    output_stream->write("new_content");
    return MinifiIoStatus::MINIFI_IO_ERROR;
  }));

  CHECK(flow_file->getSize() == 16);
  ReadUntilItCan read_until_it_can_callback;
  const auto read_result = process_session.readBuffer(flow_file);
  process_session.read(flow_file, std::ref(read_until_it_can_callback));
  CHECK(to_string(read_result) == "original_content");
}

inline void testOkWrite(std::shared_ptr<core::ContentRepository> content_repo) {
  const auto fixture = Fixture(std::move(content_repo));
  core::ProcessSession& process_session = fixture.processSession();
  const auto flow_file = process_session.create();
  fixture.writeToFlowFile(flow_file, "original_content");
  fixture.transferAndCommit(flow_file);

  CHECK(flow_file->getSize() == 16);

  process_session.write(flow_file, [](const std::shared_ptr<minifi::io::OutputStream>& output_stream) {
    constexpr std::string str = "new_content";
    return output_stream->write(as_bytes(std::span(str)));
  });

  CHECK(flow_file->getSize() == 11);
  ReadUntilItCan read_until_it_can_callback;
  const auto read_result = process_session.readBuffer(flow_file);
  process_session.read(flow_file, std::ref(read_until_it_can_callback));
  CHECK(to_string(read_result) == "new_content");
}

inline void testCancelWrite(std::shared_ptr<core::ContentRepository> content_repo) {
  const auto fixture = Fixture(std::move(content_repo));
  core::ProcessSession& process_session = fixture.processSession();
  const auto flow_file = process_session.create();
  fixture.writeToFlowFile(flow_file, "original_content");
  fixture.transferAndCommit(flow_file);

  process_session.write(flow_file, [](const std::shared_ptr<minifi::io::OutputStream>& output_stream) {
    output_stream->write("new_content");
    return MinifiIoStatus::MINIFI_IO_CANCEL;
  });

  CHECK(flow_file->getSize() == 16);
  ReadUntilItCan read_until_it_can_callback;
  const auto read_result = process_session.readBuffer(flow_file);
  process_session.read(flow_file, std::ref(read_until_it_can_callback));
  CHECK(to_string(read_result) == "original_content");
}
}  // namespace ContentRepositoryDependentTests
