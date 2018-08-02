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

#include <uuid/uuid.h>
#include <fstream>
#include <map>
#include <memory>
#include <utility>
#include <string>
#include <set>
#include <iostream>

#include "../TestBase.h"
#include "core/Core.h"

#include "core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"

#include "ApplyTemplate.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "processors/ExtractText.h"

const char* TEMPLATE = "TemplateBegins\n{{ ExampleAttribute }}\nTemplateEnds";
const char* TEMPLATE_FILE = "test_template.txt";
const char* TEST_ATTR = "ExampleAttribute";
const char* TEST_VALUE = "ExampleValue";
const char* TEST_FILE = "test_file.txt";
const char* EXPECT_OUTPUT = "TemplateBegins\nExampleValue\nTemplateEnds";

TEST_CASE("Test Creation of ApplyTemplate", "[ApplyTemplateCreate]") {
    TestController testController;
    std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::ApplyTemplate>("processorname");
    REQUIRE(processor->getName() == "processorname");
    utils::Identifier processoruuid;
    REQUIRE(processor->getUUID(processoruuid));
}

TEST_CASE("Test usage of ApplyTemplate", "[ApplyTemplateTest]") {
    TestController testController;
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::ApplyTemplate>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::PutFile>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::GetFile>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::ExtractText>();
    LogTestController::getInstance().setTrace<core::ProcessSession>();
    LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::FlowFile>();

    std::shared_ptr<TestPlan> plan = testController.createPlan();
    std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

    char dir1[] = "/tmp/gt.XXXXXX";  // GetFile source
    char dir2[] = "/tmp/gt.XXXXXX";  // Template source
    char dir3[] = "/tmp/gt.XXXXXX";  // PutFile destionation

    REQUIRE(testController.createTempDirectory(dir1) != nullptr);
    REQUIRE(testController.createTempDirectory(dir2) != nullptr);
    REQUIRE(testController.createTempDirectory(dir3) != nullptr);

    std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getFile");
    plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir1);
    plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::KeepSourceFile.getName(), "true");

    std::shared_ptr<core::Processor> maprocessor = plan->addProcessor("ExtractText", "testExtractText", core::Relationship("success", "description"), true);
    plan->setProperty(maprocessor, org::apache::nifi::minifi::processors::ExtractText::Attribute.getName(), TEST_ATTR);

    std::shared_ptr<core::Processor> atprocessor = plan->addProcessor("ApplyTemplate", "testApplyTemplate", core::Relationship("success", "description"), true);

    std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);
    plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir3);
    plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::ConflictResolution.getName(),
                      org::apache::nifi::minifi::processors::PutFile::CONFLICT_RESOLUTION_STRATEGY_REPLACE);

    // Write attribute value to file for GetFile->ExtractText
    std::stringstream ss1;
    ss1 << dir1 << "/" << TEST_FILE;
    std::string test_path = ss1.str();

    std::ofstream test_file(test_path);
    REQUIRE(test_file.is_open());
    test_file << TEST_VALUE;
    test_file.close();

    // Write template to file
    std::stringstream ss2;
    ss2 << dir2 << "/" << TEMPLATE_FILE;
    std::string template_path = ss2.str();

    std::ofstream template_file(template_path);
    REQUIRE(template_file.is_open());
    template_file << TEMPLATE;
    template_file.close();

    plan->setProperty(atprocessor, org::apache::nifi::minifi::processors::ApplyTemplate::Template.getName(), template_path);

    // Run processor chain
    plan->runNextProcessor();  // GetFile
    plan->runNextProcessor();  // ExtractText
    plan->runNextProcessor();  // ApplyTemplate
    plan->runNextProcessor();  // PutFile

    // Read contents of file
    std::stringstream ss3;
    ss3 << dir3 << "/" << TEST_FILE;
    std::string output_path = ss3.str();

    std::ifstream output_file(output_path);
    std::stringstream output_buf;
    output_buf << output_file.rdbuf();
    std::string output_contents = output_buf.str();
    REQUIRE(output_contents == EXPECT_OUTPUT);
}
