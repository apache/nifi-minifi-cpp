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
#include <map>
#include <memory>
#include <utility>
#include <string>
#include <set>
#include <random>
#include <sstream>
#include <iostream>
#include "FlowController.h"
#include "../TestBase.h"
#include "core/Core.h"
#include "../../include/core/FlowFile.h"
#include "../unit/ProvenanceTestHelper.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "CompressContent.h"
#include "io/FileStream.h"
#include "FlowFileRecord.h"
#include "processors/LogAttribute.h"
#include "processors/PutFile.h"
#include "utils/file/FileUtils.h"
#include "../Utils.h"
#include "utils/gsl.h"

class ReadCallback: public minifi::InputStreamCallback {
 public:
  explicit ReadCallback(size_t size)
      :buffer_size_{size}  // default member initializers use this
  {}
  ReadCallback(const ReadCallback&) = delete;
  ReadCallback(ReadCallback&&) = delete;
  ReadCallback& operator=(const ReadCallback&) = delete;
  ReadCallback& operator=(ReadCallback&&) = delete;

  ~ReadCallback() override {
    delete[] buffer_;
    delete[] archive_buffer_;
  }
  int64_t process(const std::shared_ptr<minifi::io::BaseStream>& stream) override {
    int64_t total_read = 0;
    do {
      const auto ret = stream->read(buffer_ + read_size_, buffer_size_ - read_size_);
      if (ret == 0) break;
      if (minifi::io::isError(ret)) return -1;
      read_size_ += gsl::narrow<size_t>(ret);
      total_read += ret;
    } while (buffer_size_ != read_size_);
    return total_read;
  }
  void archive_read() {
    struct archive *a;
    a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    archive_read_open_memory(a, buffer_, read_size_);
    struct archive_entry *ae;

    REQUIRE(archive_read_next_header(a, &ae) == ARCHIVE_OK);
    const auto size = archive_entry_size(ae);
    archive_buffer_ = new char[size];
    archive_buffer_size_ = size;
    archive_read_data(a, archive_buffer_, gsl::narrow<size_t>(size));
    archive_read_free(a);
  }

  size_t buffer_size_;
  uint8_t *buffer_ = new uint8_t[buffer_size_];
  size_t read_size_ = 0;
  char *archive_buffer_ = nullptr;
  int64_t archive_buffer_size_ = 0;
};

/**
 * There is strong coupling between these compression and decompression
 * tests. Some compression tests also set up the stage for the subsequent
 * decompression test. Each such test controller should either be
 * CompressTestController or a DecompressTestController.
 */
class CompressDecompressionTestController : public TestController{
 protected:
  static std::string tempDir_;
  static std::string raw_content_path_;
  static std::string compressed_content_path_;
  static TestController& get_global_controller() {
    static TestController controller;
    return controller;
  }

  void setupFlow() {
    LogTestController::getInstance().setTrace<processors::CompressContent>();
    LogTestController::getInstance().setTrace<processors::LogAttribute>();
    LogTestController::getInstance().setTrace<core::ProcessSession>();
    LogTestController::getInstance().setTrace<core::ProcessContext>();
    LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
    LogTestController::getInstance().setTrace<minifi::Connection>();
    LogTestController::getInstance().setTrace<minifi::core::Connectable>();
    LogTestController::getInstance().setTrace<minifi::io::FileStream>();

    std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

    processor = std::make_shared<processors::CompressContent>("compresscontent");
    processor->initialize();
    utils::Identifier processoruuid = processor->getUUID();
    REQUIRE(processoruuid);

    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(std::make_shared<minifi::Configure>());
    // connection from compress processor to log attribute
    output = std::make_shared<minifi::Connection>(repo, content_repo, "Output");
    output->addRelationship(core::Relationship("success", "compress successful output"));
    output->setSource(processor);
    output->setSourceUUID(processoruuid);
    processor->addConnection(output);
    // connection to compress processor
    input = std::make_shared<minifi::Connection>(repo, content_repo, "Input");
    input->setDestination(processor);
    input->setDestinationUUID(processoruuid);
    processor->addConnection(input);

    processor->setAutoTerminatedRelationships({{"failure", ""}});

    processor->incrementActiveTasks();
    processor->setScheduledState(core::ScheduledState::RUNNING);

    context = std::make_shared<core::ProcessContext>(std::make_shared<core::ProcessorNode>(processor), nullptr, repo, repo, content_repo);
  }

