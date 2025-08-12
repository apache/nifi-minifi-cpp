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

#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "../controllers/SparkplugBWriter.h"
#include "io/BufferStream.h"
#include "sparkplug_b.pb.h"
#include "core/ProcessSession.h"

namespace org::apache::nifi::minifi::test {

class SparkplugBWriterTestFixture {
 public:
  const core::Relationship Success{"success", "everything is fine"};
  const core::Relationship Failure{"failure", "something has gone awry"};

  SparkplugBWriterTestFixture() {
    test_plan_ = test_controller_.createPlan();
    dummy_processor_ = test_plan_->addProcessor("DummyProcessor", "dummyProcessor");
    context_ = [this] {
      test_plan_->runNextProcessor();
      return test_plan_->getCurrentContext();
    }();
    process_session_ = std::make_unique<core::ProcessSessionImpl>(context_);
  }

  core::ProcessSession &processSession() { return *process_session_; }

  void transferAndCommit(const std::shared_ptr<core::FlowFile>& flow_file) {
    process_session_->transfer(flow_file, Success);
    process_session_->commit();
  }

 private:
  TestController test_controller_;

  std::shared_ptr<TestPlan> test_plan_;
  core::Processor* dummy_processor_;
  std::shared_ptr<core::ProcessContext> context_;
  std::unique_ptr<core::ProcessSession> process_session_;
};

TEST_CASE_METHOD(SparkplugBWriterTestFixture, "Test empty record set", "[SparkplugBWriter]") {
  core::RecordSet record_set;

  record_set.emplace_back(core::RecordObject{});
  auto flow_file = processSession().create();

  controllers::SparkplugBWriter sparkplug_writer("SparkplugBWriter");
  sparkplug_writer.write(record_set, flow_file, processSession());
  transferAndCommit(flow_file);
  org::eclipse::tahu::protobuf::Payload payload;
  processSession().read(*flow_file, [&payload](const std::shared_ptr<io::InputStream>& input_stream) {
    std::vector<std::byte> buffer(input_stream->size());
    input_stream->read(buffer);
    payload.ParseFromArray(static_cast<void*>(buffer.data()), gsl::narrow<int>(input_stream->size()));
    return gsl::narrow<int64_t>(input_stream->size());
  });

  CHECK_FALSE(payload.has_timestamp());
  CHECK_FALSE(payload.has_seq());
  CHECK_FALSE(payload.has_uuid());
  CHECK_FALSE(payload.has_body());
  CHECK(payload.metrics_size() == 0);
}

TEST_CASE_METHOD(SparkplugBWriterTestFixture, "Test invalid type set", "[SparkplugBWriter]") {
  core::RecordSet record_set;
  core::RecordObject payload_record;
  payload_record.emplace("timestamp", core::RecordField{std::string("invalid_timestamp")});
  record_set.emplace_back(std::move(payload_record));

  auto flow_file = processSession().create();

  controllers::SparkplugBWriter sparkplug_writer("SparkplugBWriter");
  sparkplug_writer.write(record_set, flow_file, processSession());
  transferAndCommit(flow_file);
  org::eclipse::tahu::protobuf::Payload payload;
  processSession().read(*flow_file, [&payload](const std::shared_ptr<io::InputStream>& input_stream) {
    std::vector<std::byte> buffer(input_stream->size());
    input_stream->read(buffer);
    payload.ParseFromArray(static_cast<void*>(buffer.data()), gsl::narrow<int>(input_stream->size()));
    return gsl::narrow<int64_t>(input_stream->size());
  });

  CHECK_FALSE(payload.has_timestamp());
  CHECK_FALSE(payload.has_seq());
  CHECK_FALSE(payload.has_uuid());
  CHECK_FALSE(payload.has_body());
  CHECK(payload.metrics_size() == 0);
  CHECK(LogTestController::getInstance().contains("Failed to write record to Sparkplug B payload: Invalid type for integral conversion"));
}

TEST_CASE_METHOD(SparkplugBWriterTestFixture, "Test record set conversion to Sparkplug B payload", "[SparkplugBWriter]") {
  core::RecordSet record_set;
  core::RecordObject payload_record;
  payload_record.emplace("timestamp", core::RecordField{static_cast<uint64_t>(1234)});
  payload_record.emplace("seq", core::RecordField{static_cast<uint64_t>(456)});
  payload_record.emplace("uuid", core::RecordField{std::string("my-uuid")});
  payload_record.emplace("body", core::RecordField{std::string("testbody")});
  core::RecordObject metric_object;
  metric_object.emplace("name", core::RecordField{std::string("test_metric")});
  metric_object.emplace("alias", core::RecordField{static_cast<uint64_t>(1)});
  metric_object.emplace("timestamp", core::RecordField{static_cast<uint64_t>(789)});
  metric_object.emplace("datatype", core::RecordField{static_cast<uint32_t>(2)});
  metric_object.emplace("is_historical", core::RecordField{false});
  metric_object.emplace("is_transient", core::RecordField{false});
  metric_object.emplace("is_null", core::RecordField{false});

  core::RecordObject metadata_object;
  metadata_object.emplace("is_multi_part", core::RecordField{false});
  metadata_object.emplace("content_type", core::RecordField{std::string("application/json")});
  metadata_object.emplace("size", core::RecordField{static_cast<uint64_t>(1024)});
  metadata_object.emplace("seq", core::RecordField{static_cast<uint64_t>(1)});
  metadata_object.emplace("file_name", core::RecordField{std::string("example.json")});
  metadata_object.emplace("file_type", core::RecordField{std::string("json")});
  metadata_object.emplace("md5", core::RecordField{std::string("d41d8cd98f00b204e9800998ecf8427e")});
  metadata_object.emplace("description", core::RecordField{std::string("Example metadata description")});
  metric_object.emplace("metadata", core::RecordField{std::move(metadata_object)});

  core::RecordObject propertyset_object;
  core::RecordArray keys_array;
  keys_array.emplace_back(core::RecordField{std::string("key1")});
  keys_array.emplace_back(core::RecordField{std::string("key2")});
  core::RecordArray propertyvalues_array;
  core::RecordObject propertyvalue1;
  propertyvalue1.emplace("type", core::RecordField{static_cast<uint32_t>(1)});
  propertyvalue1.emplace("is_null", core::RecordField{false});
  propertyvalue1.emplace("int_value", core::RecordField{static_cast<uint32_t>(42)});
  core::RecordObject propertyvalue2;
  propertyvalue2.emplace("type", core::RecordField{static_cast<uint32_t>(1)});
  propertyvalue2.emplace("is_null", core::RecordField{false});
  propertyvalue2.emplace("int_value", core::RecordField{static_cast<uint32_t>(43)});
  propertyvalues_array.emplace_back(core::RecordField{std::move(propertyvalue1)});
  propertyvalues_array.emplace_back(core::RecordField{std::move(propertyvalue2)});
  propertyset_object.emplace("keys", core::RecordField{std::move(keys_array)});
  propertyset_object.emplace("values", core::RecordField{std::move(propertyvalues_array)});
  metric_object.emplace("properties", core::RecordField{std::move(propertyset_object)});

  core::RecordObject dataset_object;

  dataset_object.emplace("num_of_columns", core::RecordField{static_cast<uint64_t>(1)});
  core::RecordArray columns_array;
  columns_array.emplace_back(core::RecordField{std::string("column1")});
  dataset_object.emplace("columns", core::RecordField{std::move(columns_array)});
  core::RecordArray types_array;
  types_array.emplace_back(core::RecordField{static_cast<uint32_t>(1)});
  dataset_object.emplace("types", core::RecordField{std::move(types_array)});
  core::RecordArray rows_array;
  core::RecordObject row_object;
  core::RecordArray elements_array;
  core::RecordObject element_object;
  element_object.emplace("int_value", core::RecordField{static_cast<uint32_t>(100)});
  elements_array.emplace_back(core::RecordField{std::move(element_object)});
  row_object.emplace("elements", core::RecordField{std::move(elements_array)});
  rows_array.emplace_back(core::RecordField{std::move(row_object)});
  dataset_object.emplace("rows", core::RecordField{std::move(rows_array)});
  metric_object.emplace("dataset_value", core::RecordField{std::move(dataset_object)});

  core::RecordArray metric_array;
  core::RecordField metric_field{std::move(metric_object)};
  metric_array.emplace_back(std::move(metric_field));
  payload_record.emplace("metrics", core::RecordField{std::move(metric_array)});
  record_set.emplace_back(std::move(payload_record));

  record_set.emplace_back(core::RecordObject{});
  auto flow_file = processSession().create();

  controllers::SparkplugBWriter sparkplug_writer("SparkplugBWriter");
  sparkplug_writer.write(record_set, flow_file, processSession());
  transferAndCommit(flow_file);
  org::eclipse::tahu::protobuf::Payload payload;
  processSession().read(*flow_file, [&payload](const std::shared_ptr<io::InputStream>& input_stream) {
    std::vector<std::byte> buffer(input_stream->size());
    input_stream->read(buffer);
    payload.ParseFromArray(static_cast<void*>(buffer.data()), gsl::narrow<int>(input_stream->size()));
    return gsl::narrow<int64_t>(input_stream->size());
  });

  CHECK(payload.timestamp() == 1234);
  CHECK(payload.seq() == 456);
  CHECK(payload.uuid() == "my-uuid");
  CHECK(payload.body() == "testbody");
  CHECK(payload.metrics_size() == 1);
  const auto& metric = payload.metrics(0);
  CHECK(metric.name() == "test_metric");
  CHECK(metric.alias() == 1);
  CHECK(metric.timestamp() == 789);
  CHECK(metric.datatype() == 2);
  CHECK_FALSE(metric.is_historical());
  CHECK_FALSE(metric.is_transient());
  CHECK_FALSE(metric.is_null());
  CHECK(metric.has_metadata());
  CHECK(metric.metadata().is_multi_part() == false);
  CHECK(metric.metadata().content_type() == "application/json");
  CHECK(metric.metadata().size() == 1024);
  CHECK(metric.metadata().seq() == 1);
  CHECK(metric.metadata().file_name() == "example.json");
  CHECK(metric.metadata().file_type() == "json");
  CHECK(metric.metadata().md5() == "d41d8cd98f00b204e9800998ecf8427e");
  CHECK(metric.metadata().description() == "Example metadata description");

  CHECK(metric.has_properties());
  const auto& properties = metric.properties();
  CHECK(properties.keys_size() == 2);
  CHECK(properties.keys(0) == "key1");
  CHECK(properties.keys(1) == "key2");
  CHECK(properties.values_size() == 2);
  const auto& property1 = properties.values(0);
  CHECK(property1.type() == 1);
  CHECK_FALSE(property1.is_null());
  CHECK(property1.int_value() == 42);
  const auto& property2 = properties.values(1);
  CHECK(property2.type() == 1);
  CHECK_FALSE(property2.is_null());
  CHECK(property2.int_value() == 43);

  CHECK(metric.has_dataset_value());
  const auto& dataset = metric.dataset_value();
  CHECK(dataset.num_of_columns() == 1);
  CHECK(dataset.columns_size() == 1);
  CHECK(dataset.columns(0) == "column1");
  CHECK(dataset.types_size() == 1);
  CHECK(dataset.types(0) == 1);
  CHECK(dataset.rows_size() == 1);
  const auto& row = dataset.rows(0);
  CHECK(row.elements_size() == 1);
  const auto& element = row.elements(0);
  CHECK(element.has_int_value());
  CHECK(element.int_value() == 100);
  CHECK_FALSE(element.has_long_value());
}

}  // namespace org::apache::nifi::minifi::test
