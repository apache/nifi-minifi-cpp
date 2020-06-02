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

class ReadCallback: public org::apache::nifi::minifi::InputStreamCallback {
 public:
  explicit ReadCallback(uint64_t size) :
      read_size_(0) {
    buffer_size_ = size;
    buffer_ = new uint8_t[buffer_size_];
    archive_buffer_ = nullptr;
    archive_buffer_size_ = 0;
  }
  ~ReadCallback() {
    if (buffer_)
      delete[] buffer_;
    if (archive_buffer_)
      delete[] archive_buffer_;
  }
  int64_t process(std::shared_ptr<org::apache::nifi::minifi::io::BaseStream> stream) {
    int64_t total_read = 0;
    int64_t ret = 0;
    do {
      ret = stream->read(buffer_ + read_size_, buffer_size_ - read_size_);
      if (ret == 0) break;
      if (ret < 0) return ret;
      read_size_ += ret;
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
    int size = archive_entry_size(ae);
    archive_buffer_ = new char[size];
    archive_buffer_size_ = size;
    archive_read_data(a, archive_buffer_, size);
    archive_read_free(a);
  }

  uint8_t *buffer_;
  uint64_t buffer_size_;
  uint64_t read_size_;
  char *archive_buffer_;
  int archive_buffer_size_;
};

/**
 * There is strong coupling between these compression and decompression
 * tests. Some compression tests also set up the stage for the subsequent
 * decompression test. Each such test controller should either be
 * CompressTestController or a DecompressTestController.
 */
class CompressDecompressionTestController : public TestController{
 protected:
  static std::string tempDir;
  static std::string raw_content_path;
  static std::string compressed_content_path;
  static TestController& get_global_controller() {
    static TestController controller;
    return controller;
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
    return raw_content_path;
  }

  std::string compressedPath() const {
    return compressed_content_path;
  }

  RawContent getRawContent() const {
    std::ifstream file;
    file.open(raw_content_path, std::ios::binary);
    std::string contents{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return RawContent{std::move(contents)};
  }

  virtual ~CompressDecompressionTestController() = 0;
};

CompressDecompressionTestController::~CompressDecompressionTestController() = default;

std::string CompressDecompressionTestController::tempDir;
std::string CompressDecompressionTestController::raw_content_path;
std::string CompressDecompressionTestController::compressed_content_path;

class CompressTestController : public CompressDecompressionTestController {
  static void initContentWithRandomData() {
    int random_seed = 0x454;
    std::ofstream file;
    file.open(raw_content_path, std::ios::binary);

    std::mt19937 gen(random_seed);
    for (int i = 0; i < 100000; i++) {
      file << std::to_string(gen() % 100);
    }
  }

 public:
  CompressTestController() {
    char format[] = "/tmp/test.XXXXXX";
    tempDir = get_global_controller().createTempDirectory(format);
    REQUIRE(!tempDir.empty());
    raw_content_path = utils::file::FileUtils::concat_path(tempDir, "minifi-expect-compresscontent.txt");
    compressed_content_path = utils::file::FileUtils::concat_path(tempDir, "minifi-compresscontent");
    initContentWithRandomData();
  }

  template<class ...Args>
  void writeCompressed(Args&& ...args){
    std::ofstream file(compressed_content_path, std::ios::binary);
    file.write(std::forward<Args>(args)...);
  }
};

class DecompressTestController : public CompressDecompressionTestController{
 public:
  ~DecompressTestController(){
    tempDir = "";
    raw_content_path = "";
    compressed_content_path = "";
  }
};

TEST_CASE("CompressFileGZip", "[compressfiletest1]") {
  CompressTestController testController;

  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::CompressContent>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();
  LogTestController::getInstance().setTrace<core::ProcessContext>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::CompressContent>("compresscontent");
  std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
  processor->initialize();
  utils::Identifier processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));
  utils::Identifier logAttributeuuid;
  REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  // std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();

  content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
  // connection from compress processor to log attribute
  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
  connection->addRelationship(core::Relationship("success", "compress successful output"));
  connection->setSource(processor);
  connection->setDestination(logAttributeProcessor);
  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(logAttributeuuid);
  processor->addConnection(connection);
  // connection to compress processor
  std::shared_ptr<minifi::Connection> compressconnection = std::make_shared<minifi::Connection>(repo, content_repo, "compressconnection");
  compressconnection->setDestination(processor);
  compressconnection->setDestinationUUID(processoruuid);
  processor->addConnection(compressconnection);

