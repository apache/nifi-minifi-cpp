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

#include "TestBase.h"
#include "WriteToFlowFileTestProcessor.h"
#include "ReadFromFlowFileTestProcessor.h"
#include "UpdateAttribute.h"
#include "DefragmentText.h"
#include "TextFragmentUtils.h"
#include "utils/TestUtils.h"
#include "serialization/PayloadSerializer.h"
#include "serialization/FlowFileSerializer.h"
#include "unit/ContentRepositoryDependentTests.h"

using WriteToFlowFileTestProcessor = org::apache::nifi::minifi::processors::WriteToFlowFileTestProcessor;
using ReadFromFlowFileTestProcessor = org::apache::nifi::minifi::processors::ReadFromFlowFileTestProcessor;
using UpdateAttribute = org::apache::nifi::minifi::processors::UpdateAttribute;
using DefragmentText = org::apache::nifi::minifi::processors::DefragmentText;

TEST_CASE("DefragmentText Single source tests", "[defragmenttextsinglesource]") {
  TestController testController;
  auto plan = testController.createPlan();
  auto write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  auto defrag_text_flow_files = std::dynamic_pointer_cast<DefragmentText>(plan->addProcessor("DefragmentText", "defrag_text_flow_files"));
  auto read_from_success_relationship = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_success_relationship"));
  auto read_from_failure_relationship = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_failure_relationship"));

  plan->addConnection(write_to_flow_file, WriteToFlowFileTestProcessor::Success, defrag_text_flow_files);

  plan->addConnection(defrag_text_flow_files, DefragmentText::Success, read_from_success_relationship);
  plan->addConnection(defrag_text_flow_files, DefragmentText::Failure, read_from_failure_relationship);

  read_from_success_relationship->setAutoTerminatedRelationships({ReadFromFlowFileTestProcessor::Success});
  read_from_failure_relationship->setAutoTerminatedRelationships({ReadFromFlowFileTestProcessor::Success});


  SECTION("Throws on empty pattern") {
    REQUIRE_THROWS(testController.runSession(plan));
  }

  SECTION("Throws on invalid pattern") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "\"[a-b][a\"");

    REQUIRE_THROWS(testController.runSession(plan));
  }

  SECTION("Single line messages starting with pattern") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc.getName(), toString(DefragmentText::PatternLocation::START_OF_MESSAGE));

    write_to_flow_file->setContent("<1> Foo");
    testController.runSession(plan);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 0);
    write_to_flow_file->setContent("<2> Bar");
    plan->reset();
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent("<1> Foo"));
    write_to_flow_file->setContent("<3> Baz");
    plan->reset();
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent("<2> Bar"));
  }

  SECTION("Single line messages ending with pattern") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc.getName(), toString(DefragmentText::PatternLocation::END_OF_MESSAGE));

    write_to_flow_file->setContent("Foo <1>");
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent("Foo <1>"));
    write_to_flow_file->setContent("Bar <2>");
    plan->reset();
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent("Bar <2>"));
    write_to_flow_file->setContent("Baz <3>");
    plan->reset();
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent("Baz <3>"));
  }

  SECTION("Multiline matching start of messages") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc.getName(), toString(DefragmentText::PatternLocation::START_OF_MESSAGE));

    write_to_flow_file->setContent("apple<1> banana<2> cherry<3> dragon ");
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent("apple<1> banana<2> cherry"));

    write_to_flow_file->setContent("fruit<4> elderberry<5> fig<6> grapefruit");
    plan->reset();
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent("<3> dragon fruit<4> elderberry<5> fig"));
  }

  SECTION("Multiline matching end of messages") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc.getName(), toString(DefragmentText::PatternLocation::END_OF_MESSAGE));

    write_to_flow_file->setContent("apple<1> banana<2> cherry<3> dragon ");
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent("apple<1> banana<2> cherry<3>"));

    write_to_flow_file->setContent("fruit<4> elderberry<5> fig<6> grapefruit");
    plan->reset();
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent(" dragon fruit<4> elderberry<5> fig<6>"));
  }

  SECTION("Timeout test Start of Line") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferAge.getName(), "100 ms");

    write_to_flow_file->setContent("Message");
    testController.runSession(plan);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 0);

    plan->reset();
    write_to_flow_file->setContent("");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent("Message"));
  }

  SECTION("Timeout test Start of Line") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc.getName(), toString(DefragmentText::PatternLocation::START_OF_MESSAGE));
    plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferAge.getName(), "100 ms");

    write_to_flow_file->setContent("Message");
    testController.runSession(plan);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 0);

    plan->reset();
    write_to_flow_file->setContent("");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent("Message"));
  }

  SECTION("Timeout test Start of Line") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc.getName(), toString(DefragmentText::PatternLocation::END_OF_MESSAGE));
    plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferAge.getName(), "100 ms");

    write_to_flow_file->setContent("Message");
    testController.runSession(plan);
    CHECK(read_from_failure_relationship->numberOfFlowFilesRead() == 0);

    plan->reset();
    write_to_flow_file->setContent("");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    testController.runSession(plan);
    CHECK(read_from_failure_relationship->readFlowFileWithContent("Message"));
  }

  SECTION("Timeout test without enough time") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferAge.getName(), "1 h");

    write_to_flow_file->setContent("Message");
    testController.runSession(plan);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 0);
    CHECK(read_from_failure_relationship->numberOfFlowFilesRead() == 0);

    plan->reset();
    write_to_flow_file->setContent("");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    testController.runSession(plan);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 0);
    CHECK(read_from_failure_relationship->numberOfFlowFilesRead() == 0);
  }

  SECTION("Max Buffer test") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferSize.getName(), "100 B");
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");

    write_to_flow_file->setContent("Message");
    testController.runSession(plan);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 0);
    CHECK(read_from_failure_relationship->numberOfFlowFilesRead() == 0);

    plan->reset();
    write_to_flow_file->setContent(std::string(150, '*'));
    testController.runSession(plan);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 0);
    CHECK(read_from_failure_relationship->readFlowFileWithContent(std::string("Message").append(std::string(150, '*'))));
  }

  SECTION("Max Buffer test without overflow") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferSize.getName(), "100 MB");
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");

    write_to_flow_file->setContent("Message");
    testController.runSession(plan);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 0);

    plan->reset();
    write_to_flow_file->setContent(std::string(150, '*'));
    testController.runSession(plan);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 0);
    CHECK(read_from_failure_relationship->numberOfFlowFilesRead() == 0);
  }
}

