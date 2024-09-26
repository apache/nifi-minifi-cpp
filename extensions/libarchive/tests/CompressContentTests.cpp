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
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/Core.h"
#include "../../include/core/FlowFile.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "CompressContent.h"
#include "io/FileStream.h"
#include "FlowFileRecord.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "utils/file/FileUtils.h"
#include "unit/TestUtils.h"
#include "utils/gsl.h"

class ReadCallback {
 public:
  explicit ReadCallback(size_t size)
      :buffer_{size}
  {}
  ReadCallback(const ReadCallback&) = delete;
  ReadCallback(ReadCallback&&) = delete;
  ReadCallback& operator=(const ReadCallback&) = delete;
  ReadCallback& operator=(ReadCallback&&) = delete;
  ~ReadCallback() = default;

  int64_t operator()(const std::shared_ptr<minifi::io::InputStream>& stream) {
    int64_t total_read = 0;
    do {
      const auto ret = stream->read(std::span(buffer_).subspan(read_size_));
      if (ret == 0) break;
      if (minifi::io::isError(ret)) return -1;
      read_size_ += gsl::narrow<size_t>(ret);
      total_read += gsl::narrow<int64_t>(ret);
    } while (buffer_.size() != read_size_);
    return total_read;
  }
  void archive_read() {
    struct archive_read_deleter { void operator()(struct archive* p) const noexcept { archive_read_free(p); } };
    std::unique_ptr<struct archive, archive_read_deleter> a{archive_read_new()};
    archive_read_support_format_all(a.get());
    archive_read_support_filter_all(a.get());
    archive_read_open_memory(a.get(), buffer_.data(), read_size_);
    struct archive_entry *ae = nullptr;

    REQUIRE(archive_read_next_header(a.get(), &ae) == ARCHIVE_OK);
    const auto size = [&] {
      const auto size = archive_entry_size(ae);
      REQUIRE(size >= 0);
      return gsl::narrow<size_t>(size);
    }();
    archive_buffer_.resize(size);
    archive_read_data(a.get(), archive_buffer_.data(), size);
  }

  std::vector<std::byte> buffer_;
  size_t read_size_ = 0;
  std::vector<std::byte> archive_buffer_;
};

/**
 * There is strong coupling between these compression and decompression
 * tests. Some compression tests also set up the stage for the subsequent
 * decompression test. Each such test controller should either be
 * CompressTestController or a DecompressTestController.
 */
class CompressDecompressionTestController : public TestController {
 protected:
  static std::filesystem::path tempDir_;
  static std::filesystem::path raw_content_path_;
  static std::filesystem::path compressed_content_path_;
  static TestController& get_global_controller() {
    static TestController controller;
    return controller;
  }

  void setupFlow() {
    LogTestController::getInstance().setTrace<minifi::processors::CompressContent>();
    LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<core::ProcessSession>();
    LogTestController::getInstance().setTrace<core::ProcessContext>();
    LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
    LogTestController::getInstance().setTrace<minifi::Connection>();
    LogTestController::getInstance().setTrace<minifi::core::Connectable>();
    LogTestController::getInstance().setTrace<minifi::io::FileStream>();

    std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

    processor = std::make_shared<minifi::processors::CompressContent>("compresscontent");
    processor->initialize();
    utils::Identifier processoruuid = processor->getUUID();
    REQUIRE(processoruuid);

    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(std::make_shared<minifi::ConfigureImpl>());
    // connection from compress processor to success
    output = std::make_shared<minifi::ConnectionImpl>(repo, content_repo, "Output");
    output->addRelationship(core::Relationship("success", "compress successful output"));
    output->setSource(processor.get());
    output->setSourceUUID(processoruuid);
    processor->addConnection(output.get());
    // connection to compress processor
    input = std::make_shared<minifi::ConnectionImpl>(repo, content_repo, "Input");
    input->setDestination(processor.get());
    input->setDestinationUUID(processoruuid);
    processor->addConnection(input.get());

    // connection from compress processor to failure
    failure_output = std::make_shared<minifi::ConnectionImpl>(repo, content_repo, "FailureOutput");
    failure_output->addRelationship(core::Relationship("failure", "compress failure output"));
    failure_output->setSource(processor.get());
    failure_output->setSourceUUID(processoruuid);
    processor->addConnection(failure_output.get());

    processor->incrementActiveTasks();
    processor->setScheduledState(core::ScheduledState::RUNNING);

    context = std::make_shared<core::ProcessContextImpl>(std::make_shared<core::ProcessorNodeImpl>(processor.get()), nullptr, repo, repo, content_repo);
    helper_session = std::make_shared<core::ProcessSessionImpl>(context);
  }