  std::set<core::Relationship> autoTerminatedRelationships;
  core::Relationship failure("failure", "");
  autoTerminatedRelationships.insert(failure);
  processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  logAttributeProcessor->incrementActiveTasks();
  logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
  auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressMode, MODE_COMPRESS);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressFormat, COMPRESSION_FORMAT_GZIP);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
  std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(testController.rawContentPath(), flow, true, 0);
  income_connection->put(flow);

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  auto session = std::make_shared<core::ProcessSession>(context);
  processor->onTrigger(context, session);
  session->commit();

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    flow1->getAttribute(FlowAttributeKey(org::apache::nifi::minifi::MIME_TYPE), mime);
    REQUIRE(mime == "application/gzip");
    ReadCallback callback(flow1->getSize());
    sessionGenFlowFile.read(flow1, &callback);
    callback.archive_read();
    std::string content(reinterpret_cast<char *> (callback.archive_buffer_), callback.archive_buffer_size_);
    REQUIRE(testController.getRawContent() == content);
    // write the compress content for next test
    testController.writeCompressed(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
  }
  LogTestController::getInstance().reset();
}

TEST_CASE("DecompressFileGZip", "[compressfiletest2]") {
  DecompressTestController testController;
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::CompressContent>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();
  LogTestController::getInstance().setTrace<core::ProcessContext>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  // LogTestController::getInstance().setTrace<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::io::FileStream>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::CompressContent>("compresscontent");
  std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
  processor->initialize();
  utils::Identifier processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));
  utils::Identifier logAttributeuuid;
  REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  // std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();
  content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
  // connection from compress processor to log attribute
  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
  connection->addRelationship(core::Relationship("success", "compress successful output"));
  connection->setSource(processor);
  connection->setDestination(logAttributeProcessor);
  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(logAttributeuuid);
  processor->addConnection(connection);
  // connection to compress processor
  std::shared_ptr<minifi::Connection> compressconnection = std::make_shared<minifi::Connection>(repo, content_repo, "compressconnection");
  compressconnection->setDestination(processor);
  compressconnection->setDestinationUUID(processoruuid);
  processor->addConnection(compressconnection);

  std::set<core::Relationship> autoTerminatedRelationships;
  core::Relationship failure("failure", "");
  autoTerminatedRelationships.insert(failure);
  processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  logAttributeProcessor->incrementActiveTasks();
  logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
  auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressMode, MODE_DECOMPRESS);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressFormat, COMPRESSION_FORMAT_GZIP);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
  std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(testController.compressedPath(), flow, true, 0);
  income_connection->put(flow);

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  auto session = std::make_shared<core::ProcessSession>(context);
  processor->onTrigger(context, session);
  session->commit();

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    REQUIRE(flow1->getAttribute(FlowAttributeKey(org::apache::nifi::minifi::MIME_TYPE), mime) == false);
    ReadCallback callback(flow1->getSize());
    sessionGenFlowFile.read(flow1, &callback);
    std::string content(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
    REQUIRE(testController.getRawContent() == content);
  }
  LogTestController::getInstance().reset();
}

TEST_CASE("CompressFileBZip", "[compressfiletest3]") {
  CompressTestController testController;

  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::CompressContent>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::CompressContent>("compresscontent");
  std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
  processor->initialize();
  utils::Identifier processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));
  utils::Identifier logAttributeuuid;
  REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  // std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();

  content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
  // connection from compress processor to log attribute
  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
  connection->addRelationship(core::Relationship("success", "compress successful output"));
  connection->setSource(processor);
  connection->setDestination(logAttributeProcessor);
  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(logAttributeuuid);
  processor->addConnection(connection);
  // connection to compress processor
  std::shared_ptr<minifi::Connection> compressconnection = std::make_shared<minifi::Connection>(repo, content_repo, "compressconnection");
  compressconnection->setDestination(processor);
  compressconnection->setDestinationUUID(processoruuid);
  processor->addConnection(compressconnection);

  std::set<core::Relationship> autoTerminatedRelationships;
  core::Relationship failure("failure", "");
  autoTerminatedRelationships.insert(failure);
  processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  logAttributeProcessor->incrementActiveTasks();
  logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
  auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressMode, MODE_COMPRESS);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressFormat, COMPRESSION_FORMAT_BZIP2);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
  std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(testController.rawContentPath(), flow, true, 0);
  income_connection->put(flow);

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  auto session = std::make_shared<core::ProcessSession>(context);
  processor->onTrigger(context, session);
  session->commit();

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    flow1->getAttribute(FlowAttributeKey(org::apache::nifi::minifi::MIME_TYPE), mime);
    REQUIRE(mime == "application/bzip2");
    ReadCallback callback(flow1->getSize());
    sessionGenFlowFile.read(flow1, &callback);
    callback.archive_read();
    std::string contents(reinterpret_cast<char *> (callback.archive_buffer_), callback.archive_buffer_size_);
    REQUIRE(testController.getRawContent() == contents);
    // write the compress content for next test
    testController.writeCompressed(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
  }
  LogTestController::getInstance().reset();
}


