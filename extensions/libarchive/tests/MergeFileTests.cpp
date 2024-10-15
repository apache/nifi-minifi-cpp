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
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <utility>
#include <set>
#include <sstream>
#include <string>

#include "core/Relationship.h"
#include "core/Core.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessorNode.h"
#include "core/ProcessSession.h"
#include "FlowController.h"
#include "../../include/core/FlowFile.h"
#include "MergeContent.h"
#include "processors/LogAttribute.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/ProvenanceTestHelper.h"
#include "serialization/FlowFileV3Serializer.h"
#include "serialization/PayloadSerializer.h"
#include "unit/TestUtils.h"
#include "utils/gsl.h"
#include "utils/span.h"

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
      auto tempDir = global_controller.createTempDirectory();
      FLOW_FILE = (tempDir / "minifi-mergecontent").string();
      EXPECT_MERGE_CONTENT_FIRST = (tempDir / "minifi-expect-mergecontent1.txt").string();
      EXPECT_MERGE_CONTENT_SECOND = (tempDir / "minifi-expect-mergecontent2.txt").string();
      HEADER_FILE = (tempDir / "minifi-mergecontent.header").string();
      FOOTER_FILE = (tempDir / "minifi-mergecontent.footer").string();
      DEMARCATOR_FILE = (tempDir / "minifi-mergecontent.demarcator").string();
    }
  };
  static Initializer initializer;
}

class FixedBuffer {
 public:
  explicit FixedBuffer(std::size_t capacity) : capacity_(capacity) {
    buf_.reset(new uint8_t[capacity_]);  // NOLINT(cppcoreguidelines-owning-memory)
  }
  FixedBuffer(FixedBuffer&& other) noexcept : buf_(std::move(other.buf_)), size_(other.size_), capacity_(other.capacity_) {
    other.size_ = 0;
    other.capacity_ = 0;
  }
  FixedBuffer(const FixedBuffer&) = delete;
  FixedBuffer& operator=(FixedBuffer&&) = delete;
  FixedBuffer& operator=(const FixedBuffer&) = delete;
  ~FixedBuffer() = default;
  [[nodiscard]] std::size_t size() const { return size_; }
  [[nodiscard]] std::size_t capacity() const { return capacity_; }
  [[nodiscard]] uint8_t* begin() const { return buf_.get(); }
  [[nodiscard]] uint8_t* end() const { return buf_.get() + size_; }

  [[nodiscard]] std::string to_string() const {
    return {begin(), end()};
  }

  template<class Input>
  int64_t write(Input& input, std::size_t len) {
    REQUIRE(size_ + len <= capacity_);
    size_t total_read = 0;
    do {
      const size_t ret{input.read(as_writable_bytes(std::span(end(), len)))};
      if (ret == 0) break;
      if (minifi::io::isError(ret)) return -1;
      size_ += ret;
      len -= ret;
      total_read += ret;
    } while (size_ != capacity_);
    return gsl::narrow<int64_t>(total_read);
  }

  int64_t operator()(const std::shared_ptr<minifi::io::InputStream>& stream) {
    return write(*stream, capacity_);
  }

 private:
  std::unique_ptr<uint8_t[]> buf_;  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  std::size_t size_ = 0;
  std::size_t capacity_ = 0;
};

