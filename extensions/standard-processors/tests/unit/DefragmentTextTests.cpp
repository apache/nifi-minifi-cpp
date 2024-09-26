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
#include <array>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/WriteToFlowFileTestProcessor.h"
#include "unit/ReadFromFlowFileTestProcessor.h"
#include "UpdateAttribute.h"
#include "DefragmentText.h"
#include "TextFragmentUtils.h"
#include "serialization/PayloadSerializer.h"
#include "serialization/FlowFileSerializer.h"

using WriteToFlowFileTestProcessor = org::apache::nifi::minifi::processors::WriteToFlowFileTestProcessor;
using ReadFromFlowFileTestProcessor = org::apache::nifi::minifi::processors::ReadFromFlowFileTestProcessor;
using UpdateAttribute = org::apache::nifi::minifi::processors::UpdateAttribute;
using DefragmentText = org::apache::nifi::minifi::processors::DefragmentText;
namespace textfragmentutils = org::apache::nifi::minifi::processors::textfragmentutils;
namespace defragment_text = org::apache::nifi::minifi::processors::defragment_text;

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

  read_from_success_relationship->setAutoTerminatedRelationships(std::array<core::Relationship, 1>{ReadFromFlowFileTestProcessor::Success});
  read_from_failure_relationship->setAutoTerminatedRelationships(std::array<core::Relationship, 1>{ReadFromFlowFileTestProcessor::Success});


  SECTION("Throws on empty pattern") {
    REQUIRE_THROWS(testController.runSession(plan));
  }

  SECTION("Throws on invalid pattern") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern, "\"[a-b][a\"");

    REQUIRE_THROWS(testController.runSession(plan));
  }

  SECTION("Single line messages starting with pattern") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern, "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc, magic_enum::enum_name(defragment_text::PatternLocation::START_OF_MESSAGE));

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
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern, "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc, magic_enum::enum_name(defragment_text::PatternLocation::END_OF_MESSAGE));

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
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern, "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc, magic_enum::enum_name(defragment_text::PatternLocation::START_OF_MESSAGE));

    write_to_flow_file->setContent("apple<1> banana<2> cherry<3> dragon ");
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent("apple<1> banana<2> cherry"));

    write_to_flow_file->setContent("fruit<4> elderberry<5> fig<6> grapefruit");
    plan->reset();
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent("<3> dragon fruit<4> elderberry<5> fig"));
  }

  SECTION("Multiline matching end of messages") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern, "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc, magic_enum::enum_name(defragment_text::PatternLocation::END_OF_MESSAGE));

    write_to_flow_file->setContent("apple<1> banana<2> cherry<3> dragon ");
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent("apple<1> banana<2> cherry<3>"));

    write_to_flow_file->setContent("fruit<4> elderberry<5> fig<6> grapefruit");
    plan->reset();
    testController.runSession(plan);
    CHECK(read_from_success_relationship->readFlowFileWithContent(" dragon fruit<4> elderberry<5> fig<6>"));
  }

  SECTION("Timeout test Start of Line") {
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern, "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferAge, "100 ms");

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
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern, "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc, magic_enum::enum_name(defragment_text::PatternLocation::START_OF_MESSAGE));
    plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferAge, "100 ms");

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
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern, "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::PatternLoc, magic_enum::enum_name(defragment_text::PatternLocation::END_OF_MESSAGE));
    plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferAge, "100 ms");

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
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern, "<[0-9]+>");
    plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferAge, "1 h");

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
    plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferSize, "100 B");
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern, "<[0-9]+>");

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
    plan->setProperty(defrag_text_flow_files, DefragmentText::MaxBufferSize, "100 MB");
    plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern, "<[0-9]+>");

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