TEST_CASE("DecompressFileBZip", "[compressfiletest4]") {
  DecompressTestController testController;

  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::CompressContent>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  // LogTestController::getInstance().setTrace<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::io::FileStream>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::CompressContent>("compresscontent");
  std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
  processor->initialize();
  utils::Identifier processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));
  utils::Identifier logAttributeuuid;
  REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  // std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();
  content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
  // connection from compress processor to log attribute
  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
  connection->addRelationship(core::Relationship("success", "compress successful output"));
  connection->setSource(processor);
  connection->setDestination(logAttributeProcessor);
  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(logAttributeuuid);
  processor->addConnection(connection);
  // connection to compress processor
  std::shared_ptr<minifi::Connection> compressconnection = std::make_shared<minifi::Connection>(repo, content_repo, "compressconnection");
  compressconnection->setDestination(processor);
  compressconnection->setDestinationUUID(processoruuid);
  processor->addConnection(compressconnection);

  std::set<core::Relationship> autoTerminatedRelationships;
  core::Relationship failure("failure", "");
  autoTerminatedRelationships.insert(failure);
  processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  logAttributeProcessor->incrementActiveTasks();
  logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
  auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressMode, MODE_DECOMPRESS);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressFormat, COMPRESSION_FORMAT_BZIP2);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
  std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(testController.compressedPath(), flow, true, 0);
  income_connection->put(flow);

  REQUIRE(processor->getName() == "compresscontent");
  auto factory = std::make_shared<core::ProcessSessionFactory>(context);
  processor->onSchedule(context, factory);
  auto session = std::make_shared<core::ProcessSession>(context);
  processor->onTrigger(context, session);
  session->commit();

  // validate the compress content
  std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
  std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    REQUIRE(flow1->getAttribute(FlowAttributeKey(org::apache::nifi::minifi::MIME_TYPE), mime) == false);
    ReadCallback callback(flow1->getSize());
    sessionGenFlowFile.read(flow1, &callback);
    std::string contents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
    REQUIRE(testController.getRawContent() == contents);
  }
  LogTestController::getInstance().reset();
}

TEST_CASE("CompressFileLZMA", "[compressfiletest5]") {
  CompressTestController testController;

  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::CompressContent>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::CompressContent>("compresscontent");
  std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
  processor->initialize();
  utils::Identifier processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));
  utils::Identifier logAttributeuuid;
  REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  // std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();

  content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
  // connection from compress processor to log attribute
  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
  connection->addRelationship(core::Relationship("success", "compress successful output"));
  connection->setSource(processor);
  connection->setDestination(logAttributeProcessor);
  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(logAttributeuuid);
  processor->addConnection(connection);
  // connection to compress processor
  std::shared_ptr<minifi::Connection> compressconnection = std::make_shared<minifi::Connection>(repo, content_repo, "compressconnection");
  compressconnection->setDestination(processor);
  compressconnection->setDestinationUUID(processoruuid);
  processor->addConnection(compressconnection);

  std::set<core::Relationship> autoTerminatedRelationships;
  core::Relationship failure("failure", "");
  autoTerminatedRelationships.insert(failure);
  processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  logAttributeProcessor->incrementActiveTasks();
  logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
  auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressMode, MODE_COMPRESS);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressFormat, COMPRESSION_FORMAT_LZMA);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
  std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(testController.rawContentPath(), flow, true, 0);
  income_connection->put(flow);

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
  std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    flow1->getAttribute(FlowAttributeKey(org::apache::nifi::minifi::MIME_TYPE), mime);
    REQUIRE(mime == "application/x-lzma");
    ReadCallback callback(flow1->getSize());
    sessionGenFlowFile.read(flow1, &callback);
    callback.archive_read();
    std::string contents(reinterpret_cast<char *> (callback.archive_buffer_), callback.archive_buffer_size_);
    REQUIRE(testController.getRawContent() == contents);
    // write the compress content for next test
    testController.writeCompressed(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
  }
  LogTestController::getInstance().reset();
}


