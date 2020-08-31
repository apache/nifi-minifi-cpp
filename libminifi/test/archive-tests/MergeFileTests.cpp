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
#include <iostream>
#include <map>
#include <memory>
#include <utility>
#include <set>
#include <sstream>
#include <string>

#include "core/Core.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessorNode.h"
#include "core/ProcessSession.h"
#include "FlowController.h"
#include "../../include/core/FlowFile.h"
#include "MergeContent.h"
#include "processors/LogAttribute.h"
#include "../TestBase.h"
#include "../unit/ProvenanceTestHelper.h"

std::string FLOW_FILE;
std::string EXPECT_MERGE_CONTENT_FIRST;
std::string EXPECT_MERGE_CONTENT_SECOND;
std::string HEADER_FILE;
std::string FOOTER_FILE;
std::string DEMARCATOR_FILE;

void init_file_paths() {
  struct Initializer {
    Initializer() {
      static TestController global_controller;
      char format[] = "/tmp/test.XXXXXX";
      std::string tempDir = global_controller.createTempDirectory(format);
      FLOW_FILE = utils::file::FileUtils::concat_path(tempDir, "minifi-mergecontent");
      EXPECT_MERGE_CONTENT_FIRST = utils::file::FileUtils::concat_path(tempDir, "minifi-expect-mergecontent1.txt");
      EXPECT_MERGE_CONTENT_SECOND = utils::file::FileUtils::concat_path(tempDir, "minifi-expect-mergecontent2.txt");
      HEADER_FILE = utils::file::FileUtils::concat_path(tempDir, "minifi-mergecontent.header");
      FOOTER_FILE = utils::file::FileUtils::concat_path(tempDir, "minifi-mergecontent.footer");
      DEMARCATOR_FILE = utils::file::FileUtils::concat_path(tempDir, "minifi-mergecontent.demarcator");
    }
  };
  static Initializer initializer;
}

class FixedBuffer : public org::apache::nifi::minifi::InputStreamCallback {
 public:
  explicit FixedBuffer(std::size_t capacity) : capacity_(capacity) {
    buf_.reset(new uint8_t[capacity_]);
  }
  FixedBuffer(FixedBuffer&& other) : buf_(std::move(other.buf_)), size_(other.size_), capacity_(other.capacity_) {
    other.size_ = 0;
    other.capacity_ = 0;
  }
  std::size_t size() const { return size_; }
  std::size_t capacity() const { return capacity_; }
  uint8_t* begin() const { return buf_.get(); }
  uint8_t* end() const { return buf_.get() + size_; }

  std::string to_string() const {
    return {begin(), end()};
  }

  template<class Input>
  int write(Input input, std::size_t len) {
    REQUIRE(size_ + len <= capacity_);
    int total_read = 0;
    do {
      auto ret = input.read(end(), len);
      if (ret == 0) break;
      if (ret < 0) return ret;
      size_ += ret;
      len -= ret;
      total_read += ret;
    } while (size_ != capacity_);
    return total_read;
  }
  int64_t process(std::shared_ptr<org::apache::nifi::minifi::io::BaseStream> stream) {
    return write(*stream.get(), capacity_);
  }

 private:
  std::unique_ptr<uint8_t[]> buf_;
  std::size_t size_ = 0;
  std::size_t capacity_ = 0;
};

std::vector<FixedBuffer> read_archives(const FixedBuffer& input) {
  class ArchiveEntryReader {
   public:
    explicit ArchiveEntryReader(archive* arch) : arch(arch) {}
    int read(uint8_t* out, std::size_t len) {
      return archive_read_data(arch, out, len);
    }
   private:
    archive* arch;
  };
  std::vector<FixedBuffer> archive_contents;
  struct archive *a;
  a = archive_read_new();
  archive_read_support_format_all(a);
  archive_read_support_filter_all(a);
  archive_read_open_memory(a, input.begin(), input.size());
  struct archive_entry *ae;

  while (archive_read_next_header(a, &ae) == ARCHIVE_OK) {
    int size = gsl::narrow<int>(archive_entry_size(ae));
    FixedBuffer buf(size);
    ArchiveEntryReader reader(a);
    auto ret = buf.write(reader, buf.capacity());
    REQUIRE(ret == size);
    archive_contents.emplace_back(std::move(buf));
  }
  return archive_contents;
}