  [[nodiscard]] std::shared_ptr<core::FlowFile> importFlowFile(const std::filesystem::path& content_path) const {
    std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast<core::FlowFile>(helper_session->create());
    helper_session->import(content_path.string(), flow, true, 0);
    helper_session->flushContent();
    input->put(flow);
    return flow;
  }

  template<typename T>
  std::shared_ptr<core::FlowFile> importFlowFileFrom(T&& source) {
    std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast<core::FlowFile>(helper_session->create());
    helper_session->importFrom(std::forward<T>(source), flow);
    helper_session->flushContent();
    input->put(flow);
    return flow;
  }

  void trigger() const {
    auto factory = core::ProcessSessionFactoryImpl(context);
    processor->onSchedule(*context, factory);
    auto session = core::ProcessSessionImpl(context);
    processor->onTrigger(*context, session);
    session.commit();
  }

  void read(const std::shared_ptr<core::FlowFile>& file, ReadCallback& reader) const {
    helper_session->read(file, std::ref(reader));
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

  [[nodiscard]] static std::filesystem::path rawContentPath() {
    return raw_content_path_;
  }

  [[nodiscard]] static std::filesystem::path compressedPath() {
    return compressed_content_path_;
  }

  [[nodiscard]] static RawContent getRawContent() {
    std::ifstream file;
    file.open(raw_content_path_, std::ios::binary);
    std::string contents{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return RawContent{std::move(contents)};
  }

  CompressDecompressionTestController() = default;
  CompressDecompressionTestController(CompressDecompressionTestController&&) = delete;
  CompressDecompressionTestController(const CompressDecompressionTestController&) = delete;
  CompressDecompressionTestController& operator=(CompressDecompressionTestController&&) = delete;
  CompressDecompressionTestController& operator=(const CompressDecompressionTestController&) = delete;
  virtual ~CompressDecompressionTestController();

  std::shared_ptr<core::Processor> processor;
  std::shared_ptr<core::ProcessSession> helper_session;
  std::shared_ptr<core::ProcessContext> context;
  std::shared_ptr<minifi::Connection> output;
  std::shared_ptr<minifi::Connection> failure_output;
  std::shared_ptr<minifi::Connection> input;
};

CompressDecompressionTestController::~CompressDecompressionTestController() {
  LogTestController::getInstance().reset();
}

std::filesystem::path CompressDecompressionTestController::tempDir_;
std::filesystem::path CompressDecompressionTestController::raw_content_path_;
std::filesystem::path CompressDecompressionTestController::compressed_content_path_;

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
    tempDir_ = get_global_controller().createTempDirectory();
    REQUIRE(!tempDir_.empty());
    raw_content_path_ = tempDir_ / "minifi-expect-compresscontent.txt";
    compressed_content_path_ = tempDir_ / "minifi-compresscontent";
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
  DecompressTestController(DecompressTestController&&) = delete;
  DecompressTestController(const DecompressTestController&) = delete;
  DecompressTestController& operator=(DecompressTestController&&) = delete;
  DecompressTestController& operator=(const DecompressTestController&) = delete;
  ~DecompressTestController() override {
    tempDir_ = "";
    raw_content_path_ = "";
    compressed_content_path_ = "";
  }
};

using CompressionFormat = minifi::processors::compress_content::ExtendedCompressionFormat;
using CompressionMode = minifi::processors::compress_content::CompressionMode;

TEST_CASE_METHOD(CompressTestController, "CompressFileGZip", "[compressfiletest1]") {
  context->setProperty(minifi::processors::CompressContent::CompressMode, magic_enum::enum_name(CompressionMode::compress));
  context->setProperty(minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::GZIP));
  context->setProperty(minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(minifi::processors::CompressContent::UpdateFileName, "true");

  auto flow = importFlowFile(rawContentPath());
  flow->setAttribute(core::SpecialFlowAttribute::FILENAME, "inputfile");

  trigger();

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string attribute_value;
    flow1->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, attribute_value);
    REQUIRE(attribute_value == "application/gzip");
    flow1->getAttribute(core::SpecialFlowAttribute::FILENAME, attribute_value);
    REQUIRE(attribute_value == "inputfile.tar.gz");
    ReadCallback callback(gsl::narrow<size_t>(flow1->getSize()));
    read(flow1, callback);
    callback.archive_read();
    std::string content(reinterpret_cast<char *> (callback.archive_buffer_.data()), callback.archive_buffer_.size());
    REQUIRE(getRawContent() == content);
    // write the compress content for next test
    writeCompressed(reinterpret_cast<char *> (callback.buffer_.data()), callback.read_size_);
  }
}