TEST_CASE("DefragmentTextInvalidSources", "[defragmenttextinvalidsources]") {
  TestController testController;
  auto plan = testController.createPlan();
  auto write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  auto update_ff = std::dynamic_pointer_cast<UpdateAttribute>(plan->addProcessor("UpdateAttribute", "update_attribute"));
  auto defrag_text_flow_files =  std::dynamic_pointer_cast<DefragmentText>(plan->addProcessor("DefragmentText", "defrag_text_flow_files"));
  auto read_from_failure_relationship = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_failure_relationship"));

  plan->addConnection(write_to_flow_file, WriteToFlowFileTestProcessor::Success, update_ff);
  plan->addConnection(update_ff, UpdateAttribute ::Success, defrag_text_flow_files);

  plan->addConnection(defrag_text_flow_files, DefragmentText::Failure, read_from_failure_relationship);
  defrag_text_flow_files->setAutoTerminatedRelationships({DefragmentText::Success});

  read_from_failure_relationship->setAutoTerminatedRelationships({ReadFromFlowFileTestProcessor::Success});

  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");
  plan->setProperty(update_ff, org::apache::nifi::minifi::processors::textfragmentutils::BASE_NAME_ATTRIBUTE, "${UUID()}", true);

  write_to_flow_file->setContent("Foo <1> Foo");
  testController.runSession(plan);
  CHECK(read_from_failure_relationship->numberOfFlowFilesRead() == 0);
  write_to_flow_file->setContent("Bar <2> Bar");
  plan->reset();
  testController.runSession(plan);
  CHECK(read_from_failure_relationship->numberOfFlowFilesRead() == 2);
  CHECK(read_from_failure_relationship->readFlowFileWithContent("<1> Foo"));
  CHECK(read_from_failure_relationship->readFlowFileWithContent("Bar <2> Bar"));
}