class MergeTestController : public TestController {
 public:
  MergeTestController() {
    init_file_paths();
    LogTestController::getInstance().setTrace<minifi::processors::MergeContent>();
    LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<core::ProcessSession>();
    LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
    LogTestController::getInstance().setTrace<minifi::processors::BinFiles>();
    LogTestController::getInstance().setTrace<minifi::processors::Bin>();
    LogTestController::getInstance().setTrace<minifi::processors::BinManager>();
    LogTestController::getInstance().setTrace<minifi::Connection>();
    LogTestController::getInstance().setTrace<minifi::core::Connectable>();

    std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();
    auto content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(std::make_shared<minifi::Configure>());

    processor = std::make_shared<processors::MergeContent>("mergecontent");
    processor->initialize();
    utils::Identifier processoruuid;
    REQUIRE(processor->getUUID(processoruuid));
    std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<minifi::processors::LogAttribute>("logattribute");
    utils::Identifier logAttributeuuid;
    REQUIRE(logAttributeProcessor->getUUID(logAttributeuuid));

    // output from merge processor to log attribute
    output = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
    output->addRelationship(processors::MergeContent::Merge);
    output->setSource(processor);
    output->setDestination(logAttributeProcessor);
    output->setSourceUUID(processoruuid);
    output->setDestinationUUID(logAttributeuuid);
    processor->addConnection(output);
    // input to merge processor
    input = std::make_shared<minifi::Connection>(repo, content_repo, "mergeinput");
    input->setDestination(processor);
    input->setDestinationUUID(processoruuid);
    processor->addConnection(input);

    processor->setAutoTerminatedRelationships({processors::MergeContent::Original, processors::MergeContent::Failure});

    processor->incrementActiveTasks();
    processor->setScheduledState(core::ScheduledState::RUNNING);
    logAttributeProcessor->incrementActiveTasks();
    logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

    context = std::make_shared<core::ProcessContext>(std::make_shared<core::ProcessorNode>(processor), nullptr, repo, repo, content_repo);
  }
  ~MergeTestController() {
    LogTestController::getInstance().reset();
  }

  std::shared_ptr<core::ProcessContext> context;
  std::shared_ptr<core::ProcessorNode> node;
  std::shared_ptr<core::Processor> processor;
  std::shared_ptr<minifi::Connection> input;
  std::shared_ptr<minifi::Connection> output;
};

TEST_CASE_METHOD(MergeTestController, "MergeFileDefragment", "[mergefiletest1]") {
  {
    std::ofstream expectfileFirst(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);
    std::ofstream expectfileSecond(EXPECT_MERGE_CONTENT_SECOND, std::ios::binary);

    // Create and write to the test file
    for (int i = 0; i < 6; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      std::ofstream tmpfile(flowFileName.c_str(), std::ios::binary);
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        if (i < 3)
          expectfileFirst << std::to_string(i);
        else
          expectfileSecond << std::to_string(i);
      }
    }
  }

  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, org::apache::nifi::minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, org::apache::nifi::minifi::processors::merge_content_options::MERGE_STRATEGY_DEFRAGMENT);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStrategy, org::apache::nifi::minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);

  core::ProcessSession sessionGenFlowFile(context);
  // Generate 6 flowfiles, first threes merged to one, second thress merged to one
  for (const int i : {0, 2, 5, 4, 1, 3}) {
    const auto flow = std::static_pointer_cast<core::FlowFile>(sessionGenFlowFile.create());
    std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
    sessionGenFlowFile.import(flowFileName, flow, true, 0);
    // three bundle
    if (i < 3)
      flow->setAttribute(processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(0));
    else
      flow->setAttribute(processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(1));
    if (i < 3)
      flow->setAttribute(processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i));
    else
      flow->setAttribute(processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i-3));
    flow->setAttribute(processors::BinFiles::FRAGMENT_COUNT_ATTRIBUTE, std::to_string(3));
    sessionGenFlowFile.flushContent();
    input->put(flow);
  }

  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  for (int i = 0; i < 6; i++) {
    auto session = std::make_shared<core::ProcessSession>(context);
    processor->onTrigger(context, session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() == 96);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    std::ifstream file1(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
    REQUIRE(callback.to_string() == contents);
  }
  REQUIRE(flow2->getSize() == 96);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, &callback);
    std::ifstream file2(EXPECT_MERGE_CONTENT_SECOND, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());
    REQUIRE(callback.to_string() == contents);
  }
}