std::vector<FixedBuffer> read_archives(const FixedBuffer& input) {
  class ArchiveEntryReader {
   public:
    explicit ArchiveEntryReader(archive* arch) : arch(arch) {}
    size_t read(std::span<std::byte> out_buffer) {
      const auto ret = archive_read_data(arch, out_buffer.data(), out_buffer.size());
      return ret < 0 ? minifi::io::STREAM_ERROR : gsl::narrow<size_t>(ret);
    }
   private:
    archive* arch;
  };
  std::vector<FixedBuffer> archive_contents;
  struct archive *a = archive_read_new();
  archive_read_support_format_all(a);
  archive_read_support_filter_all(a);
  archive_read_open_memory(a, input.begin(), input.size());
  struct archive_entry *ae = nullptr;

  while (archive_read_next_header(a, &ae) == ARCHIVE_OK) {
    int64_t size{archive_entry_size(ae)};
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
    content_repo->initialize(std::make_shared<minifi::ConfigureImpl>());

    merge_content_processor_ = std::make_unique<minifi::processors::MergeContent>("mergecontent");
    merge_content_processor_->initialize();
    utils::Identifier processoruuid = merge_content_processor_->getUUID();
    REQUIRE(processoruuid);
    log_attribute_processor_ = std::make_unique<minifi::processors::LogAttribute>("logattribute");
    utils::Identifier logAttributeuuid = log_attribute_processor_->getUUID();
    REQUIRE(logAttributeuuid);

    // output from merge processor to log attribute
    output_ = std::make_unique<minifi::ConnectionImpl>(repo, content_repo, "logattributeconnection");
    output_->addRelationship(minifi::processors::MergeContent::Merge);
    output_->setSource(merge_content_processor_.get());
    output_->setDestination(log_attribute_processor_.get());
    output_->setSourceUUID(processoruuid);
    output_->setDestinationUUID(logAttributeuuid);
    merge_content_processor_->addConnection(output_.get());
    // input to merge processor
    input_ = std::make_unique<minifi::ConnectionImpl>(repo, content_repo, "mergeinput");
    input_->setDestination(merge_content_processor_.get());
    input_->setDestinationUUID(processoruuid);
    merge_content_processor_->addConnection(input_.get());

    merge_content_processor_->setAutoTerminatedRelationships(std::array<core::Relationship, 2>{minifi::processors::MergeContent::Original, minifi::processors::MergeContent::Failure});

    merge_content_processor_->incrementActiveTasks();
    merge_content_processor_->setScheduledState(core::ScheduledState::RUNNING);
    log_attribute_processor_->incrementActiveTasks();
    log_attribute_processor_->setScheduledState(core::ScheduledState::RUNNING);

    context_ = std::make_shared<core::ProcessContextImpl>(std::make_shared<core::ProcessorNodeImpl>(merge_content_processor_.get()), nullptr, repo, repo, content_repo);

    for (size_t i = 0; i < 6; ++i) {
      flowFileContents_[i] = utils::string::repeat(std::to_string(i), 32);
    }
  }

  MergeTestController(MergeTestController&&) = delete;
  MergeTestController(const MergeTestController&) = delete;
  MergeTestController& operator=(MergeTestController&&) = delete;
  MergeTestController& operator=(const MergeTestController&) = delete;

  ~MergeTestController() {
    LogTestController::getInstance().reset();
  }

  std::array<std::string, 6> flowFileContents_;
  std::shared_ptr<core::ProcessContext> context_;
  std::unique_ptr<core::Processor> merge_content_processor_;
  std::unique_ptr<core::Processor> log_attribute_processor_;
  std::unique_ptr<minifi::Connection> input_;
  std::unique_ptr<minifi::Connection> output_;
};

TEST_CASE_METHOD(MergeTestController, "MergeFileDefragment", "[mergefiletest1]") {
  std::array<std::string, 2> expected {
    flowFileContents_[0] + flowFileContents_[1] + flowFileContents_[2],
    flowFileContents_[3] + flowFileContents_[4] + flowFileContents_[5]
  };

  context_->setProperty(minifi::processors::MergeContent::MergeFormat, minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
  context_->setProperty(minifi::processors::MergeContent::MergeStrategy, minifi::processors::merge_content_options::MERGE_STRATEGY_DEFRAGMENT);
  context_->setProperty(minifi::processors::MergeContent::DelimiterStrategy, minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);

  core::ProcessSessionImpl sessionGenFlowFile(context_);
  // Generate 6 flowfiles, first three merged to one, second three merged to one
  for (const int i : {0, 2, 5, 4, 1, 3}) {
    const auto flow = sessionGenFlowFile.create();
    sessionGenFlowFile.importFrom(minifi::io::BufferStream(flowFileContents_[i]), flow);
    // three bundle
    if (i < 3)
      flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(0));
    else
      flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(1));
    if (i < 3)
      flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i));
    else
      flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i-3));
    flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_COUNT_ATTRIBUTE, std::to_string(3));
    sessionGenFlowFile.flushContent();
    input_->put(flow);
  }

  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context_);
  merge_content_processor_->onSchedule(*context_, *factory);
  for (int i = 0; i < 6; i++) {
    auto session = std::make_shared<core::ProcessSessionImpl>(context_);
    merge_content_processor_->onTrigger(*context_, *session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output_->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output_->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() == 96);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, std::ref(callback));
    REQUIRE(callback.to_string() == expected[0]);
  }
  REQUIRE(flow2->getSize() == 96);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, std::ref(callback));
    REQUIRE(callback.to_string() == expected[1]);
  }
}