TEST_CASE("DecompressFileLZMA", "[compressfiletest6]") {
  DecompressTestController testController;

  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::CompressContent>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  // LogTestController::getInstance().setTrace<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::io::FileStream>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::CompressContent>("compresscontent");
  std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
  processor->initialize();
  utils::Identifier processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));
  utils::Identifier logAttributeuuid;
  REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  // std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();
  content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
  // connection from compress processor to log attribute
  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
  connection->addRelationship(core::Relationship("success", "compress successful output"));
  connection->setSource(processor);
  connection->setDestination(logAttributeProcessor);
  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(logAttributeuuid);
  processor->addConnection(connection);
  // connection to compress processor
  std::shared_ptr<minifi::Connection> compressconnection = std::make_shared<minifi::Connection>(repo, content_repo, "compressconnection");
  compressconnection->setDestination(processor);
  compressconnection->setDestinationUUID(processoruuid);
  processor->addConnection(compressconnection);

  std::set<core::Relationship> autoTerminatedRelationships;
  core::Relationship failure("failure", "");
  autoTerminatedRelationships.insert(failure);
  processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  logAttributeProcessor->incrementActiveTasks();
  logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
  auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressMode, MODE_DECOMPRESS);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressFormat, COMPRESSION_FORMAT_ATTRIBUTE);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
  std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(testController.compressedPath(), flow, true, 0);
  flow->setAttribute(FlowAttributeKey(org::apache::nifi::minifi::MIME_TYPE), "application/x-lzma");
  income_connection->put(flow);

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
  std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    REQUIRE(flow1->getAttribute(FlowAttributeKey(org::apache::nifi::minifi::MIME_TYPE), mime) == false);
    ReadCallback callback(flow1->getSize());
    sessionGenFlowFile.read(flow1, &callback);
    std::string contents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
    REQUIRE(testController.getRawContent() == contents);
  }
  LogTestController::getInstance().reset();
}

TEST_CASE("CompressFileXYLZMA", "[compressfiletest7]") {
  CompressTestController testController;

  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::CompressContent>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::CompressContent>("compresscontent");
  std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
  processor->initialize();
  utils::Identifier processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));
  utils::Identifier logAttributeuuid;
  REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  // std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();

  content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
  // connection from compress processor to log attribute
  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
  connection->addRelationship(core::Relationship("success", "compress successful output"));
  connection->setSource(processor);
  connection->setDestination(logAttributeProcessor);
  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(logAttributeuuid);
  processor->addConnection(connection);
  // connection to compress processor
  std::shared_ptr<minifi::Connection> compressconnection = std::make_shared<minifi::Connection>(repo, content_repo, "compressconnection");
  compressconnection->setDestination(processor);
  compressconnection->setDestinationUUID(processoruuid);
  processor->addConnection(compressconnection);

  std::set<core::Relationship> autoTerminatedRelationships;
  core::Relationship failure("failure", "");
  autoTerminatedRelationships.insert(failure);
  processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  logAttributeProcessor->incrementActiveTasks();
  logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
  auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressMode, MODE_COMPRESS);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressFormat, COMPRESSION_FORMAT_XZ_LZMA2);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
  std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(testController.rawContentPath(), flow, true, 0);
  income_connection->put(flow);

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
  std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    flow1->getAttribute(FlowAttributeKey(org::apache::nifi::minifi::MIME_TYPE), mime);
    REQUIRE(mime == "application/x-xz");
    ReadCallback callback(flow1->getSize());
    sessionGenFlowFile.read(flow1, &callback);
    callback.archive_read();
    std::string contents(reinterpret_cast<char *> (callback.archive_buffer_), callback.archive_buffer_size_);
    REQUIRE(testController.getRawContent() == contents);
    // write the compress content for next test
    testController.writeCompressed(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
  }
  LogTestController::getInstance().reset();
}


