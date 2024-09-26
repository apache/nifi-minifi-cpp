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

#include "controllers/RecordSetReader.h"
#include "controllers/RecordSetWriter.h"
#include "minifi-cpp/core/Record.h"
#include "TestBase.h"
#include "core/ProcessSession.h"

namespace org::apache::nifi::minifi::core::test {

class RecordSetFixture {
 public:
  explicit RecordSetFixture(TestController::PlanConfig config = {}): plan_config_(std::move(config)) {}

  [[nodiscard]] ProcessSession& processSession() const { return *process_session_; }

  [[nodiscard]] const Relationship& getRelationship() const { return relationship_; }
 private:
  TestController test_controller_{};
  TestController::PlanConfig plan_config_{};
  std::shared_ptr<TestPlan> test_plan_ = test_controller_.createPlan(plan_config_);
  std::shared_ptr<Processor> dummy_processor_ = test_plan_->addProcessor("DummyProcessor", "dummyProcessor");
  std::shared_ptr<ProcessContext> context_ = [this] { test_plan_->runNextProcessor(); return test_plan_->getCurrentContext(); }();
  std::unique_ptr<ProcessSession> process_session_ = std::make_unique<ProcessSessionImpl>(context_);

  const Relationship relationship_{"success", "description"};
};

bool testRecordWriter(RecordSetWriter& record_set_writer, const RecordSet& record_set, auto tester) {
  const RecordSetFixture fixture;
  ProcessSession& process_session = fixture.processSession();

  const auto flow_file = process_session.create();

  record_set_writer.write(record_set, flow_file, process_session);
  process_session.transfer(flow_file, fixture.getRelationship());
  process_session.commit();
  const auto input_stream = process_session.getFlowFileContentStream(*flow_file);
  std::array<std::byte, 2048> buffer{};
  const auto buffer_size = input_stream->read(buffer);
  const std::string flow_file_content(reinterpret_cast<char*>(buffer.data()), buffer_size);

  return tester(flow_file_content);
}

inline bool testRecordReader(RecordSetReader& record_set_reader, const std::string_view serialized_record_set, const RecordSet& expected_record_set) {
  const RecordSetFixture fixture;
  ProcessSession& process_session = fixture.processSession();

  const auto flow_file = process_session.create();
  process_session.writeBuffer(flow_file, serialized_record_set);
  process_session.transfer(flow_file, fixture.getRelationship());
  process_session.commit();

  const auto record_set = record_set_reader.read(flow_file, process_session);
  if (!record_set)
    return false;

  return *record_set == expected_record_set;
}

}  // namespace org::apache::nifi::minifi::core::test