TEST_CASE_METHOD(MergeTestController, "MergeFileDefragmentDelimiter", "[mergefiletest2]") {
  std::array<std::string, 2> expected {
    "header" + flowFileContents_[0] + "demarcator" + flowFileContents_[1] + "demarcator" + flowFileContents_[2] + "footer",
    "header" + flowFileContents_[3] + "demarcator" + flowFileContents_[4] + "demarcator" + flowFileContents_[5] + "footer"
  };

  std::ofstream(HEADER_FILE, std::ios::binary) << "header";
  std::ofstream(FOOTER_FILE, std::ios::binary) << "footer";
  std::ofstream(DEMARCATOR_FILE, std::ios::binary) << "demarcator";

  context_->setProperty(minifi::processors::MergeContent::MergeFormat, minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
  context_->setProperty(minifi::processors::MergeContent::MergeStrategy, minifi::processors::merge_content_options::MERGE_STRATEGY_DEFRAGMENT);
  context_->setProperty(minifi::processors::MergeContent::DelimiterStrategy, minifi::processors::merge_content_options::DELIMITER_STRATEGY_FILENAME);
  context_->setProperty(minifi::processors::MergeContent::Header, HEADER_FILE);
  context_->setProperty(minifi::processors::MergeContent::Footer, FOOTER_FILE);
  context_->setProperty(minifi::processors::MergeContent::Demarcator, DEMARCATOR_FILE);

  core::ProcessSessionImpl sessionGenFlowFile(context_);
  // Generate 6 flowfiles, first three merged to one, second three merged to one
  for (const int i : {0, 2, 5, 4, 1, 3}) {
    const auto flow = sessionGenFlowFile.create();
    sessionGenFlowFile.importFrom(minifi::io::BufferStream(flowFileContents_[i]), flow);
    // three bundle
    if (i < 3)
      flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(0));
    else
      flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(1));
    if (i < 3)
      flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i));
    else
      flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i-3));
    flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_COUNT_ATTRIBUTE, std::to_string(3));
    sessionGenFlowFile.flushContent();
    input_->put(flow);
  }

  REQUIRE(merge_content_processor_->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context_);
  merge_content_processor_->onSchedule(*context_, *factory);
  for (int i = 0; i < 6; i++) {
    auto session = std::make_shared<core::ProcessSessionImpl>(context_);
    merge_content_processor_->onTrigger(*context_, *session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output_->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output_->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() == 128);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, std::ref(callback));
    REQUIRE(callback.to_string() == expected[0]);
  }
  REQUIRE(flow2->getSize() == 128);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, std::ref(callback));
    REQUIRE(callback.to_string() == expected[1]);
  }
}

TEST_CASE_METHOD(MergeTestController, "MergeFileDefragmentDropFlow", "[mergefiletest3]") {
  // drop record 4
  std::array<std::string, 2> expected {
    flowFileContents_[0] + flowFileContents_[1] + flowFileContents_[2],
    flowFileContents_[3] + flowFileContents_[5]
  };

  context_->setProperty(minifi::processors::MergeContent::MergeFormat, minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
  context_->setProperty(minifi::processors::MergeContent::MergeStrategy, minifi::processors::merge_content_options::MERGE_STRATEGY_DEFRAGMENT);
  context_->setProperty(minifi::processors::MergeContent::DelimiterStrategy, minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context_->setProperty(minifi::processors::MergeContent::MaxBinAge, "1 sec");

  core::ProcessSessionImpl sessionGenFlowFile(context_);
  // Generate 5 flowfiles, first threes merged to one, the other two merged to one
  for (const int i : {0, 2, 5, 1, 3}) {
    const auto flow = sessionGenFlowFile.create();
    sessionGenFlowFile.importFrom(minifi::io::BufferStream(flowFileContents_[i]), flow);
    // three bundle
    if (i < 3)
      flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(0));
    else
      flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(1));
    if (i < 3)
      flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i));
    else
      flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i-3));
    flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_COUNT_ATTRIBUTE, std::to_string(3));
    sessionGenFlowFile.flushContent();
    input_->put(flow);
  }

  REQUIRE(merge_content_processor_->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context_);
  merge_content_processor_->onSchedule(*context_, *factory);
  for (int i = 0; i < 5; i++) {
    auto session = std::make_shared<core::ProcessSessionImpl>(context_);
    merge_content_processor_->onTrigger(*context_, *session);
    session->commit();
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  {
    auto session = std::make_shared<core::ProcessSessionImpl>(context_);
    merge_content_processor_->onTrigger(*context_, *session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output_->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output_->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() == 96);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, std::ref(callback));
    REQUIRE(callback.to_string() == expected[0]);
  }
  REQUIRE(flow2->getSize() == 64);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, std::ref(callback));
    REQUIRE(callback.to_string() == expected[1]);
  }
}