 public:
  class RawContent{
    std::string content_;
    explicit RawContent(std::string&& content_): content_(std::move(content_)) {}
    friend class CompressDecompressionTestController;
   public:
    bool operator==(const std::string& actual) const noexcept {
      return content_ == actual;
    }
    bool operator!=(const std::string& actual) const noexcept {
      return content_ != actual;
    }
  };

  std::string rawContentPath() const {
    return raw_content_path_;
  }

  std::string compressedPath() const {
    return compressed_content_path_;
  }

  RawContent getRawContent() const {
    std::ifstream file;
    file.open(raw_content_path_, std::ios::binary);
    std::string contents{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return RawContent{std::move(contents)};
  }

  virtual ~CompressDecompressionTestController() = 0;

  std::shared_ptr<core::Processor> processor;
  std::shared_ptr<core::ProcessContext> context;
  std::shared_ptr<minifi::Connection> output;
  std::shared_ptr<minifi::Connection> input;
};

CompressDecompressionTestController::~CompressDecompressionTestController() {
  LogTestController::getInstance().reset();
}

std::string CompressDecompressionTestController::tempDir_;
std::string CompressDecompressionTestController::raw_content_path_;
std::string CompressDecompressionTestController::compressed_content_path_;

class CompressTestController : public CompressDecompressionTestController {
  static void initContentWithRandomData() {
    int random_seed = 0x454;
    std::ofstream file;
    file.open(raw_content_path_, std::ios::binary);

    std::mt19937 gen(random_seed);
    std::uniform_int_distribution<> dis(0, 99);
    for (int i = 0; i < 100000; i++) {
      file << std::to_string(dis(gen));
    }
  }

 public:
  CompressTestController() {
    char CompressionFormat[] = "/tmp/test.XXXXXX";
    tempDir_ = get_global_controller().createTempDirectory(CompressionFormat);
    REQUIRE(!tempDir_.empty());
    raw_content_path_ = utils::file::FileUtils::concat_path(tempDir_, "minifi-expect-compresscontent.txt");
    compressed_content_path_ = utils::file::FileUtils::concat_path(tempDir_, "minifi-compresscontent");
    initContentWithRandomData();
    setupFlow();
  }

  template<class ...Args>
  void writeCompressed(Args&& ...args) {
    std::ofstream file(compressed_content_path_, std::ios::binary);
    file.write(std::forward<Args>(args)...);
  }
};

class DecompressTestController : public CompressDecompressionTestController{
 public:
  DecompressTestController() {
    setupFlow();
  }
  ~DecompressTestController() {
    tempDir_ = "";
    raw_content_path_ = "";
    compressed_content_path_ = "";
  }
};

using CompressionFormat = processors::CompressContent::ExtendedCompressionFormat;
using CompressionMode = processors::CompressContent::CompressionMode;

TEST_CASE_METHOD(CompressTestController, "CompressFileGZip", "[compressfiletest1]") {
  context->setProperty(processors::CompressContent::CompressMode, toString(CompressionMode::Compress));
  context->setProperty(processors::CompressContent::CompressFormat, toString(CompressionFormat::GZIP));
  context->setProperty(processors::CompressContent::CompressLevel, "9");
  context->setProperty(processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(rawContentPath(), flow, true, 0);
  sessionGenFlowFile.flushContent();
  input->put(flow);

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  auto session = std::make_shared<core::ProcessSession>(context);
  processor->onTrigger(context, session);
  session->commit();

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    flow1->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, mime);
    REQUIRE(mime == "application/gzip");
    ReadCallback callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    callback.archive_read();
    std::string content(reinterpret_cast<char *> (callback.archive_buffer_), callback.archive_buffer_size_);
    REQUIRE(getRawContent() == content);
    // write the compress content for next test
    writeCompressed(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
  }
}

TEST_CASE_METHOD(DecompressTestController, "DecompressFileGZip", "[compressfiletest2]") {
  context->setProperty(processors::CompressContent::CompressMode, toString(CompressionMode::Decompress));
  context->setProperty(processors::CompressContent::CompressFormat, toString(CompressionFormat::GZIP));
  context->setProperty(processors::CompressContent::CompressLevel, "9");
  context->setProperty(processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(compressedPath(), flow, true, 0);
  sessionGenFlowFile.flushContent();
  input->put(flow);

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  auto session = std::make_shared<core::ProcessSession>(context);
  processor->onTrigger(context, session);
  session->commit();

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    REQUIRE(flow1->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, mime) == false);
    ReadCallback callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    std::string content(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
    REQUIRE(getRawContent() == content);
  }
}

TEST_CASE_METHOD(CompressTestController, "CompressFileBZip", "[compressfiletest3]") {
  context->setProperty(processors::CompressContent::CompressMode, toString(CompressionMode::Compress));
  context->setProperty(processors::CompressContent::CompressFormat, toString(CompressionFormat::BZIP2));
  context->setProperty(processors::CompressContent::CompressLevel, "9");
  context->setProperty(processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(rawContentPath(), flow, true, 0);
  sessionGenFlowFile.flushContent();
  input->put(flow);

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  auto session = std::make_shared<core::ProcessSession>(context);
  processor->onTrigger(context, session);
  session->commit();

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    flow1->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, mime);
    REQUIRE(mime == "application/bzip2");
    ReadCallback callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    callback.archive_read();
    std::string contents(reinterpret_cast<char *> (callback.archive_buffer_), callback.archive_buffer_size_);
    REQUIRE(getRawContent() == contents);
    // write the compress content for next test
    writeCompressed(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
  }
}


TEST_CASE_METHOD(DecompressTestController, "DecompressFileBZip", "[compressfiletest4]") {
  context->setProperty(processors::CompressContent::CompressMode, toString(CompressionMode::Decompress));
  context->setProperty(processors::CompressContent::CompressFormat, toString(CompressionFormat::BZIP2));
  context->setProperty(processors::CompressContent::CompressLevel, "9");
  context->setProperty(processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(compressedPath(), flow, true, 0);
  sessionGenFlowFile.flushContent();
  input->put(flow);

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  auto session = std::make_shared<core::ProcessSession>(context);
  processor->onTrigger(context, session);
  session->commit();

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    REQUIRE(flow1->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, mime) == false);
    ReadCallback callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    std::string contents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
    REQUIRE(getRawContent() == contents);
  }
}