TEST_CASE_METHOD(DecompressTestController, "DecompressFileGZip", "[compressfiletest2]") {
  context->setProperty(minifi::processors::CompressContent::CompressMode, magic_enum::enum_name(CompressionMode::decompress));
  context->setProperty(minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::GZIP));
  context->setProperty(minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(minifi::processors::CompressContent::UpdateFileName, "true");

  auto flow = importFlowFile(compressedPath());
  flow->setAttribute(core::SpecialFlowAttribute::FILENAME, "inputfile.tar.gz");

  trigger();

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string attribute_value;
    REQUIRE_FALSE(flow1->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, attribute_value));
    flow1->getAttribute(core::SpecialFlowAttribute::FILENAME, attribute_value);
    REQUIRE(attribute_value == "inputfile");
    ReadCallback callback(gsl::narrow<size_t>(flow1->getSize()));
    read(flow1, callback);
    std::string content(reinterpret_cast<char *> (callback.buffer_.data()), callback.read_size_);
    REQUIRE(getRawContent() == content);
  }
}

TEST_CASE_METHOD(CompressTestController, "CompressFileBZip", "[compressfiletest3]") {
  context->setProperty(minifi::processors::CompressContent::CompressMode, magic_enum::enum_name(CompressionMode::compress));
  context->setProperty(minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::BZIP2));
  context->setProperty(minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(minifi::processors::CompressContent::UpdateFileName, "true");

  auto flow = importFlowFile(rawContentPath());
  trigger();

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
    read(flow1, callback);
    callback.archive_read();
    std::string contents(reinterpret_cast<char *> (callback.archive_buffer_.data()), callback.archive_buffer_.size());
    REQUIRE(getRawContent() == contents);
    // write the compress content for next test
    writeCompressed(reinterpret_cast<char *> (callback.buffer_.data()), callback.read_size_);
  }
}


TEST_CASE_METHOD(DecompressTestController, "DecompressFileBZip", "[compressfiletest4]") {
  context->setProperty(minifi::processors::CompressContent::CompressMode, magic_enum::enum_name(CompressionMode::decompress));
  context->setProperty(minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::BZIP2));
  context->setProperty(minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(minifi::processors::CompressContent::UpdateFileName, "true");

  auto flow = importFlowFile(compressedPath());

  trigger();

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = output->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    REQUIRE(flow1->getAttribute(core::SpecialFlowAttribute::MIME_TYPE, mime) == false);
    ReadCallback callback(gsl::narrow<size_t>(flow1->getSize()));
    read(flow1, callback);
    std::string contents(reinterpret_cast<char *> (callback.buffer_.data()), callback.read_size_);
    REQUIRE(getRawContent() == contents);
  }
}