TEST_CASE_METHOD(MergeTestController, "MergeFileDefragmentDelimiter", "[mergefiletest2]") {
  {
    std::ofstream expectfileFirst(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);
    std::ofstream expectfileSecond(EXPECT_MERGE_CONTENT_SECOND, std::ios::binary);
    std::ofstream headerfile(HEADER_FILE, std::ios::binary);
    std::ofstream footerfile(FOOTER_FILE, std::ios::binary);
    std::ofstream demarcatorfile(DEMARCATOR_FILE, std::ios::binary);
    headerfile << "header";
    expectfileFirst << "header";
    expectfileSecond << "header";
    footerfile << "footer";
    demarcatorfile << "demarcator";


    // Create and write to the test file
    for (int i = 0; i < 6; i++) {
      if (i != 0 && i <= 2)
        expectfileFirst << "demarcator";
      if (i != 3 && i >= 4)
        expectfileSecond << "demarcator";
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      std::ofstream tmpfile(flowFileName.c_str(), std::ios::binary);
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        if (i < 3)
          expectfileFirst << std::to_string(i);
        else
          expectfileSecond << std::to_string(i);
      }
    }
    expectfileFirst << "footer";
    expectfileSecond << "footer";
  }

  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, org::apache::nifi::minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, org::apache::nifi::minifi::processors::merge_content_options::MERGE_STRATEGY_DEFRAGMENT);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStrategy, org::apache::nifi::minifi::processors::merge_content_options::DELIMITER_STRATEGY_FILENAME);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::Header, HEADER_FILE);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::Footer, FOOTER_FILE);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::Demarcator, DEMARCATOR_FILE);

  core::ProcessSession sessionGenFlowFile(context);
  // Generate 6 flowfiles, first threes merged to one, second thress merged to one
  for (const int i : {0, 2, 5, 4, 1, 3}) {
    const auto flow = std::static_pointer_cast<core::FlowFile>(sessionGenFlowFile.create());
    std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
    sessionGenFlowFile.import(flowFileName, flow, true, 0);
    // three bundle
    if (i < 3)
      flow->setAttribute(processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(0));
    else
      flow->setAttribute(processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(1));
    if (i < 3)
      flow->setAttribute(processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i));
    else
      flow->setAttribute(processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i-3));
    flow->setAttribute(processors::BinFiles::FRAGMENT_COUNT_ATTRIBUTE, std::to_string(3));
    sessionGenFlowFile.flushContent();
    input->put(flow);
  }

  REQUIRE(processor->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  for (int i = 0; i < 6; i++) {
    auto session = std::make_shared<core::ProcessSession>(context);
    processor->onTrigger(context, session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() == 128);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    std::ifstream file1(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
    REQUIRE(callback.to_string() == contents);
  }
  REQUIRE(flow2->getSize() == 128);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, &callback);
    std::ifstream file2(EXPECT_MERGE_CONTENT_SECOND, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());
    REQUIRE(callback.to_string() == contents);
  }
}