TEST_CASE_METHOD(CompressTestController, "CompressFileLZMA", "[compressfiletest5]") {
  context->setProperty(processors::CompressContent::CompressMode, toString(CompressionMode::Compress));
  context->setProperty(processors::CompressContent::CompressFormat, toString(CompressionFormat::LZMA));
  context->setProperty(processors::CompressContent::CompressLevel, "9");
  context->setProperty(processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(rawContentPath(), flow, true, 0);
  sessionGenFlowFile.flushContent();
  input->put(flow);

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  auto session = std::make_shared<core::ProcessSession>(context);
  processor->onTrigger(context, session);
  session->commit();

  if (LogTestController::getInstance().contains("compression not supported on this platform")) {
    // platform not support LZMA
    LogTestController::getInstance().reset();
    return;
  }

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    flow1->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, mime);
    REQUIRE(mime == "application/x-lzma");
    ReadCallback callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    callback.archive_read();
    std::string contents(reinterpret_cast<char *> (callback.archive_buffer_), callback.archive_buffer_size_);
    REQUIRE(getRawContent() == contents);
    // write the compress content for next test
    writeCompressed(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
  }
}


TEST_CASE_METHOD(DecompressTestController, "DecompressFileLZMA", "[compressfiletest6]") {
  context->setProperty(processors::CompressContent::CompressMode, toString(CompressionMode::Decompress));
  context->setProperty(processors::CompressContent::CompressFormat, toString(CompressionFormat::USE_MIME_TYPE));
  context->setProperty(processors::CompressContent::CompressLevel, "9");
  context->setProperty(processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(compressedPath(), flow, true, 0);
  flow->setAttribute(core::SpecialFlowAttribute::MIME_TYPE, "application/x-lzma");
  sessionGenFlowFile.flushContent();
  input->put(flow);

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  auto session = std::make_shared<core::ProcessSession>(context);
  processor->onTrigger(context, session);
  session->commit();

  if (LogTestController::getInstance().contains("compression not supported on this platform")) {
    // platform not support LZMA
    LogTestController::getInstance().reset();
    return;
  }

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    REQUIRE(flow1->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, mime) == false);
    ReadCallback callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    std::string contents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
    REQUIRE(getRawContent() == contents);
  }
}

