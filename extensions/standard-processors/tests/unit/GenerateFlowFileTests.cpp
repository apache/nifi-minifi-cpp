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
#include <set>

#include "unit/TestBase.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/Catch.h"
#include "GenerateFlowFile.h"

using minifi::processors::GenerateFlowFile;

TEST_CASE("GenerateFlowFileWithBinaryData") {
  std::optional<bool> is_unique;

  SECTION("Not unique") {
    is_unique = false;
  }

  SECTION("Unique") {
    is_unique = true;
  }


  minifi::test::SingleProcessorTestController test_controller{std::make_unique<GenerateFlowFile>("GenerateFlowFile")};
  auto generate_flow_file = test_controller.getProcessor();
  LogTestController::getInstance().setWarn<GenerateFlowFile>();

  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::FileSize, "10"));
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::BatchSize, "2"));

  // This property will be ignored if binary files are used
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::CustomText, "Current time: ${now()}"));

  REQUIRE(is_unique.has_value());
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::UniqueFlowFiles, fmt::format("{}", *is_unique)));

  auto first_batch = test_controller.trigger();
  REQUIRE(first_batch.at(GenerateFlowFile::Success).size() == 2);
  auto first_batch_0 = test_controller.plan->getContent(first_batch.at(GenerateFlowFile::Success)[0]);
  auto first_batch_1 = test_controller.plan->getContent(first_batch.at(GenerateFlowFile::Success)[1]);
  CHECK(first_batch_0.size() == 10);

  if (*is_unique) {
    CHECK(first_batch_0 != first_batch_1);
  } else {
    CHECK(first_batch_0 == first_batch_1);
  }

  CHECK(LogTestController::getInstance().contains("Custom Text property is set but not used. For Custom Text to be used, Data Format needs to be Text, and Unique FlowFiles needs to be false."));
}

TEST_CASE("GenerateFlowFileTestEmpty") {
  minifi::test::SingleProcessorTestController test_controller{std::make_unique<GenerateFlowFile>("GenerateFlowFile")};
  auto generate_flow_file = test_controller.getProcessor();

  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::FileSize, "0"));
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::UniqueFlowFiles, "false"));
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::DataFormat, "Text"));

  auto result = test_controller.trigger();
  REQUIRE(result.at(GenerateFlowFile::Success).size() == 1);
  auto result_0 = test_controller.plan->getContent(result.at(GenerateFlowFile::Success)[0]);
  CHECK(result_0.empty());
}

TEST_CASE("GenerateFlowFileCustomTextTest") {
  minifi::test::SingleProcessorTestController test_controller{std::make_unique<GenerateFlowFile>("GenerateFlowFile")};
  auto generate_flow_file = test_controller.getProcessor();

  constexpr auto uuid_string_length = 36;

  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::CustomText, "${UUID()}"));
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::UniqueFlowFiles, "false"));
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::DataFormat, "Text"));

  auto result = test_controller.trigger();
  REQUIRE(result.at(GenerateFlowFile::Success).size() == 1);
  auto result_0 = test_controller.plan->getContent(result.at(GenerateFlowFile::Success)[0]);
  CHECK(result_0.length() == uuid_string_length);
}

TEST_CASE("GenerateFlowFileCustomTextEmptyTest") {
  minifi::test::SingleProcessorTestController test_controller{std::make_unique<GenerateFlowFile>("GenerateFlowFile")};
  auto generate_flow_file = test_controller.getProcessor();

  constexpr int32_t file_size = 10;

  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::FileSize, std::to_string(file_size)));
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::UniqueFlowFiles, "false"));
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::DataFormat, "Text"));
  SECTION("Empty custom data") {
    CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::CustomText, ""));
  }

  SECTION("No custom data") {
  }

  auto result = test_controller.trigger();
  REQUIRE(result.at(GenerateFlowFile::Success).size() == 1);
  auto result_0 = test_controller.plan->getContent(result.at(GenerateFlowFile::Success)[0]);
  CHECK(result_0.length() == file_size);
}

TEST_CASE("GenerateFlowFile reevaluating CustomText") {
  minifi::test::SingleProcessorTestController test_controller{std::make_unique<GenerateFlowFile>("GenerateFlowFile")};
  auto generate_flow_file = test_controller.getProcessor();
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::DataFormat, "Text"));
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::UniqueFlowFiles, "false"));
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::CustomText, "${nextInt()}"));
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::BatchSize, "2"));

  for (auto i = 0; i < 100; ++i) {
    auto batch = test_controller.trigger();
    auto batch_0 = test_controller.plan->getContent(batch.at(GenerateFlowFile::Success)[0]);
    auto batch_1 = test_controller.plan->getContent(batch.at(GenerateFlowFile::Success)[1]);
    CHECK(batch_0 == batch_1);
    CHECK(batch_0 == std::to_string(i));
  }
}

TEST_CASE("GenerateFlowFile CustomText evaluates to empty string") {
  minifi::test::SingleProcessorTestController test_controller{std::make_unique<GenerateFlowFile>("GenerateFlowFile")};
  auto generate_flow_file = test_controller.getProcessor();
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::DataFormat, "Text"));
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::UniqueFlowFiles, "false"));
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::CustomText, "${invalid_variable}"));
  CHECK(test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::BatchSize, "2"));

  auto batch = test_controller.trigger();
  auto batch_0 = test_controller.plan->getContent(batch.at(GenerateFlowFile::Success)[0]);
  auto batch_1 = test_controller.plan->getContent(batch.at(GenerateFlowFile::Success)[1]);
  CHECK(batch_0 == batch_1);
  CHECK(batch_0.empty());
}