TEST_CASE_METHOD(MergeTestController, "MergeFileDefragmentDropFlow", "[mergefiletest3]") {
  {
    std::ofstream expectfileFirst(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);
    std::ofstream expectfileSecond(EXPECT_MERGE_CONTENT_SECOND, std::ios::binary);

    // Create and write to the test file, drop record 4
    for (int i = 0; i < 6; i++) {
      if (i == 4)
        continue;
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      std::ofstream tmpfile(flowFileName.c_str(), std::ios::binary);
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        if (i < 3)
          expectfileFirst << std::to_string(i);
        else
          expectfileSecond << std::to_string(i);
      }
    }
  }

  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, org::apache::nifi::minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, org::apache::nifi::minifi::processors::merge_content_options::MERGE_STRATEGY_DEFRAGMENT);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStrategy, org::apache::nifi::minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MaxBinAge, "1 sec");

  core::ProcessSession sessionGenFlowFile(context);
  // Generate 6 flowfiles, first threes merged to one, second thress merged to one
  for (const int i : {0, 2, 5, 1, 3}) {
    if (i == 4)
      continue;
    const auto flow = std::static_pointer_cast<core::FlowFile>(sessionGenFlowFile.create());
    std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
    sessionGenFlowFile.import(flowFileName, flow, true, 0);
    // three bundle
    if (i < 3)
      flow->setAttribute(processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(0));
    else
      flow->setAttribute(processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(1));
    if (i < 3)
      flow->setAttribute(processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i));
    else
      flow->setAttribute(processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i-3));
    flow->setAttribute(processors::BinFiles::FRAGMENT_COUNT_ATTRIBUTE, std::to_string(3));
    sessionGenFlowFile.flushContent();
    input->put(flow);
  }

  REQUIRE(processor->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  for (int i = 0; i < 6; i++) {
    if (i == 4)
      continue;
    auto session = std::make_shared<core::ProcessSession>(context);
    processor->onTrigger(context, session);
    session->commit();
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  {
    auto session = std::make_shared<core::ProcessSession>(context);
    processor->onTrigger(context, session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() == 96);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    std::ifstream file1(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
    REQUIRE(callback.to_string() == contents);
  }
  REQUIRE(flow2->getSize() == 64);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, &callback);
    std::ifstream file2(EXPECT_MERGE_CONTENT_SECOND, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());
    REQUIRE(callback.to_string() == contents);
  }
}

TEST_CASE_METHOD(MergeTestController, "MergeFileBinPack", "[mergefiletest4]") {
  {
    std::ofstream expectfileFirst(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);
    std::ofstream expectfileSecond(EXPECT_MERGE_CONTENT_SECOND, std::ios::binary);

    // Create and write to the test file
    for (int i = 0; i < 6; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      std::ofstream tmpfile(flowFileName.c_str(), std::ios::binary);
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        if (i < 3)
          expectfileFirst << std::to_string(i);
        else
          expectfileSecond << std::to_string(i);
      }
    }
  }

  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, org::apache::nifi::minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, org::apache::nifi::minifi::processors::merge_content_options::MERGE_STRATEGY_BIN_PACK);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStrategy, org::apache::nifi::minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MinSize, "96");
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::CorrelationAttributeName, "tag");

  core::ProcessSession sessionGenFlowFile(context);
  // Generate 6 flowfiles, first threes merged to one, second thress merged to one
  for (const int i : {0, 1, 2, 3, 4, 5}) {
    const auto flow = std::static_pointer_cast<core::FlowFile>(sessionGenFlowFile.create());
    std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
    sessionGenFlowFile.import(flowFileName, flow, true, 0);
    flow->setAttribute("tag", "tag");
    sessionGenFlowFile.flushContent();
    input->put(flow);
  }

  REQUIRE(processor->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  for (int i = 0; i < 6; i++) {
    auto session = std::make_shared<core::ProcessSession>(context);
    processor->onTrigger(context, session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() == 96);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    std::ifstream file1(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
    REQUIRE(callback.to_string() == contents);
  }
  REQUIRE(flow2->getSize() == 96);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, &callback);
    std::ifstream file2(EXPECT_MERGE_CONTENT_SECOND, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());
    REQUIRE(callback.to_string() == contents);
  }
}