TEST_CASE_METHOD(CompressTestController, "CompressFileLZMA", "[compressfiletest5]") {
  context->setProperty(minifi::processors::CompressContent::CompressMode, magic_enum::enum_name(CompressionMode::compress));
  context->setProperty(minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::LZMA));
  context->setProperty(minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(minifi::processors::CompressContent::UpdateFileName, "true");

  auto flow = importFlowFile(rawContentPath());
  trigger();

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
    read(flow1, callback);
    callback.archive_read();
    std::string contents(reinterpret_cast<char *> (callback.archive_buffer_.data()), callback.archive_buffer_.size());
    REQUIRE(getRawContent() == contents);
    // write the compress content for next test
    writeCompressed(reinterpret_cast<char *> (callback.buffer_.data()), callback.read_size_);
  }
}


TEST_CASE_METHOD(DecompressTestController, "DecompressFileLZMA", "[compressfiletest6]") {
  context->setProperty(minifi::processors::CompressContent::CompressMode, magic_enum::enum_name(CompressionMode::decompress));
  context->setProperty(minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::USE_MIME_TYPE));
  context->setProperty(minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(minifi::processors::CompressContent::UpdateFileName, "true");

  auto flow = importFlowFile(compressedPath());
  flow->setAttribute(core::SpecialFlowAttribute::MIME_TYPE, "application/x-lzma");
  trigger();

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
    read(flow1, callback);
    std::string contents(reinterpret_cast<char *> (callback.buffer_.data()), callback.read_size_);
    REQUIRE(getRawContent() == contents);
  }
}

TEST_CASE_METHOD(CompressTestController, "CompressFileXYLZMA", "[compressfiletest7]") {
  context->setProperty(minifi::processors::CompressContent::CompressMode, magic_enum::enum_name(CompressionMode::compress));
  context->setProperty(minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::XZ_LZMA2));
  context->setProperty(minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(minifi::processors::CompressContent::UpdateFileName, "true");

  auto flow = importFlowFile(rawContentPath());
  trigger();

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
    read(flow1, callback);
    callback.archive_read();
    std::string contents(reinterpret_cast<char *> (callback.archive_buffer_.data()), callback.archive_buffer_.size());
    REQUIRE(getRawContent() == contents);
    // write the compress content for next test
    writeCompressed(reinterpret_cast<char *> (callback.buffer_.data()), callback.read_size_);
  }
}


TEST_CASE_METHOD(DecompressTestController, "DecompressFileXYLZMA", "[compressfiletest8]") {
  context->setProperty(minifi::processors::CompressContent::CompressMode, magic_enum::enum_name(CompressionMode::decompress));
  context->setProperty(minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::USE_MIME_TYPE));
  context->setProperty(minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(minifi::processors::CompressContent::UpdateFileName, "true");

  auto flow = importFlowFile(compressedPath());
  flow->setAttribute(core::SpecialFlowAttribute::MIME_TYPE, "application/x-xz");
  trigger();

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
    read(flow1, callback);
    std::string contents(reinterpret_cast<char *> (callback.buffer_.data()), callback.read_size_);
    REQUIRE(getRawContent() == contents);
  }
}