TEST_CASE("DefragmentTextMultipleSources", "[defragmenttextinvalidsources]") {
  TestController testController;
  auto plan = testController.createPlan();
  auto input_1 = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(plan->addProcessor("WriteToFlowFileTestProcessor", "input_1"));
  auto input_2 = std::dynamic_pointer_cast<WriteToFlowFileTestProcessor>(plan->addProcessor("WriteToFlowFileTestProcessor", "input_2"));
  auto update_ff_1 = std::dynamic_pointer_cast<UpdateAttribute>(plan->addProcessor("UpdateAttribute", "update_attribute_1"));
  auto update_ff_2 = std::dynamic_pointer_cast<UpdateAttribute>(plan->addProcessor("UpdateAttribute", "update_attribute_2"));
  auto defrag_text_flow_files = std::dynamic_pointer_cast<DefragmentText>(plan->addProcessor("DefragmentText", "defrag_text_flow_files"));
  auto read_from_failure_relationship = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_failure_relationship"));
  auto read_from_success_relationship = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_success_relationship"));

  plan->addConnection(input_1, WriteToFlowFileTestProcessor::Success, update_ff_1);
  plan->addConnection(input_2, WriteToFlowFileTestProcessor::Success, update_ff_2);
  plan->addConnection(update_ff_1, UpdateAttribute::Success, defrag_text_flow_files);
  plan->addConnection(update_ff_2, UpdateAttribute::Success, defrag_text_flow_files);

  plan->addConnection(defrag_text_flow_files, DefragmentText::Failure, read_from_failure_relationship);
  plan->addConnection(defrag_text_flow_files, DefragmentText::Success, read_from_success_relationship);

  read_from_failure_relationship->disableClearOnTrigger();
  read_from_success_relationship->disableClearOnTrigger();
  read_from_failure_relationship->setAutoTerminatedRelationships(std::array<core::Relationship, 1>{ReadFromFlowFileTestProcessor::Success});
  read_from_success_relationship->setAutoTerminatedRelationships(std::array<core::Relationship, 1>{ReadFromFlowFileTestProcessor::Success});
  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern, "%");

  SECTION("Multiple Sources with different fragment attributes") {
    plan->setDynamicProperty(update_ff_1, core::SpecialFlowAttribute::ABSOLUTE_PATH, "input_1");
    plan->setDynamicProperty(update_ff_2, core::SpecialFlowAttribute::ABSOLUTE_PATH, "input_2");

    input_1->setContent("abc%def");
    input_2->setContent("ABC%DEF");
    testController.runSession(plan);
    plan->reset();
    input_1->clearContent();
    input_2->clearContent();
    testController.runSession(plan);

    CHECK(read_from_failure_relationship->numberOfFlowFilesRead() == 0);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 2);
    CHECK(read_from_success_relationship->readFlowFileWithContent("abc"));
    CHECK(read_from_success_relationship->readFlowFileWithContent("ABC"));

    plan->reset();
    input_1->setContent("ghi%jkl");
    input_2->setContent("GHI%JKL");
    testController.runSession(plan);
    plan->reset();
    input_1->clearContent();
    input_2->clearContent();
    testController.runSession(plan);

    CHECK(read_from_failure_relationship->numberOfFlowFilesRead() == 0);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 4);
    CHECK(read_from_success_relationship->readFlowFileWithContent("%defghi"));
    CHECK(read_from_success_relationship->readFlowFileWithContent("%DEFGHI"));
  }

  SECTION("Multiple Sources with same fragment attributes mix up") {
    plan->setDynamicProperty(update_ff_1, core::SpecialFlowAttribute::ABSOLUTE_PATH, "input");
    plan->setDynamicProperty(update_ff_2, core::SpecialFlowAttribute::ABSOLUTE_PATH, "input");

    input_1->setContent("abc%def");
    input_2->setContent("ABC%DEF");
    testController.runSession(plan);
    plan->reset();
    input_1->clearContent();
    input_2->clearContent();
    testController.runSession(plan);

    CHECK(read_from_failure_relationship->numberOfFlowFilesRead() == 0);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 2);
    CHECK((read_from_success_relationship->readFlowFileWithContent("abc") || read_from_success_relationship->readFlowFileWithContent("ABC")));
    CHECK((read_from_success_relationship->readFlowFileWithContent("%DEFabc") || read_from_success_relationship->readFlowFileWithContent("%defABC")));

    plan->reset();
    input_1->setContent("ghi%jkl");
    input_2->setContent("GHI%JKL");
    testController.runSession(plan);
    plan->reset();
    input_1->clearContent();
    input_2->clearContent();
    testController.runSession(plan);

    CHECK(read_from_failure_relationship->numberOfFlowFilesRead() == 0);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 4);
    CHECK((read_from_success_relationship->readFlowFileWithContent("%defghi")
        || read_from_success_relationship->readFlowFileWithContent("%defGHI")
        || read_from_success_relationship->readFlowFileWithContent("%DEFGHI")
        || read_from_success_relationship->readFlowFileWithContent("%DEFghi")));
  }
}

class FragmentGenerator : public core::ProcessorImpl {
 public:
  static constexpr const char* Description = "FragmentGenerator (only for testing purposes)";
  static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  static constexpr auto Success = core::RelationshipDefinition{"success", "success operational on the flow record"};
  static constexpr auto Relationships = std::array{Success};
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit FragmentGenerator(std::string_view name, const utils::Identifier& uuid = utils::Identifier())
      : ProcessorImpl(name, uuid) {
  }

  void onTrigger(core::ProcessContext&, core::ProcessSession& session) override {
    std::vector<core::FlowFile> flow_files;
    for (const size_t max_i = i_ + batch_size_; i_ < fragment_contents_.size() && i_ < max_i; ++i_) {
      std::shared_ptr<core::FlowFile> flow_file = session.create();
      if (base_name_attribute_)
        flow_file->addAttribute(textfragmentutils::BASE_NAME_ATTRIBUTE, *base_name_attribute_);
      if (post_name_attribute_)
        flow_file->addAttribute(textfragmentutils::POST_NAME_ATTRIBUTE, *post_name_attribute_);
      if (absolute_path_attribute_)
        flow_file->addAttribute(core::SpecialFlowAttribute::ABSOLUTE_PATH, *absolute_path_attribute_);
      flow_file->addAttribute(textfragmentutils::OFFSET_ATTRIBUTE, std::to_string(offset_));
      auto& fragment_content = fragment_contents_[i_];
      offset_ += fragment_content.size();
      session.writeBuffer(flow_file, fragment_content);
      session.transfer(flow_file, Success);
    }
  }
  void initialize() override { setSupportedRelationships(Relationships); }

