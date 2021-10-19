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

TEST_CASE("DefragTextFlowFilesNoMultilinePatternAtStartTest", "[defragmenttextnomultilinepatternatstarttest]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<WriteToFlowFileTestProcessor> write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(
      plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  std::shared_ptr<DefragmentText> defrag_text_flow_files =  std::dynamic_pointer_cast<DefragmentText>(
      plan->addProcessor("DefragmentText", "defrag_text_flow_files", core::Relationship("success", "description"), true));
  std::shared_ptr<ReadFromFlowFileTestProcessor> read_from_flow_file = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(
      plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_flow_file", DefragmentText::Success, true));
  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");


  write_to_flow_file->setContent("<1> Foo");
  testController.runSession(plan);
  CHECK(read_from_flow_file->numberOfFlowFilesRead() == 0);
  write_to_flow_file->setContent("<2> Bar");
  plan->reset();
  testController.runSession(plan);
  CHECK(read_from_flow_file->readFlowFileWithContent("<1> Foo"));
  write_to_flow_file->setContent("<3> Baz");
  plan->reset();
  testController.runSession(plan);
  CHECK(read_from_flow_file->readFlowFileWithContent("<2> Bar"));
}

TEST_CASE("DefragmentTextEmptyPattern", "[defragmenttextemptypattern]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<WriteToFlowFileTestProcessor> write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(
      plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  std::shared_ptr<DefragmentText> defrag_text_flow_files =  std::dynamic_pointer_cast<DefragmentText>(
      plan->addProcessor("DefragmentText", "defrag_text_flow_files", core::Relationship("success", "description"), true));
  std::shared_ptr<ReadFromFlowFileTestProcessor> read_from_flow_file = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(
      plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_flow_file", DefragmentText::Success, true));
  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "");
  plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc.getName(), toString(DefragmentText::PatternLocation::END_OF_MESSAGE));

  REQUIRE_THROWS_WITH(testController.runSession(plan), "Process Schedule Operation: Pattern property missing or invalid");
}

TEST_CASE("DefragmentTextNoMultilinePatternAtEndTest", "[defragmenttextnomultilinepatternatendtest]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<WriteToFlowFileTestProcessor> write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(
      plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  std::shared_ptr<DefragmentText> defrag_text_flow_files =  std::dynamic_pointer_cast<DefragmentText>(
      plan->addProcessor("DefragmentText", "defrag_text_flow_files", core::Relationship("success", "description"), true));
  std::shared_ptr<ReadFromFlowFileTestProcessor> read_from_flow_file = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(
      plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_flow_file", DefragmentText::Success, true));
  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");
  plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc.getName(), toString(DefragmentText::PatternLocation::END_OF_MESSAGE));

  write_to_flow_file->setContent("Foo <1>");
  testController.runSession(plan);
  CHECK(read_from_flow_file->readFlowFileWithContent("Foo <1>"));
  write_to_flow_file->setContent("Bar <2>");
  plan->reset();
  testController.runSession(plan);
  CHECK(read_from_flow_file->readFlowFileWithContent("Bar <2>"));
  write_to_flow_file->setContent("Baz <3>");
  plan->reset();
  testController.runSession(plan);
  CHECK(read_from_flow_file->readFlowFileWithContent("Baz <3>"));
}