TEST_CASE_METHOD(MergeTestController, "MergeFileBinPack", "[mergefiletest4]") {
  std::array<std::string, 2> expected {
    flowFileContents_[0] + flowFileContents_[1] + flowFileContents_[2],
    flowFileContents_[3] + flowFileContents_[4] + flowFileContents_[5]
  };

  context_->setProperty(minifi::processors::MergeContent::MergeFormat, minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
  context_->setProperty(minifi::processors::MergeContent::MergeStrategy, minifi::processors::merge_content_options::MERGE_STRATEGY_BIN_PACK);
  context_->setProperty(minifi::processors::MergeContent::DelimiterStrategy, minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context_->setProperty(minifi::processors::MergeContent::MinSize, "96");
  context_->setProperty(minifi::processors::MergeContent::CorrelationAttributeName, "tag");

  core::ProcessSessionImpl sessionGenFlowFile(context_);
  // Generate 6 flowfiles, first threes merged to one, second thress merged to one
  for (const int i : {0, 1, 2, 3, 4, 5}) {
    const auto flow = sessionGenFlowFile.create();
    sessionGenFlowFile.importFrom(minifi::io::BufferStream(flowFileContents_[i]), flow);
    flow->setAttribute("tag", "tag");
    sessionGenFlowFile.flushContent();
    input_->put(flow);
  }

  REQUIRE(merge_content_processor_->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context_);
  merge_content_processor_->onSchedule(*context_, *factory);
  for (int i = 0; i < 6; i++) {
    auto session = std::make_shared<core::ProcessSessionImpl>(context_);
    merge_content_processor_->onTrigger(*context_, *session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output_->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output_->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() == 96);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, std::ref(callback));
    REQUIRE(callback.to_string() == expected[0]);
  }
  REQUIRE(flow2->getSize() == 96);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, std::ref(callback));
    REQUIRE(callback.to_string() == expected[1]);
  }
}


TEST_CASE_METHOD(MergeTestController, "MergeFileTar", "[mergefiletest4]") {
  context_->setProperty(minifi::processors::MergeContent::MergeFormat, minifi::processors::merge_content_options::MERGE_FORMAT_TAR_VALUE);
  context_->setProperty(minifi::processors::MergeContent::MergeStrategy, minifi::processors::merge_content_options::MERGE_STRATEGY_BIN_PACK);
  context_->setProperty(minifi::processors::MergeContent::DelimiterStrategy, minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context_->setProperty(minifi::processors::MergeContent::MinSize, "96");
  context_->setProperty(minifi::processors::MergeContent::CorrelationAttributeName, "tag");

  core::ProcessSessionImpl sessionGenFlowFile(context_);
  // Generate 6 flowfiles, first threes merged to one, second thress merged to one
  for (const int i : {0, 1, 2, 3, 4, 5}) {
    const auto flow = sessionGenFlowFile.create();
    sessionGenFlowFile.importFrom(minifi::io::BufferStream(flowFileContents_[i]), flow);
    flow->setAttribute("tag", "tag");
    sessionGenFlowFile.flushContent();
    input_->put(flow);
  }

  REQUIRE(merge_content_processor_->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context_);
  merge_content_processor_->onSchedule(*context_, *factory);
  for (int i = 0; i < 6; i++) {
    auto session = std::make_shared<core::ProcessSessionImpl>(context_);
    merge_content_processor_->onTrigger(*context_, *session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output_->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output_->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, std::ref(callback));
    auto archives = read_archives(callback);
    REQUIRE(archives.size() == 3);
    for (int i = 0; i < 3; i++) {
      REQUIRE(archives[i].to_string() == flowFileContents_[i]);
    }
  }
  REQUIRE(flow2->getSize() > 0);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, std::ref(callback));
    auto archives = read_archives(callback);
    REQUIRE(archives.size() == 3);
    for (int i = 3; i < 6; i++) {
      REQUIRE(archives[i-3].to_string() == flowFileContents_[i]);
    }
  }
}