TEST_CASE_METHOD(MergeTestController, "MergeFileTar", "[mergefiletest4]") {
  {
    std::ofstream expectfileFirst(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);
    std::ofstream expectfileSecond(EXPECT_MERGE_CONTENT_SECOND, std::ios::binary);

    // Create and write to the test file
    for (int i = 0; i < 6; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      std::ofstream tmpfile(flowFileName.c_str(), std::ios::binary);
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        if (i < 3)
          expectfileFirst << std::to_string(i);
        else
          expectfileSecond << std::to_string(i);
      }
    }
  }

  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, org::apache::nifi::minifi::processors::merge_content_options::MERGE_FORMAT_TAR_VALUE);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, org::apache::nifi::minifi::processors::merge_content_options::MERGE_STRATEGY_BIN_PACK);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStrategy, org::apache::nifi::minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MinSize, "96");
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::CorrelationAttributeName, "tag");

  core::ProcessSession sessionGenFlowFile(context);
  // Generate 6 flowfiles, first threes merged to one, second thress merged to one
  for (const int i : {0, 1, 2, 3, 4, 5}) {
    const auto flow = std::static_pointer_cast<core::FlowFile>(sessionGenFlowFile.create());
    std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
    sessionGenFlowFile.import(flowFileName, flow, true, 0);
    flow->setAttribute("tag", "tag");
    sessionGenFlowFile.flushContent();
    input->put(flow);
  }

  REQUIRE(processor->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  for (int i = 0; i < 6; i++) {
    auto session = std::make_shared<core::ProcessSession>(context);
    processor->onTrigger(context, session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    auto archives = read_archives(callback);
    REQUIRE(archives.size() == 3);
    for (int i = 0; i < 3; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      std::ifstream file1(flowFileName, std::ios::binary);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      REQUIRE(archives[i].to_string() == contents);
    }
  }
  REQUIRE(flow2->getSize() > 0);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, &callback);
    auto archives = read_archives(callback);
    REQUIRE(archives.size() == 3);
    for (int i = 3; i < 6; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      std::ifstream file1(flowFileName, std::ios::binary);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      REQUIRE(archives[i-3].to_string() == contents);
    }
  }
}

TEST_CASE_METHOD(MergeTestController, "MergeFileZip", "[mergefiletest5]") {
  {
    std::ofstream expectfileFirst(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);
    std::ofstream expectfileSecond(EXPECT_MERGE_CONTENT_SECOND, std::ios::binary);

    // Create and write to the test file
    for (int i = 0; i < 6; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      std::ofstream tmpfile(flowFileName.c_str(), std::ios::binary);
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        if (i < 3)
          expectfileFirst << std::to_string(i);
        else
          expectfileSecond << std::to_string(i);
      }
    }
  }

  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, org::apache::nifi::minifi::processors::merge_content_options::MERGE_FORMAT_ZIP_VALUE);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, org::apache::nifi::minifi::processors::merge_content_options::MERGE_STRATEGY_BIN_PACK);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStrategy, org::apache::nifi::minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MinSize, "96");
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::CorrelationAttributeName, "tag");

  core::ProcessSession sessionGenFlowFile(context);
  // Generate 6 flowfiles, first threes merged to one, second thress merged to one
  for (const int i : {0, 1, 2, 3, 4, 5}) {
    const auto flow = std::static_pointer_cast<core::FlowFile>(sessionGenFlowFile.create());
    std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
    sessionGenFlowFile.import(flowFileName, flow, true, 0);
    flow->setAttribute("tag", "tag");
    sessionGenFlowFile.flushContent();
    input->put(flow);
  }

  REQUIRE(processor->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  for (int i = 0; i < 6; i++) {
    auto session = std::make_shared<core::ProcessSession>(context);
    processor->onTrigger(context, session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    auto archives = read_archives(callback);
    REQUIRE(archives.size() == 3);
    for (int i = 0; i < 3; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      std::ifstream file1(flowFileName, std::ios::binary);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      REQUIRE(archives[i].to_string() == contents);
    }
  }
  REQUIRE(flow2->getSize() > 0);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, &callback);
    auto archives = read_archives(callback);
    REQUIRE(archives.size() == 3);
    for (int i = 3; i < 6; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      std::ifstream file1(flowFileName, std::ios::binary);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      REQUIRE(archives[i-3].to_string() == contents);
    }
  }
}

