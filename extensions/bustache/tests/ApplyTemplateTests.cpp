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

#include <fstream>
#include <memory>
#include <string>
#include <iostream>

#include "unit/TestBase.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"
#include "core/Core.h"

#include "minifi-cpp/core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "repository/VolatileContentRepository.h"
#include "unit/ProvenanceTestHelper.h"

#include "ApplyTemplate.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "processors/ExtractText.h"

namespace org::apache::nifi::minifi::processors::test {

const char* TEMPLATE = "TemplateBegins\n{{ ExampleAttribute }}\nTemplateEnds";
const char* TEMPLATE_FILE = "test_template.txt";
const char* TEST_ATTR = "ExampleAttribute";
const char* TEST_VALUE = "ExampleValue";
const char* TEST_FILE = "test_file.txt";
const char* EXPECT_OUTPUT = "TemplateBegins\nExampleValue\nTemplateEnds";

TEST_CASE("Test Creation of ApplyTemplate", "[ApplyTemplateCreate]") {
  TestController testController;
  std::shared_ptr<core::Processor> processor = minifi::test::utils::make_processor<org::apache::nifi::minifi::processors::ApplyTemplate>("processor_name");
  REQUIRE(processor->getName() == "processor_name");
  REQUIRE(processor->getUUID());
}

TEST_CASE("Test usage of ApplyTemplate", "[ApplyTemplateTest]") {
  TestController testController;
  LogTestController::getInstance().setTrace<ApplyTemplate>();
  LogTestController::getInstance().setTrace<PutFile>();
  LogTestController::getInstance().setTrace<GetFile>();
  LogTestController::getInstance().setTrace<ExtractText>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  LogTestController::getInstance().setTrace<Connection>();
  LogTestController::getInstance().setTrace<core::Connectable>();
  LogTestController::getInstance().setTrace<core::FlowFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  auto get_file_source_dir = testController.createTempDirectory();
  auto template_source_dir = testController.createTempDirectory();
  auto put_file_destination_dir = testController.createTempDirectory();

  REQUIRE_FALSE(get_file_source_dir.empty());
  REQUIRE_FALSE(template_source_dir.empty());
  REQUIRE_FALSE(put_file_destination_dir.empty());

  auto getfile = plan->addProcessor("GetFile", "getFile");
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, get_file_source_dir.string());
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::KeepSourceFile, "true");

  auto extract_text = plan->addProcessor("ExtractText", "testExtractText", core::Relationship("success", "description"), true);
  plan->setProperty(extract_text, org::apache::nifi::minifi::processors::ExtractText::Attribute, TEST_ATTR);

  auto apply_template = plan->addProcessor("ApplyTemplate", "testApplyTemplate", core::Relationship("success", "description"), true);

  auto put_file = plan->addProcessor("PutFile", "put_file", core::Relationship("success", "description"), true);
  plan->setProperty(put_file, org::apache::nifi::minifi::processors::PutFile::Directory, put_file_destination_dir.string());
  plan->setProperty(put_file, org::apache::nifi::minifi::processors::PutFile::ConflictResolution, magic_enum::enum_name(minifi::processors::PutFile::FileExistsResolutionStrategy::replace));

  // Write attribute value to file for GetFile->ExtractText

  std::ofstream test_file(get_file_source_dir / TEST_FILE);
  REQUIRE(test_file.is_open());
  test_file << TEST_VALUE;
  test_file.close();

  // Write template to file
  auto template_path = template_source_dir / TEMPLATE_FILE;
  std::ofstream template_file(template_path);
  REQUIRE(template_file.is_open());
  template_file << TEMPLATE;
  template_file.close();

  plan->setProperty(apply_template, org::apache::nifi::minifi::processors::ApplyTemplate::Template, template_path.string());

  // Run processor chain
  plan->runNextProcessor();  // GetFile
  plan->runNextProcessor();  // ExtractText
  plan->runNextProcessor();  // ApplyTemplate
  plan->runNextProcessor();  // PutFile

  // Read contents of file
  std::ifstream output_file(put_file_destination_dir / TEST_FILE);
  std::stringstream output_buf;
  output_buf << output_file.rdbuf();
  std::string output_contents = output_buf.str();
  REQUIRE(output_contents == EXPECT_OUTPUT);
}

}  // namespace org::apache::nifi::minifi::processors::test