TEST_CASE_METHOD(MergeTestController, "MergeFileZip", "[mergefiletest5]") {
  context_->setProperty(minifi::processors::MergeContent::MergeFormat, minifi::processors::merge_content_options::MERGE_FORMAT_ZIP_VALUE);
  context_->setProperty(minifi::processors::MergeContent::MergeStrategy, minifi::processors::merge_content_options::MERGE_STRATEGY_BIN_PACK);
  context_->setProperty(minifi::processors::MergeContent::DelimiterStrategy, minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context_->setProperty(minifi::processors::MergeContent::MinSize, "96");
  context_->setProperty(minifi::processors::MergeContent::CorrelationAttributeName, "tag");

  core::ProcessSessionImpl sessionGenFlowFile(context_);
  // Generate 6 flowfiles, first threes merged to one, second thress merged to one
  for (const int i : {0, 1, 2, 3, 4, 5}) {
    const auto flow = sessionGenFlowFile.create();
    sessionGenFlowFile.importFrom(minifi::io::BufferStream(flowFileContents_[i]), flow);
    flow->setAttribute("tag", "tag");
    sessionGenFlowFile.flushContent();
    input_->put(flow);
  }

  REQUIRE(merge_content_processor_->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context_);
  merge_content_processor_->onSchedule(*context_, *factory);
  for (int i = 0; i < 6; i++) {
    auto session = std::make_shared<core::ProcessSessionImpl>(context_);
    merge_content_processor_->onTrigger(*context_, *session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output_->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output_->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, std::ref(callback));
    auto archives = read_archives(callback);
    REQUIRE(archives.size() == 3);
    for (int i = 0; i < 3; i++) {
      REQUIRE(archives[i].to_string() == flowFileContents_[i]);
    }
  }
  REQUIRE(flow2->getSize() > 0);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, std::ref(callback));
    auto archives = read_archives(callback);
    REQUIRE(archives.size() == 3);
    for (int i = 3; i < 6; i++) {
      REQUIRE(archives[i-3].to_string() == flowFileContents_[i]);
    }
  }
}