TEST_CASE_METHOD(CompressTestController, "CompressFileXYLZMA", "[compressfiletest7]") {
  context->setProperty(processors::CompressContent::CompressMode, toString(CompressionMode::Compress));
  context->setProperty(processors::CompressContent::CompressFormat, toString(CompressionFormat::XZ_LZMA2));
  context->setProperty(processors::CompressContent::CompressLevel, "9");
  context->setProperty(processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(rawContentPath(), flow, true, 0);
  sessionGenFlowFile.flushContent();
  input->put(flow);

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  auto session = std::make_shared<core::ProcessSession>(context);
  processor->onTrigger(context, session);
  session->commit();

  if (LogTestController::getInstance().contains("compression not supported on this platform")) {
    // platform not support LZMA
    LogTestController::getInstance().reset();
    return;
  }

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    flow1->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, mime);
    REQUIRE(mime == "application/x-xz");
    ReadCallback callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    callback.archive_read();
    std::string contents(reinterpret_cast<char *> (callback.archive_buffer_), callback.archive_buffer_size_);
    REQUIRE(getRawContent() == contents);
    // write the compress content for next test
    writeCompressed(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
  }
}


TEST_CASE_METHOD(DecompressTestController, "DecompressFileXYLZMA", "[compressfiletest8]") {
  context->setProperty(processors::CompressContent::CompressMode, toString(CompressionMode::Decompress));
  context->setProperty(processors::CompressContent::CompressFormat, toString(CompressionFormat::USE_MIME_TYPE));
  context->setProperty(processors::CompressContent::CompressLevel, "9");
  context->setProperty(processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(compressedPath(), flow, true, 0);
  flow->setAttribute(core::SpecialFlowAttribute::MIME_TYPE, "application/x-xz");
  sessionGenFlowFile.flushContent();
  input->put(flow);

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  auto session = std::make_shared<core::ProcessSession>(context);
  processor->onTrigger(context, session);
  session->commit();

  if (LogTestController::getInstance().contains("compression not supported on this platform")) {
    // platform not support LZMA
    LogTestController::getInstance().reset();
    return;
  }

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    REQUIRE(flow1->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, mime) == false);
    ReadCallback callback(gsl::narrow<size_t>(flow1->getSize()));
    sessionGenFlowFile.read(flow1, &callback);
    std::string contents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
    REQUIRE(getRawContent() == contents);
  }
}

TEST_CASE_METHOD(TestController, "RawGzipCompressionDecompression", "[compressfiletest8]") {
  LogTestController::getInstance().setTrace<processors::CompressContent>();
  LogTestController::getInstance().setTrace<processors::PutFile>();

  // Create temporary directories
  char format_src[] = "/tmp/archives.XXXXXX";
  std::string src_dir = createTempDirectory(format_src);
  REQUIRE(!src_dir.empty());

  char format_dst[] = "/tmp/archived.XXXXXX";
  std::string dst_dir = createTempDirectory(format_dst);
  REQUIRE(!dst_dir.empty());

  // Define files
  std::string src_file = utils::file::FileUtils::concat_path(src_dir, "src.txt");
  std::string compressed_file = utils::file::FileUtils::concat_path(dst_dir, "src.txt.gz");
  std::string decompressed_file = utils::file::FileUtils::concat_path(dst_dir, "src.txt");

  // Build MiNiFi processing graph
  auto plan = createPlan();
  auto get_file = plan->addProcessor(
      "GetFile",
      "GetFile");
  auto compress_content = plan->addProcessor(
      "CompressContent",
      "CompressContent",
      core::Relationship("success", "d"),
      true);
  auto put_compressed = plan->addProcessor(
      "PutFile",
      "PutFile",
      core::Relationship("success", "d"),
      true);
  auto decompress_content = plan->addProcessor(
      "CompressContent",
      "CompressContent",
      core::Relationship("success", "d"),
      true);
  auto put_decompressed = plan->addProcessor(
      "PutFile",
      "PutFile",
      core::Relationship("success", "d"),
      true);

  // Configure GetFile processor
  plan->setProperty(get_file, "Input Directory", src_dir);

  // Configure CompressContent processor for compression
  plan->setProperty(compress_content, "Mode", toString(CompressionMode::Compress));
  plan->setProperty(compress_content, "Compression Format", toString(CompressionFormat::GZIP));
  plan->setProperty(compress_content, "Update Filename", "true");
  plan->setProperty(compress_content, "Encapsulate in TAR", "false");

  // Configure first PutFile processor
  plan->setProperty(put_compressed, "Directory", dst_dir);

  // Configure CompressContent processor for decompression
  plan->setProperty(decompress_content, "Mode", toString(CompressionMode::Decompress));
  plan->setProperty(decompress_content, "Compression Format", toString(CompressionFormat::GZIP));
  plan->setProperty(decompress_content, "Update Filename", "true");
  plan->setProperty(decompress_content, "Encapsulate in TAR", "false");

  // Configure second PutFile processor
  plan->setProperty(put_decompressed, "Directory", dst_dir);

  // Create source file
  std::string content;
  SECTION("Empty content") {
  }
  SECTION("Short content") {
    content = "Repeated repeated repeated repeated repeated stuff.";
  }
  SECTION("Long content") {
    std::stringstream content_ss;
    // if only this part fails VolatileContentRepository's fixed max size
    // and some change in the cleanup logic might interfere
    for (size_t i = 0U; i < 512 * 1024U; i++) {
      content_ss << "foobar";
    }
    content = content_ss.str();
  }

  std::ofstream{ src_file } << content;

  // Run flow
  runSession(plan, true);

  // Check compressed file
  std::ifstream compressed(compressed_file, std::ios::in | std::ios::binary);
  std::vector<uint8_t> compressed_content((std::istreambuf_iterator<char>(compressed)), std::istreambuf_iterator<char>());
  REQUIRE(2 < compressed_content.size());
  // gzip magic number
  REQUIRE(0x1f == compressed_content[0]);
  REQUIRE(0x8b == compressed_content[1]);

  // Check decompressed file
  std::ifstream decompressed(decompressed_file, std::ios::in | std::ios::binary);
  std::string decompressed_content((std::istreambuf_iterator<char>(decompressed)), std::istreambuf_iterator<char>());
  REQUIRE(content == decompressed_content);

  LogTestController::getInstance().reset();
}