TEST_CASE_METHOD(MergeTestController, "MergeFileOnAttribute", "[mergefiletest5]") {
  {
    std::ofstream expectfileFirst(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);
    std::ofstream expectfileSecond(EXPECT_MERGE_CONTENT_SECOND, std::ios::binary);

    // Create and write to the test file
    for (int i = 0; i < 6; i++) {
      std::ofstream{std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt", std::ios::binary} << std::to_string(i);
      if (i % 2 == 0)
        expectfileFirst << std::to_string(i);
      else
        expectfileSecond << std::to_string(i);
    }
  }

  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, org::apache::nifi::minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, org::apache::nifi::minifi::processors::merge_content_options::MERGE_STRATEGY_BIN_PACK);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStrategy, org::apache::nifi::minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MinEntries, "3");
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::CorrelationAttributeName, "tag");

  core::ProcessSession sessionGenFlowFile(context);
  // Generate 6 flowfiles, even files are merged to one, odd files are merged to an other
  for (const int i : {0, 1, 2, 3, 4, 5}) {
    const auto flow = std::static_pointer_cast<core::FlowFile>(sessionGenFlowFile.create());
    std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
    sessionGenFlowFile.import(flowFileName, flow, true, 0);
    if (i % 2 == 0)
      flow->setAttribute("tag", "even");
    else
      flow->setAttribute("tag", "odd");
    sessionGenFlowFile.flushContent();
    input->put(flow);
  }

  REQUIRE(processor->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  for (int i = 0; i < 6; i++) {
    auto session = std::make_shared<core::ProcessSession>(context);
    processor->onTrigger(context, session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output->poll(expiredFlowRecords);
  {
    FixedBuffer callback(flow1->getSize());
    sessionGenFlowFile.read(flow1, &callback);
    std::ifstream file1(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
    REQUIRE(callback.to_string() == contents);
  }
  {
    FixedBuffer callback(flow2->getSize());
    sessionGenFlowFile.read(flow2, &callback);
    std::ifstream file2(EXPECT_MERGE_CONTENT_SECOND, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());
    REQUIRE(callback.to_string() == contents);
  }
}

TEST_CASE_METHOD(MergeTestController, "Test Merge File Attributes Keeping Only Common Attributes", "[testMergeFileKeepOnlyCommonAttributes]") {
  {
    std::ofstream expectfileFirst(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);

    // Create and write to the test file
    for (int i = 0; i < 3; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      std::ofstream tmpfile(flowFileName.c_str(), std::ios::binary);
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        expectfileFirst << std::to_string(i);
      }
    }
  }

  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, org::apache::nifi::minifi::processors::merge_content_options::MERGE_FORMAT_TAR_VALUE);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, org::apache::nifi::minifi::processors::merge_content_options::MERGE_STRATEGY_DEFRAGMENT);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStrategy, org::apache::nifi::minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);

  core::ProcessSession sessionGenFlowFile(context);

  // Generate 3 flowfiles merging all into one
  for (const int i : {1, 2, 0}) {
    const auto flow = std::static_pointer_cast<core::FlowFile>(sessionGenFlowFile.create());
    std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
    sessionGenFlowFile.import(flowFileName, flow, true, 0);
    flow->setAttribute(processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(0));
    flow->setAttribute(processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i));
    flow->setAttribute(processors::BinFiles::FRAGMENT_COUNT_ATTRIBUTE, std::to_string(3));
    flow->setAttribute("mime.type", "application/octet-stream");
    if (i == 1)
      flow->setAttribute("tagUnique1", "unique1");
    else if (i == 2)
      flow->setAttribute("tagUnique2", "unique2");
    if (i % 2 == 0) {
      flow->setAttribute("tagUncommon", "uncommon1");
    } else {
      flow->setAttribute("tagUncommon", "uncommon2");
    }
    flow->setAttribute("tagCommon", "common");

    sessionGenFlowFile.flushContent();
    input->put(flow);
  }

  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  for (int i = 0; i < 3; i++) {
    auto session = std::make_shared<core::ProcessSession>(context);
    processor->onTrigger(context, session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow = output->poll(expiredFlowRecords);

  auto attributes = flow->getAttributes();
  REQUIRE(attributes.find("tagUncommon") == attributes.end());
  REQUIRE(attributes.find("tagUnique1") == attributes.end());
  REQUIRE(attributes.find("tagUnique2") == attributes.end());
  REQUIRE(attributes["tagCommon"] == "common");
  REQUIRE(attributes["mime.type"] == "application/tar");
}

TEST_CASE_METHOD(MergeTestController, "Test Merge File Attributes Keeping All Unique Attributes", "[testMergeFileKeepAllUniqueAttributes]") {
  {
    std::ofstream expectfileFirst(EXPECT_MERGE_CONTENT_FIRST, std::ios::binary);

    // Create and write to the test file
    for (int i = 0; i < 3; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      std::ofstream tmpfile(flowFileName.c_str(), std::ios::binary);
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        expectfileFirst << std::to_string(i);
      }
    }
  }

  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, org::apache::nifi::minifi::processors::merge_content_options::MERGE_FORMAT_TAR_VALUE);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, org::apache::nifi::minifi::processors::merge_content_options::MERGE_STRATEGY_DEFRAGMENT);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStrategy, org::apache::nifi::minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context->setProperty(org::apache::nifi::minifi::processors::MergeContent::AttributeStrategy, org::apache::nifi::minifi::processors::merge_content_options::ATTRIBUTE_STRATEGY_KEEP_ALL_UNIQUE);

  core::ProcessSession sessionGenFlowFile(context);
  // Generate 3 flowfiles merging all into one
  for (const int i : {1, 2, 0}) {
    const auto flow = std::static_pointer_cast<core::FlowFile>(sessionGenFlowFile.create());
    std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
    sessionGenFlowFile.import(flowFileName, flow, true, 0);
    flow->setAttribute(processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(0));
    flow->setAttribute(processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i));
    flow->setAttribute(processors::BinFiles::FRAGMENT_COUNT_ATTRIBUTE, std::to_string(3));
    flow->setAttribute("mime.type", "application/octet-stream");
    if (i == 1)
      flow->setAttribute("tagUnique1", "unique1");
    else if (i == 2)
      flow->setAttribute("tagUnique2", "unique2");
    if (i % 2 == 0) {
      flow->setAttribute("tagUncommon", "uncommon1");
    } else {
      flow->setAttribute("tagUncommon", "uncommon2");
    }
    flow->setAttribute("tagCommon", "common");

    sessionGenFlowFile.flushContent();
    input->put(flow);
  }

  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  for (int i = 0; i < 3; i++) {
    auto session = std::make_shared<core::ProcessSession>(context);
    processor->onTrigger(context, session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow = output->poll(expiredFlowRecords);

  auto attributes = flow->getAttributes();
  REQUIRE(attributes.find("tagUncommon") == attributes.end());
  REQUIRE(attributes["tagUnique1"] == "unique1");
  REQUIRE(attributes["tagUnique2"] == "unique2");
  REQUIRE(attributes["tagCommon"] == "common");
  REQUIRE(attributes["mime.type"] == "application/tar");
}