TEST_CASE("DecompressFileXYLZMA", "[compressfiletest8]") {
  DecompressTestController testController;

  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::CompressContent>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  // LogTestController::getInstance().setTrace<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::io::FileStream>();

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::CompressContent>("compresscontent");
  std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
  processor->initialize();
  utils::Identifier processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));
  utils::Identifier logAttributeuuid;
  REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  // std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();
  content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
  // connection from compress processor to log attribute
  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
  connection->addRelationship(core::Relationship("success", "compress successful output"));
  connection->setSource(processor);
  connection->setDestination(logAttributeProcessor);
  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(logAttributeuuid);
  processor->addConnection(connection);
  // connection to compress processor
  std::shared_ptr<minifi::Connection> compressconnection = std::make_shared<minifi::Connection>(repo, content_repo, "compressconnection");
  compressconnection->setDestination(processor);
  compressconnection->setDestinationUUID(processoruuid);
  processor->addConnection(compressconnection);

  std::set<core::Relationship> autoTerminatedRelationships;
  core::Relationship failure("failure", "");
  autoTerminatedRelationships.insert(failure);
  processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  logAttributeProcessor->incrementActiveTasks();
  logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
  auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressMode, MODE_DECOMPRESS);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressFormat, COMPRESSION_FORMAT_ATTRIBUTE);
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::CompressLevel, "9");
  context->setProperty(org::apache::nifi::minifi::processors::CompressContent::UpdateFileName, "true");

  core::ProcessSession sessionGenFlowFile(context);
  std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
  std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
  std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
  sessionGenFlowFile.import(testController.compressedPath(), flow, true, 0);
  flow->setAttribute(FlowAttributeKey(org::apache::nifi::minifi::MIME_TYPE), "application/x-xz");
  income_connection->put(flow);

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
  std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
  REQUIRE(flow1->getSize() > 0);
  {
    REQUIRE(flow1->getSize() != flow->getSize());
    std::string mime;
    REQUIRE(flow1->getAttribute(FlowAttributeKey(org::apache::nifi::minifi::MIME_TYPE), mime) == false);
    ReadCallback callback(flow1->getSize());
    sessionGenFlowFile.read(flow1, &callback);
    std::string contents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
    REQUIRE(testController.getRawContent() == contents);
  }
  LogTestController::getInstance().reset();
}

TEST_CASE("RawGzipCompressionDecompression", "[compressfiletest8]") {
  TestController testController;
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::CompressContent>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::PutFile>();

  // Create temporary directories
  char format_src[] = "/tmp/archives.XXXXXX";
  std::string src_dir = testController.createTempDirectory(format_src);
  REQUIRE(!src_dir.empty());

  char format_dst[] = "/tmp/archived.XXXXXX";
  std::string dst_dir = testController.createTempDirectory(format_dst);
  REQUIRE(!dst_dir.empty());

  // Define files
  std::string src_file = utils::file::FileUtils::concat_path(src_dir, "src.txt");
  std::string compressed_file = utils::file::FileUtils::concat_path(dst_dir, "src.txt.gz");
  std::string decompressed_file = utils::file::FileUtils::concat_path(dst_dir, "src.txt");

  // Build MiNiFi processing graph
  auto plan = testController.createPlan();
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
  plan->setProperty(compress_content, "Mode", MODE_COMPRESS);
  plan->setProperty(compress_content, "Compression Format", COMPRESSION_FORMAT_GZIP);
  plan->setProperty(compress_content, "Update Filename", "true");
  plan->setProperty(compress_content, "Encapsulate in TAR", "false");

  // Configure first PutFile processor
  plan->setProperty(put_compressed, "Directory", dst_dir);

  // Configure CompressContent processor for decompression
  plan->setProperty(decompress_content, "Mode", MODE_DECOMPRESS);
  plan->setProperty(decompress_content, "Compression Format", COMPRESSION_FORMAT_GZIP);
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
    for (size_t i = 0U; i < 1024 * 1024U; i++) {
      content_ss << "foobar";
    }
    content = content_ss.str();
  }

  std::ofstream{ src_file } << content;

  // Run flow
  testController.runSession(plan, true);

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