TEST_CASE_METHOD(CompressTestController, "Batch CompressFileGZip", "[compressFileBatchTest]") {
  std::vector<std::string> flowFileContents{
    utils::StringUtils::repeat("0", 1000), utils::StringUtils::repeat("1", 1000),
    utils::StringUtils::repeat("2", 1000), utils::StringUtils::repeat("3", 1000),
  };
  const std::size_t batchSize = 3;

  context->setProperty(processors::CompressContent::CompressMode, toString(CompressionMode::Compress));
  context->setProperty(processors::CompressContent::CompressFormat, toString(CompressionFormat::GZIP));
  context->setProperty(processors::CompressContent::CompressLevel, "9");
  context->setProperty(processors::CompressContent::UpdateFileName, "true");
  context->setProperty(processors::CompressContent::BatchSize, std::to_string(batchSize));


  core::ProcessSession sessionGenFlowFile(context);
  for (const auto& content : flowFileContents) {
    auto flow = sessionGenFlowFile.create();
    sessionGenFlowFile.importFrom(minifi::io::BufferStream(content), flow);
    sessionGenFlowFile.flushContent();
    input->put(flow);
  }

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);

  // Trigger once to process batchSize
  {
    auto session = std::make_shared<core::ProcessSession>(context);
    processor->onTrigger(context, session);
    session->commit();
  }

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::vector<std::shared_ptr<core::FlowFile>> outFiles;
  while (std::shared_ptr<core::FlowFile> file = output->poll(expiredFlowRecords)) {
    outFiles.push_back(std::move(file));
  }
  REQUIRE(outFiles.size() == batchSize);

  // Trigger a second time to process the remaining files
  {
    auto session = std::make_shared<core::ProcessSession>(context);
    processor->onTrigger(context, session);
    session->commit();
  }

  while (std::shared_ptr<core::FlowFile> file = output->poll(expiredFlowRecords)) {
    outFiles.push_back(std::move(file));
  }
  REQUIRE(outFiles.size() == flowFileContents.size());

  for (std::size_t idx = 0; idx < outFiles.size(); ++idx) {
    auto file = outFiles[idx];
    std::string mime;
    file->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, mime);
    REQUIRE(mime == "application/gzip");
    ReadCallback callback(gsl::narrow<size_t>(file->getSize()));
    sessionGenFlowFile.read(file, &callback);
    callback.archive_read();
    std::string content(reinterpret_cast<char *> (callback.archive_buffer_), callback.archive_buffer_size_);
    REQUIRE(flowFileContents[idx] == content);
  }
}
