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

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "processors/SplitRecord.h"
#include "unit/SingleProcessorTestController.h"
#include "utils/StringUtils.h"
#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::test {

class SplitRecordTestController : public TestController {
 public:
  SplitRecordTestController() {
    controller_.plan->addController("JsonRecordSetReader", "JsonRecordSetReader");
    controller_.plan->addController("JsonRecordSetWriter", "JsonRecordSetWriter");
    REQUIRE(controller_.getProcessor());
    controller_.getProcessor()->setProperty(processors::SplitRecord::RecordReader.name, "JsonRecordSetReader");
    controller_.getProcessor()->setProperty(processors::SplitRecord::RecordWriter.name, "JsonRecordSetWriter");
  }

  void verifyResults(const ProcessorTriggerResult& results, const std::vector<std::string>& expected_contents) const {
    REQUIRE(results.at(processors::SplitRecord::Original).size() == 1);
    REQUIRE(results.at(processors::SplitRecord::Splits).size() == expected_contents.size());
    auto& split_results = results.at(processors::SplitRecord::Splits);
    const auto fragment_identifier = split_results[0]->getAttribute("fragment.identifier").value();
    const auto original_filename = results.at(processors::SplitRecord::Original)[0]->getAttribute("filename").value();
    for (size_t i = 0; i < expected_contents.size(); ++i) {
      rapidjson::Document result_document;
      result_document.Parse(controller_.plan->getContent(split_results[i]).c_str());
      rapidjson::Document expected_document;
      expected_document.Parse(expected_contents[i].c_str());
      CHECK(result_document == expected_document);
      CHECK(split_results[i]->getAttribute("record.count").value() ==   std::to_string(minifi::utils::string::split(expected_contents[i], "},{").size()));
      CHECK(split_results[i]->getAttribute("fragment.index").value() == std::to_string(i));
      CHECK(split_results[i]->getAttribute("fragment.count").value() == std::to_string(expected_contents.size()));
      CHECK(split_results[i]->getAttribute("fragment.identifier").value() == fragment_identifier);
      CHECK(split_results[i]->getAttribute("segment.original.filename").value() == original_filename);
    }
  }

 protected:
  SingleProcessorTestController controller_{std::make_unique<processors::SplitRecord>("SplitRecord")};
};

TEST_CASE_METHOD(SplitRecordTestController, "Invalid Records Per Split property", "[splitrecord]") {
  controller_.getProcessor()->setProperty(processors::SplitRecord::RecordsPerSplit.name, "invalid");
  auto results = controller_.trigger({InputFlowFileData{"{\"name\": \"John\"}\n{\"name\": \"Jill\"}"}});
  REQUIRE(results[processors::SplitRecord::Failure].size() == 1);
  REQUIRE(LogTestController::getInstance().contains("Records Per Split should be set to a number larger than 0", 1s));
}

TEST_CASE_METHOD(SplitRecordTestController, "Records Per Split property should be greater than zero", "[splitrecord]") {
  controller_.getProcessor()->setProperty(processors::SplitRecord::RecordsPerSplit.name, "${id}");
  auto results = controller_.trigger({InputFlowFileData{"{\"name\": \"John\"}\n{\"name\": \"Jill\"}", {{"id", "0"}}}});
  REQUIRE(results[processors::SplitRecord::Failure].size() == 1);
  REQUIRE(LogTestController::getInstance().contains("Records Per Split should be set to a number larger than 0", 1s));
}

TEST_CASE_METHOD(SplitRecordTestController, "Invalid records in flow file result in zero splits", "[splitrecord]") {
  controller_.getProcessor()->setProperty(processors::SplitRecord::RecordsPerSplit.name, "1");
  auto results = controller_.trigger({InputFlowFileData{ R"({"name": "John)"}});
  CHECK(results[processors::SplitRecord::Splits].empty());
  REQUIRE(results[processors::SplitRecord::Original].size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::SplitRecord::Original)[0]) == "{\"name\": \"John");
}

TEST_CASE_METHOD(SplitRecordTestController, "Split records one by one", "[splitrecord]") {
  controller_.getProcessor()->setProperty(processors::SplitRecord::RecordsPerSplit.name, "1");
  auto results = controller_.trigger({InputFlowFileData{"{\"name\": \"John\"}\n{\"name\": \"Jill\"}"}});
  verifyResults(results, {R"([{"name":"John"}])", R"([{"name":"Jill"}])"});
}

TEST_CASE_METHOD(SplitRecordTestController, "Split records two by two", "[splitrecord]") {
  controller_.getProcessor()->setProperty(processors::SplitRecord::RecordsPerSplit.name, "2");
  auto results = controller_.trigger({InputFlowFileData{"{\"a\": \"1\", \"b\": \"2\"}\n{\"c\": \"3\"}\n{\"d\": \"4\", \"e\": \"5\"}\n{\"f\": \"6\"}\n{\"g\": \"7\", \"h\": \"8\"}\n"}});
  verifyResults(results, {R"([{"a":"1","b":"2"},{"c":"3"}])", R"([{"d":"4","e":"5"},{"f":"6"}])", R"([{"g":"7","h":"8"}])"});
}

}  // namespace org::apache::nifi::minifi::test