TEST_CASE_METHOD(TestController, "RawGzipCompressionDecompression", "[compressfiletest8]") {
  LogTestController::getInstance().setTrace<minifi::processors::CompressContent>();
  LogTestController::getInstance().setTrace<minifi::processors::PutFile>();

  // Create temporary directories
  auto src_dir = createTempDirectory();
  REQUIRE(!src_dir.empty());

  auto dst_dir = createTempDirectory();
  REQUIRE(!dst_dir.empty());

  // Define files
  auto src_file = src_dir / "src.txt";
  auto compressed_file = dst_dir / "src.txt.gz";
  auto decompressed_file = dst_dir / "src.txt";

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
  plan->setProperty(get_file, minifi::processors::GetFile::Directory, src_dir.string());

  // Configure CompressContent processor for compression
  plan->setProperty(compress_content, minifi::processors::CompressContent::CompressMode, magic_enum::enum_name(CompressionMode::compress));
  plan->setProperty(compress_content, minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::GZIP));
  plan->setProperty(compress_content, minifi::processors::CompressContent::UpdateFileName, "true");
  plan->setProperty(compress_content, minifi::processors::CompressContent::EncapsulateInTar, "false");

  // Configure first PutFile processor
  plan->setProperty(put_compressed, minifi::processors::PutFile::Directory, dst_dir.string());

  // Configure CompressContent processor for decompression
  plan->setProperty(decompress_content, minifi::processors::CompressContent::CompressMode, magic_enum::enum_name(CompressionMode::decompress));
  plan->setProperty(decompress_content, minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::GZIP));
  plan->setProperty(decompress_content, minifi::processors::CompressContent::UpdateFileName, "true");
  plan->setProperty(decompress_content, minifi::processors::CompressContent::EncapsulateInTar, "false");

  // Configure second PutFile processor
  plan->setProperty(put_decompressed, minifi::processors::PutFile::Directory, dst_dir.string());

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
    utils::string::repeat("0", 1000), utils::string::repeat("1", 1000),
    utils::string::repeat("2", 1000), utils::string::repeat("3", 1000),
  };
  const std::size_t batchSize = 3;

  context->setProperty(minifi::processors::CompressContent::CompressMode, magic_enum::enum_name(CompressionMode::compress));
  context->setProperty(minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::GZIP));
  context->setProperty(minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(minifi::processors::CompressContent::UpdateFileName, "true");
  context->setProperty(minifi::processors::CompressContent::BatchSize, std::to_string(batchSize));

  for (const auto& content : flowFileContents) {
    importFlowFileFrom(minifi::io::BufferStream(content));
  }

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactoryImpl>(context);
  processor->onSchedule(*context, *factory);

  // Trigger once to process batchSize
  {
    auto session = std::make_shared<core::ProcessSessionImpl>(context);
    processor->onTrigger(*context, *session);
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
    auto session = std::make_unique<core::ProcessSessionImpl>(context);
    processor->onTrigger(*context, *session);
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
    read(file, callback);
    callback.archive_read();
    std::string content(reinterpret_cast<char *> (callback.archive_buffer_.data()), callback.archive_buffer_.size());
    REQUIRE(flowFileContents[idx] == content);
  }
}

TEST_CASE_METHOD(DecompressTestController, "Invalid archive decompression", "[compressfiletest9]") {
  context->setProperty(minifi::processors::CompressContent::CompressMode, magic_enum::enum_name(CompressionMode::decompress));
  SECTION("GZIP") {
    context->setProperty(minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::GZIP));
  }
  SECTION("LZMA") {
    context->setProperty(minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::LZMA));
  }
  SECTION("XZ_LZMA2") {
    context->setProperty(minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::XZ_LZMA2));
  }
  SECTION("BZIP2") {
    context->setProperty(minifi::processors::CompressContent::CompressFormat, magic_enum::enum_name(CompressionFormat::BZIP2));
  }
  context->setProperty(minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(minifi::processors::CompressContent::UpdateFileName, "true");

  importFlowFileFrom(minifi::io::BufferStream(std::string{"banana bread"}));
  trigger();

  if (LogTestController::getInstance().contains("compression not supported on this platform")) {
    return;
  }

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  REQUIRE_FALSE(output->poll(expiredFlowRecords));
  REQUIRE(expiredFlowRecords.empty());

  auto invalid_flow = failure_output->poll(expiredFlowRecords);
  REQUIRE(invalid_flow);
  REQUIRE(expiredFlowRecords.empty());
  {
    ReadCallback callback(gsl::narrow<size_t>(invalid_flow->getSize()));
    read(invalid_flow, callback);
    std::string contents(reinterpret_cast<char *> (callback.buffer_.data()), callback.read_size_);
    REQUIRE(contents == "banana bread");
  }
}
