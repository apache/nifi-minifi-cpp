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
#include "../controllers/XMLReader.h"
#include "../controllers/JsonRecordSetWriter.h"
#include "unit/SingleProcessorTestController.h"
#include "../processors/ConvertRecord.h"
#include "utils/StringUtils.h"
#include "unit/ProcessorUtils.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("ConvertRecord scheduling fails with invalid reader and writer", "[ConvertRecord]") {
  LogTestController::getInstance().setTrace<processors::ConvertRecord>();
  SingleProcessorTestController controller(utils::make_processor<processors::ConvertRecord>("ConvertRecord"));
  controller.plan->addController("XMLReader", "XMLReader");

  REQUIRE_THROWS_WITH(controller.trigger(minifi::test::InputFlowFileData{""}), "Process Schedule Operation: Required controller service property 'Record Reader' is missing");

  REQUIRE(controller.getProcessor()->setProperty(processors::ConvertRecord::RecordReader.name, "XMLReader"));
  REQUIRE_THROWS_WITH(controller.trigger(minifi::test::InputFlowFileData{""}), "Process Schedule Operation: Required controller service property 'Record Writer' is missing");
  REQUIRE(controller.getProcessor()->setProperty(processors::ConvertRecord::RecordWriter.name, "XMLWriter"));
  REQUIRE_THROWS_WITH(controller.trigger(minifi::test::InputFlowFileData{""}), "Process Schedule Operation: Controller service 'Record Writer' = 'XMLWriter' not found");
}

TEST_CASE("Record conversion fails with read failure", "[ConvertRecord]") {
  LogTestController::getInstance().setTrace<processors::ConvertRecord>();
  SingleProcessorTestController controller(utils::make_processor<processors::ConvertRecord>("ConvertRecord"));
  controller.plan->addController("XMLReader", "XMLReader");
  controller.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  REQUIRE(controller.getProcessor()->setProperty(processors::ConvertRecord::RecordReader.name, "XMLReader"));
  REQUIRE(controller.getProcessor()->setProperty(processors::ConvertRecord::RecordWriter.name, "JsonRecordSetWriter"));

  auto results = controller.trigger(minifi::test::InputFlowFileData{"<invalidxml>"});
  REQUIRE(results.at(processors::ConvertRecord::Failure).size() == 1);
  auto& output_flow_file = results.at(processors::ConvertRecord::Failure)[0];
  CHECK(controller.plan->getContent(output_flow_file) == "<invalidxml>");
  auto error_message_attribute = minifi::utils::string::toLower(*output_flow_file->getAttribute(processors::ConvertRecord::RecordErrorMessageOutputAttribute.name));
  CHECK(error_message_attribute == "invalid argument");
  CHECK(LogTestController::getInstance().contains("Failed to read record set from flow file"));
}

TEST_CASE("Record conversion succeeds with a single record", "[ConvertRecord]") {
  SingleProcessorTestController controller(utils::make_processor<processors::ConvertRecord>("ConvertRecord"));
  controller.plan->addController("XMLReader", "XMLReader");
  controller.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  REQUIRE(controller.getProcessor()->setProperty(processors::ConvertRecord::RecordReader.name, "XMLReader"));
  REQUIRE(controller.getProcessor()->setProperty(processors::ConvertRecord::RecordWriter.name, "JsonRecordSetWriter"));

  auto results = controller.trigger(minifi::test::InputFlowFileData{"<record><field>value</field></record>"});
  REQUIRE(results.at(processors::ConvertRecord::Success).size() == 1);
  auto& output_flow_file = results.at(processors::ConvertRecord::Success)[0];
  CHECK(*output_flow_file->getAttribute(processors::ConvertRecord::RecordCountOutputAttribute.name) == "1");
  CHECK(controller.plan->getContent(output_flow_file) == R"([{"field":"value"}])");
}

TEST_CASE("Empty flow files are not transferred when Include Zero Record Flow Files is false", "[ConvertRecord]") {
  SingleProcessorTestController controller(utils::make_processor<processors::ConvertRecord>("ConvertRecord"));
  controller.plan->addController("XMLReader", "XMLReader");
  controller.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  REQUIRE(controller.getProcessor()->setProperty(processors::ConvertRecord::RecordReader.name, "XMLReader"));
  REQUIRE(controller.getProcessor()->setProperty(processors::ConvertRecord::RecordWriter.name, "JsonRecordSetWriter"));
  REQUIRE(controller.getProcessor()->setProperty(processors::ConvertRecord::IncludeZeroRecordFlowFiles.name, "false"));

  auto results = controller.trigger(minifi::test::InputFlowFileData{"<record></record>"});
  REQUIRE(results.at(processors::ConvertRecord::Success).empty());
  REQUIRE(results.at(processors::ConvertRecord::Failure).empty());
}

TEST_CASE("Empty flow files are transferred when Include Zero Record Flow Files is true", "[ConvertRecord]") {
  SingleProcessorTestController controller(utils::make_processor<processors::ConvertRecord>("ConvertRecord"));
  controller.plan->addController("XMLReader", "XMLReader");
  controller.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
  REQUIRE(controller.getProcessor()->setProperty(processors::ConvertRecord::RecordReader.name, "XMLReader"));
  REQUIRE(controller.getProcessor()->setProperty(processors::ConvertRecord::RecordWriter.name, "JsonRecordSetWriter"));
  REQUIRE(controller.getProcessor()->setProperty(processors::ConvertRecord::IncludeZeroRecordFlowFiles.name, "true"));

  auto results = controller.trigger(minifi::test::InputFlowFileData{"<record></record>"});
  REQUIRE(results.at(processors::ConvertRecord::Success).size() == 1);
  REQUIRE(results.at(processors::ConvertRecord::Failure).empty());
  auto& output_flow_file = results.at(processors::ConvertRecord::Success)[0];
  CHECK(*output_flow_file->getAttribute(processors::ConvertRecord::RecordCountOutputAttribute.name) == "0");
  CHECK(controller.plan->getContent(output_flow_file) == "[]");
}

}  // namespace org::apache::nifi::minifi::test