TEST_CASE_METHOD(MergeTestController, "MergeFileOnAttribute", "[mergefiletest5]") {
  std::array<std::string, 2> expected {
    flowFileContents_[0] + flowFileContents_[2] + flowFileContents_[4],
    flowFileContents_[1] + flowFileContents_[3] + flowFileContents_[5]
  };

  context_->setProperty(minifi::processors::MergeContent::MergeFormat, minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
  context_->setProperty(minifi::processors::MergeContent::MergeStrategy, minifi::processors::merge_content_options::MERGE_STRATEGY_BIN_PACK);
  context_->setProperty(minifi::processors::MergeContent::DelimiterStrategy, minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context_->setProperty(minifi::processors::MergeContent::MinEntries, "3");
  context_->setProperty(minifi::processors::MergeContent::CorrelationAttributeName, "tag");

  core::ProcessSessionImpl sessionGenFlowFile(context_);
  // Generate 6 flowfiles, even files are merged to one, odd files are merged to an other
  for (const int i : {0, 1, 2, 3, 4, 5}) {
    const auto flow = sessionGenFlowFile.create();
    sessionGenFlowFile.importFrom(minifi::io::BufferStream(flowFileContents_[i]), flow);
    if (i % 2 == 0)
      flow->setAttribute("tag", "even");
    else
      flow->setAttribute("tag", "odd");
    sessionGenFlowFile.flushContent();
    input_->put(flow);
  }

  REQUIRE(merge_content_processor_->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context_);
  merge_content_processor_->onSchedule(*context_, *factory);
  for (int i = 0; i < 6; i++) {
    auto session = std::make_shared<core::ProcessSessionImpl>(context_);
    merge_content_processor_->onTrigger(*context_, *session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output_->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output_->poll(expiredFlowRecords);
  {
    FixedBuffer callback(flow1->getSize());
    sessionGenFlowFile.read(flow1, std::ref(callback));
    REQUIRE(callback.to_string() == expected[0]);
  }
  {
    FixedBuffer callback(flow2->getSize());
    sessionGenFlowFile.read(flow2, std::ref(callback));
    REQUIRE(callback.to_string() == expected[1]);
  }
}

TEST_CASE_METHOD(MergeTestController, "Test Merge File Attributes Keeping Only Common Attributes", "[testMergeFileKeepOnlyCommonAttributes]") {
  context_->setProperty(minifi::processors::MergeContent::MergeFormat, minifi::processors::merge_content_options::MERGE_FORMAT_TAR_VALUE);
  context_->setProperty(minifi::processors::MergeContent::MergeStrategy, minifi::processors::merge_content_options::MERGE_STRATEGY_DEFRAGMENT);
  context_->setProperty(minifi::processors::MergeContent::DelimiterStrategy, minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);

  core::ProcessSessionImpl sessionGenFlowFile(context_);

  // Generate 3 flowfiles merging all into one
  for (const int i : {1, 2, 0}) {
    const auto flow = sessionGenFlowFile.create();
    sessionGenFlowFile.importFrom(minifi::io::BufferStream(flowFileContents_[i]), flow);
    flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(0));
    flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i));
    flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_COUNT_ATTRIBUTE, std::to_string(3));
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
    input_->put(flow);
  }

  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context_);
  merge_content_processor_->onSchedule(*context_, *factory);
  for (int i = 0; i < 3; i++) {
    auto session = std::make_shared<core::ProcessSessionImpl>(context_);
    merge_content_processor_->onTrigger(*context_, *session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow = output_->poll(expiredFlowRecords);

  auto attributes = flow->getAttributes();
  REQUIRE(attributes.find("tagUncommon") == attributes.end());
  REQUIRE(attributes.find("tagUnique1") == attributes.end());
  REQUIRE(attributes.find("tagUnique2") == attributes.end());
  REQUIRE(attributes["tagCommon"] == "common");
  REQUIRE(attributes["mime.type"] == "application/tar");
}

TEST_CASE_METHOD(MergeTestController, "Test Merge File Attributes Keeping All Unique Attributes", "[testMergeFileKeepAllUniqueAttributes]") {
  context_->setProperty(minifi::processors::MergeContent::MergeFormat, minifi::processors::merge_content_options::MERGE_FORMAT_TAR_VALUE);
  context_->setProperty(minifi::processors::MergeContent::MergeStrategy, minifi::processors::merge_content_options::MERGE_STRATEGY_DEFRAGMENT);
  context_->setProperty(minifi::processors::MergeContent::DelimiterStrategy, minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context_->setProperty(minifi::processors::MergeContent::AttributeStrategy, minifi::processors::merge_content_options::ATTRIBUTE_STRATEGY_KEEP_ALL_UNIQUE);

  core::ProcessSessionImpl sessionGenFlowFile(context_);
  // Generate 3 flowfiles merging all into one
  for (const int i : {1, 2, 0}) {
    const auto flow = sessionGenFlowFile.create();
    sessionGenFlowFile.importFrom(minifi::io::BufferStream(flowFileContents_[i]), flow);
    flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_ID_ATTRIBUTE, std::to_string(0));
    flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_INDEX_ATTRIBUTE, std::to_string(i));
    flow->setAttribute(minifi::processors::BinFiles::FRAGMENT_COUNT_ATTRIBUTE, std::to_string(3));
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
    input_->put(flow);
  }

  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context_);
  merge_content_processor_->onSchedule(*context_, *factory);
  for (int i = 0; i < 3; i++) {
    auto session = std::make_shared<core::ProcessSessionImpl>(context_);
    merge_content_processor_->onTrigger(*context_, *session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow = output_->poll(expiredFlowRecords);

  auto attributes = flow->getAttributes();
  REQUIRE(attributes.find("tagUncommon") == attributes.end());
  REQUIRE(attributes["tagUnique1"] == "unique1");
  REQUIRE(attributes["tagUnique2"] == "unique2");
  REQUIRE(attributes["tagCommon"] == "common");
  REQUIRE(attributes["mime.type"] == "application/tar");
}

void writeString(const std::string& str, const std::shared_ptr<minifi::io::OutputStream>& out) {
  out->write(reinterpret_cast<const uint8_t*>(str.data()), str.length());
}

TEST_CASE("FlowFile serialization", "[testFlowFileSerialization]") {
  MergeTestController testController;
  auto context = testController.context_;
  auto& merge_content_processor = testController.merge_content_processor_;
  auto& input = testController.input_;
  auto& output = testController.output_;

  std::string header = "BEGIN{";
  std::string footer = "}END";
  std::string demarcator = "_";

  core::ProcessSessionImpl session(context);

  minifi::PayloadSerializer payloadSerializer([&] (const std::shared_ptr<core::FlowFile>& ff, const minifi::io::InputStreamCallback& cb) {
    return session.read(ff, cb);
  });
  minifi::FlowFileV3Serializer ffV3Serializer([&] (const std::shared_ptr<core::FlowFile>& ff, const minifi::io::InputStreamCallback& cb) {
    return session.read(ff, cb);
  });

  minifi::FlowFileSerializer* usedSerializer = nullptr;

  std::vector<std::shared_ptr<core::FlowFile>> files;

  for (const auto& content : std::vector<std::string>{"first ff content", "second ff content", "some other data"}) {
    minifi::io::BufferStream contentStream{content};
    auto ff = session.create();
    ff->removeAttribute(core::SpecialFlowAttribute::FILENAME);
    ff->addAttribute("one", "banana");
    ff->addAttribute("two", "seven");
    session.importFrom(contentStream, ff);
    session.flushContent();
    files.push_back(ff);
    input->put(ff);
  }

  context->setProperty(minifi::processors::MergeContent::MergeStrategy, minifi::processors::merge_content_options::MERGE_STRATEGY_BIN_PACK);
  context->setProperty(minifi::processors::MergeContent::DelimiterStrategy, minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context->setProperty(minifi::processors::MergeContent::Header, header);
  context->setProperty(minifi::processors::MergeContent::Footer, footer);
  context->setProperty(minifi::processors::MergeContent::Demarcator, demarcator);
  context->setProperty(minifi::processors::BinFiles::MinEntries, "3");

  SECTION("Payload Serializer") {
    context->setProperty(minifi::processors::MergeContent::MergeFormat, minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
    usedSerializer = &payloadSerializer;
  }
  SECTION("FlowFileV3 Serializer") {
    context->setProperty(minifi::processors::MergeContent::MergeFormat, minifi::processors::merge_content_options::MERGE_FORMAT_FLOWFILE_STREAM_V3_VALUE);
    usedSerializer = &ffV3Serializer;
    // only Binary Concatenation take these into account
    header = "";
    demarcator = "";
    footer = "";
  }

  auto result = std::make_shared<minifi::io::BufferStream>();
  writeString(header, result);
  bool first = true;
  for (const auto& ff : files) {
    if (!first) {
      writeString(demarcator, result);
    }
    first = false;
    usedSerializer->serialize(ff, result);
  }
  writeString(footer, result);

  const auto expected = utils::span_to<std::string>(utils::as_span<const char>(result->getBuffer()));

  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context);
  merge_content_processor->onSchedule(*context, *factory);
  for (int i = 0; i < 3; i++) {
    auto mergeSession = std::make_shared<core::ProcessSessionImpl>(context);
    merge_content_processor->onTrigger(*context, *mergeSession);
    mergeSession->commit();
  }

  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow = output->poll(expiredFlowRecords);

  REQUIRE(expiredFlowRecords.empty());
  {
    FixedBuffer callback(flow->getSize());
    session.read(flow, std::ref(callback));
    REQUIRE(callback.to_string() == expected);
  }

  LogTestController::getInstance().reset();
}

TEST_CASE_METHOD(MergeTestController, "Batch Size", "[testMergeFileBatchSize]") {
  std::array<std::string, 2> expected {
    flowFileContents_[0] + flowFileContents_[1] + flowFileContents_[2],
    flowFileContents_[3] + flowFileContents_[4]
  };

  context_->setProperty(minifi::processors::MergeContent::MergeFormat, minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
  context_->setProperty(minifi::processors::MergeContent::MergeStrategy, minifi::processors::merge_content_options::MERGE_STRATEGY_BIN_PACK);
  context_->setProperty(minifi::processors::MergeContent::DelimiterStrategy, minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context_->setProperty(minifi::processors::BinFiles::BatchSize, "3");

  core::ProcessSessionImpl sessionGenFlowFile(context_);
  // enqueue 5 (five) flowFiles
  for (const int i : {0, 1, 2, 3, 4}) {
    const auto flow = sessionGenFlowFile.create();
    sessionGenFlowFile.importFrom(minifi::io::BufferStream(flowFileContents_[i]), flow);
    sessionGenFlowFile.flushContent();
    input_->put(flow);
  }

  REQUIRE(merge_content_processor_->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context_);
  merge_content_processor_->onSchedule(*context_, *factory);
  // two trigger is enough to process all five flowFiles
  for (int i = 0; i < 2; i++) {
    auto session = std::make_shared<core::ProcessSessionImpl>(context_);
    merge_content_processor_->onTrigger(*context_, *session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output_->poll(expiredFlowRecords);
  std::shared_ptr<core::FlowFile> flow2 = output_->poll(expiredFlowRecords);
  REQUIRE(expiredFlowRecords.empty());
  REQUIRE(flow1);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, std::ref(callback));
    REQUIRE(callback.to_string() == expected[0]);
  }
  REQUIRE(flow2);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, std::ref(callback));
    REQUIRE(callback.to_string() == expected[1]);
  }
}

TEST_CASE_METHOD(MergeTestController, "Maximum Group Size is respected", "[testMergeFileMaximumGroupSize]") {
  // each flowfile content is 32 bytes
  for (auto& ff : flowFileContents_) {
    REQUIRE(ff.length() == 32);
  }
  std::array<std::string, 2> expected {
    flowFileContents_[0] + flowFileContents_[1],
    flowFileContents_[2] + flowFileContents_[3]
  };

  context_->setProperty(minifi::processors::MergeContent::MergeFormat, minifi::processors::merge_content_options::MERGE_FORMAT_CONCAT_VALUE);
  context_->setProperty(minifi::processors::MergeContent::MergeStrategy, minifi::processors::merge_content_options::MERGE_STRATEGY_BIN_PACK);
  context_->setProperty(minifi::processors::MergeContent::DelimiterStrategy, minifi::processors::merge_content_options::DELIMITER_STRATEGY_TEXT);
  context_->setProperty(minifi::processors::BinFiles::BatchSize, "1000");

  // we want a bit more than 2 flowfiles
  context_->setProperty(minifi::processors::BinFiles::MinSize, "65");
  context_->setProperty(minifi::processors::BinFiles::MaxSize, "65");

  context_->setProperty(minifi::processors::BinFiles::MinEntries, "3");
  context_->setProperty(minifi::processors::BinFiles::MaxEntries, "3");

  core::ProcessSessionImpl sessionGenFlowFile(context_);
  // enqueue 6 (six) flowFiles
  for (const int i : {0, 1, 2, 3, 4, 5}) {
    const auto flow = sessionGenFlowFile.create();
    sessionGenFlowFile.writeBuffer(flow, flowFileContents_[i]);
    sessionGenFlowFile.flushContent();
    input_->put(flow);
  }

  REQUIRE(merge_content_processor_->getName() == "mergecontent");
  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context_);
  merge_content_processor_->onSchedule(*context_, *factory);
  // a single trigger is enough to process all five flowFiles
  {
    auto session = std::make_shared<core::ProcessSessionImpl>(context_);
    merge_content_processor_->onTrigger(*context_, *session);
    session->commit();
  }
  // validate the merge content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output_->poll(expiredFlowRecords);
  REQUIRE(expiredFlowRecords.empty());
  REQUIRE(flow1);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, std::ref(callback));
    REQUIRE(callback.to_string() == expected[0]);
  }

  std::shared_ptr<core::FlowFile> flow2 = output_->poll(expiredFlowRecords);
  REQUIRE(expiredFlowRecords.empty());
  REQUIRE(flow2);
  {
    FixedBuffer callback(gsl::narrow<size_t>(flow2->getSize()));
    sessionGenFlowFile.read(flow2, std::ref(callback));
    REQUIRE(callback.to_string() == expected[1]);
  }

  // no more flowfiles
  std::shared_ptr<core::FlowFile> flow3 = output_->poll(expiredFlowRecords);
  REQUIRE(expiredFlowRecords.empty());
  REQUIRE_FALSE(flow3);
}

TEST_CASE("Empty MergeContent yields") {
  const auto merge_content = std::make_shared<minifi::processors::MergeContent>("mergeContent");

  minifi::test::SingleProcessorTestController controller{merge_content};
  controller.trigger();

  CHECK(merge_content->isYield());
}

TEST_CASE("Empty MergeContent doesnt yield when processing readybins") {
  const auto merge_content = std::make_shared<minifi::processors::MergeContent>("mergeContent");

  minifi::test::SingleProcessorTestController controller{merge_content};
  controller.plan->setProperty(merge_content, minifi::processors::MergeContent::MaxBinAge, "100ms");
  controller.plan->setProperty(merge_content, minifi::processors::MergeContent::MinEntries, "2");

  auto first_trigger_results = controller.trigger("foo");
  CHECK_FALSE(merge_content->isYield());
  std::this_thread::sleep_for(100ms);
  auto second_trigger_results = controller.trigger();
  CHECK_FALSE(merge_content->isYield());
}