TEST_CASE("DefragmentTextMultilinePatternAtStartTest", "[defragmenttextmultilinepatternatstarttest]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<WriteToFlowFileTestProcessor> write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(
      plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  std::shared_ptr<DefragmentText> defrag_text_flow_files =  std::dynamic_pointer_cast<DefragmentText>(
      plan->addProcessor("DefragmentText", "defrag_text_flow_files", core::Relationship("success", "description"), true));
  std::shared_ptr<ReadFromFlowFileTestProcessor> read_from_flow_file = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(
      plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_flow_file", core::Relationship("success", "description"), true));
  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");


  write_to_flow_file->setContent("apple<1> banana<2> cherry<3> dragon ");
  testController.runSession(plan);
  CHECK(read_from_flow_file->readFlowFileWithContent("apple<1> banana<2> cherry"));

  write_to_flow_file->setContent("fruit<4> elderberry<5> fig<6> grapefruit");
  plan->reset();
  testController.runSession(plan);
  CHECK(read_from_flow_file->readFlowFileWithContent("<3> dragon fruit<4> elderberry<5> fig"));
}

TEST_CASE("DefragmentTextMultilinePatternAtEndTest", "[defragmenttextmultilinepatternatendtest]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<WriteToFlowFileTestProcessor> write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(
      plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  std::shared_ptr<DefragmentText> defrag_text_flow_files =  std::dynamic_pointer_cast<DefragmentText>(
      plan->addProcessor("DefragmentText", "defrag_text_flow_files", core::Relationship("success", "description"), true));
  std::shared_ptr<ReadFromFlowFileTestProcessor> read_from_flow_file = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(
      plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_flow_file", core::Relationship("success", "description"), true));
  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");
  plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc.getName(), toString(DefragmentText::PatternLocation::END_OF_MESSAGE));

  write_to_flow_file->setContent("apple<1> banana<2> cherry<3> dragon ");
  testController.runSession(plan);
  CHECK(read_from_flow_file->readFlowFileWithContent("apple<1> banana<2> cherry<3>"));

  write_to_flow_file->setContent("fruit<4> elderberry<5> fig<6> grapefruit");
  plan->reset();
  testController.runSession(plan);
  CHECK(read_from_flow_file->readFlowFileWithContent(" dragon fruit<4> elderberry<5> fig<6>"));
}


TEST_CASE("DefragmentTextTimeoutTest", "[defragmenttexttimeottest]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<WriteToFlowFileTestProcessor> write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(
      plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  std::shared_ptr<DefragmentText> defrag_text_flow_files =  std::dynamic_pointer_cast<DefragmentText>(
      plan->addProcessor("DefragmentText", "defrag_text_flow_files", core::Relationship("success", "description"), true));
  std::shared_ptr<ReadFromFlowFileTestProcessor> read_from_flow_file = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(
      plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_flow_file", {DefragmentText::Success, DefragmentText::Failure}, true));
  plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferAge.getName(), "100 ms");
  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");


  write_to_flow_file->setContent("Message");
  testController.runSession(plan);
  CHECK(read_from_flow_file->numberOfFlowFilesRead() == 0);

  plan->reset();
  write_to_flow_file->setContent("");
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  testController.runSession(plan);
  CHECK(read_from_flow_file->readFlowFileWithContent("Message"));
}

TEST_CASE("DefragmentTextNoTimeoutTest", "[defragmenttextnotimeottest]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<WriteToFlowFileTestProcessor> write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(
      plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  std::shared_ptr<DefragmentText> defrag_text_flow_files =  std::dynamic_pointer_cast<DefragmentText>(
      plan->addProcessor("DefragmentText", "defrag_text_flow_files", core::Relationship("success", "description"), true));
  std::shared_ptr<ReadFromFlowFileTestProcessor> read_from_flow_file = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(
      plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_flow_file", {DefragmentText::Success, DefragmentText::Failure}, true));
  plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferAge.getName(), "1 h");
  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");


  write_to_flow_file->setContent("Message");
  testController.runSession(plan);
  CHECK(read_from_flow_file->numberOfFlowFilesRead() == 0);

  plan->reset();
  write_to_flow_file->setContent("");
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  testController.runSession(plan);
  CHECK(read_from_flow_file->numberOfFlowFilesRead() == 0);
}

TEST_CASE("DefragmentTextMaxBufferTest", "[defragmenttextmaxbuffertest]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<WriteToFlowFileTestProcessor> write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(
      plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  std::shared_ptr<DefragmentText> defrag_text_flow_files =  std::dynamic_pointer_cast<DefragmentText>(
      plan->addProcessor("DefragmentText", "defrag_text_flow_files", core::Relationship("success", "description"), true));
  std::shared_ptr<ReadFromFlowFileTestProcessor> read_from_flow_file = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(
      plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_flow_file", {DefragmentText::Success, DefragmentText::Failure}, true));
  plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferSize.getName(), "100 B");
  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");

  write_to_flow_file->setContent("Message");
  testController.runSession(plan);
  CHECK(read_from_flow_file->numberOfFlowFilesRead() == 0);

  plan->reset();
  write_to_flow_file->setContent(std::string(150, '*'));
  testController.runSession(plan);
  CHECK(read_from_flow_file->readFlowFileWithContent(std::string("Message").append(std::string(150, '*'))));
}

TEST_CASE("DefragmentTextNoMaxBufferTest", "[defragmenttextnomaxbuffertest]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<WriteToFlowFileTestProcessor> write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(
      plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  std::shared_ptr<DefragmentText> defrag_text_flow_files =  std::dynamic_pointer_cast<DefragmentText>(
      plan->addProcessor("DefragmentText", "defrag_text_flow_files", core::Relationship("success", "description"), true));
  std::shared_ptr<ReadFromFlowFileTestProcessor> read_from_flow_file = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(
      plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_flow_file", {DefragmentText::Success, DefragmentText::Failure}, true));
  plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferSize.getName(), "100 MB");
  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");

  write_to_flow_file->setContent("Message");
  testController.runSession(plan);
  CHECK(read_from_flow_file->numberOfFlowFilesRead() == 0);

  plan->reset();
  write_to_flow_file->setContent(std::string(150, '*'));
  testController.runSession(plan);
  CHECK(read_from_flow_file->numberOfFlowFilesRead() == 0);
}

TEST_CASE("DefragmentTextInvalidRegexTest", "[defragmenttextinvalidregextest]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<DefragmentText> defrag_text_flow_files =  std::dynamic_pointer_cast<DefragmentText>(
      plan->addProcessor("DefragmentText", "defrag_text_flow_files", core::Relationship("success", "description")));
  defrag_text_flow_files->setAutoTerminatedRelationships({DefragmentText::Success, DefragmentText::Failure});
  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "\"[a-b][a\"");

  REQUIRE_THROWS(testController.runSession(plan));
}

TEST_CASE("DefragmentTextInvalidSources", "[defragmenttextinvalidsources]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<WriteToFlowFileTestProcessor> write_to_flow_file = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(
      plan->addProcessor("WriteToFlowFileTestProcessor", "write_to_flow_file"));
  std::shared_ptr<UpdateAttribute> update_ff = std::dynamic_pointer_cast<UpdateAttribute>(
      plan->addProcessor("UpdateAttribute", "update_attribute", core::Relationship("success", "description"), true));
  std::shared_ptr<DefragmentText> defrag_text_flow_files =  std::dynamic_pointer_cast<DefragmentText>(
      plan->addProcessor("DefragmentText", "defrag_text_flow_files", core::Relationship("success", "description"), true));
  std::shared_ptr<ReadFromFlowFileTestProcessor> read_from_failure_relationship = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(
      plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_failure_relationship", DefragmentText::Failure, true));
  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern.getName(), "<[0-9]+>");
  plan->setProperty(update_ff, org::apache::nifi::minifi::processors::textfragmentutils::BASE_NAME_ATTRIBUTE, "${UUID()}", true);
  defrag_text_flow_files->setAutoTerminatedRelationships({DefragmentText::Success});

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