  void setFragments(std::vector<std::string>&& fragments) {fragment_contents_ = std::move(fragments);}
  void setBatchSize(const size_t batch_size) {batch_size_ = batch_size;}
  void setAbsolutePathAttribute(const std::string& absolute_path_attribute) { absolute_path_attribute_ = absolute_path_attribute; }
  void setBaseNameAttribute(const std::string& base_name_attribute) { base_name_attribute_ = base_name_attribute; }
  void setPostNameAttribute(const std::string& post_name_attribute) { post_name_attribute_ = post_name_attribute; }
  void clearAbsolutePathAttribute() { absolute_path_attribute_.reset(); }
  void clearPostNameAttribute() { post_name_attribute_.reset(); }
  void clearBaseNameAttribute() { base_name_attribute_.reset(); }

 protected:
  size_t offset_ = 0;
  size_t batch_size_ = 1;
  size_t i_ = 0;
  std::optional<std::string> absolute_path_attribute_;
  std::optional<std::string> base_name_attribute_;
  std::optional<std::string> post_name_attribute_;
  std::vector<std::string> fragment_contents_;
};

REGISTER_RESOURCE(FragmentGenerator, Processor);

TEST_CASE("DefragmentText with offset attributes", "[defragmenttextoffsetattributes]") {
  TestController testController;
  auto plan = testController.createPlan();
  auto input_1 = std::dynamic_pointer_cast<FragmentGenerator>(plan->addProcessor("FragmentGenerator", "input_1"));
  auto input_2 = std::dynamic_pointer_cast<FragmentGenerator>(plan->addProcessor("FragmentGenerator", "input_2"));

  auto defrag_text_flow_files = std::dynamic_pointer_cast<DefragmentText>(plan->addProcessor("DefragmentText", "defrag_text_flow_files"));
  auto read_from_failure_relationship = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_failure_relationship"));
  auto read_from_success_relationship = std::dynamic_pointer_cast<ReadFromFlowFileTestProcessor>(plan->addProcessor("ReadFromFlowFileTestProcessor", "read_from_success_relationship"));

  plan->addConnection(input_1, FragmentGenerator::Success, defrag_text_flow_files);
  plan->addConnection(input_2, FragmentGenerator::Success, defrag_text_flow_files);

  plan->addConnection(defrag_text_flow_files, DefragmentText::Failure, read_from_failure_relationship);
  plan->addConnection(defrag_text_flow_files, DefragmentText::Success, read_from_success_relationship);

  read_from_failure_relationship->disableClearOnTrigger();
  read_from_success_relationship->disableClearOnTrigger();
  read_from_failure_relationship->setAutoTerminatedRelationships(std::array<core::Relationship, 1>{ReadFromFlowFileTestProcessor::Success});
  read_from_success_relationship->setAutoTerminatedRelationships(std::array<core::Relationship, 1>{ReadFromFlowFileTestProcessor::Success});
  plan->setProperty(defrag_text_flow_files, DefragmentText::Pattern, "%");
  input_1->setBaseNameAttribute("input_1");
  input_2->setBaseNameAttribute("input_2");
  input_1->setPostNameAttribute("log");
  input_2->setPostNameAttribute("log");
  input_1->setAbsolutePathAttribute("/tmp/input/input_1.log");
  input_2->setAbsolutePathAttribute("/tmp/input/input_2.log");

  SECTION("Single source input with offsets") {
    input_1->setFragments({"foo%bar", "%baz,app", "le%"});
    for (size_t i=0; i < 10; ++i) {
      testController.runSession(plan);
      plan->reset();
    }
    CHECK(read_from_failure_relationship->numberOfFlowFilesRead() == 0);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 3);
    CHECK(read_from_success_relationship->readFlowFileWithContent("foo"));
    CHECK(read_from_success_relationship->readFlowFileWithContent("%bar"));
    CHECK(read_from_success_relationship->readFlowFileWithContent("%baz,apple"));
  }

  SECTION("Two input sources with offsets") {
    input_1->setFragments({"foo%bar", "%baz,app", "le%"});
    input_2->setFragments({"monkey%dog", "%cat,octopu", "s%"});
    for (size_t i=0; i < 10; ++i) {
      testController.runSession(plan);
      plan->reset();
    }
    CHECK(read_from_failure_relationship->numberOfFlowFilesRead() == 0);
    CHECK(read_from_success_relationship->numberOfFlowFilesRead() == 6);
    CHECK(read_from_success_relationship->readFlowFileWithContent("foo"));
    CHECK(read_from_success_relationship->readFlowFileWithContent("%bar"));
    CHECK(read_from_success_relationship->readFlowFileWithContent("%baz,apple"));
    CHECK(read_from_success_relationship->readFlowFileWithContent("monkey"));
    CHECK(read_from_success_relationship->readFlowFileWithContent("%dog"));
    CHECK(read_from_success_relationship->readFlowFileWithContent("%cat,octopus"));
  }
}
