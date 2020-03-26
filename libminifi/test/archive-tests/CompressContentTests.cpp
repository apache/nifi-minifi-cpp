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

static const char* EXPECT_COMPRESS_CONTENT = "/tmp/minifi-expect-compresscontent.txt";
static const char* COMPRESS_CONTENT = "/tmp/minifi-compresscontent";

class ReadCallback: public org::apache::nifi::minifi::InputStreamCallback {
 public:
  explicit ReadCallback(uint64_t size) :
      read_size_(0) {
    buffer_size_ = size;
    buffer_ = new uint8_t[buffer_size_];
    archive_buffer_ = nullptr;
  }
  ~ReadCallback() {
    if (buffer_)
      delete[] buffer_;
    if (archive_buffer_)
      delete[] archive_buffer_;
  }
  int64_t process(std::shared_ptr<org::apache::nifi::minifi::io::BaseStream> stream) {
    int64_t ret = 0;
    ret = stream->read(buffer_, buffer_size_);
    if (stream)
      read_size_ = stream->getSize();
    else
      read_size_ = buffer_size_;
    return ret;
  }
  void archive_read() {
    struct archive *a;
    a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    archive_read_open_memory(a, buffer_, read_size_);
    struct archive_entry *ae;

    if (archive_read_next_header(a, &ae) == ARCHIVE_OK) {
      int size = archive_entry_size(ae);
      archive_buffer_ = new char[size];
      archive_buffer_size_ = size;
      archive_read_data(a, archive_buffer_, size);
    }
    archive_read_free(a);
  }

  uint8_t *buffer_;
  uint64_t buffer_size_;
  uint64_t read_size_;
  char *archive_buffer_;
  int archive_buffer_size_;
};

TEST_CASE("CompressFileGZip", "[compressfiletest1]") {
  try {
    std::ofstream expectfile;
    expectfile.open(EXPECT_COMPRESS_CONTENT);

    std::mt19937 gen(std::random_device { }());
    for (int i = 0; i < 100000; i++) {
      expectfile << std::to_string(gen() % 100);
    }
    expectfile.close();

    TestController testController;
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
    sessionGenFlowFile.import(EXPECT_COMPRESS_CONTENT, flow, true, 0);
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
      std::string flowFileName = std::string(EXPECT_COMPRESS_CONTENT);
      std::ifstream file1;
      file1.open(flowFileName, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.archive_buffer_), callback.archive_buffer_size_);
      REQUIRE(expectContents == contents);
      // write the compress content for next test
      std::ofstream file(COMPRESS_CONTENT);
      file.write(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      file.close();
      file1.close();
    }
    LogTestController::getInstance().reset();
  } catch (...) {
  }
}

TEST_CASE("DecompressFileGZip", "[compressfiletest2]") {
  try {
    TestController testController;
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
    sessionGenFlowFile.import(COMPRESS_CONTENT, flow, true, 0);
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
      std::string flowFileName = std::string(EXPECT_COMPRESS_CONTENT);
      std::ifstream file1;
      file1.open(flowFileName, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      REQUIRE(expectContents == contents);
      file1.close();
    }
    LogTestController::getInstance().reset();
    unlink(COMPRESS_CONTENT);
    unlink(EXPECT_COMPRESS_CONTENT);
  } catch (...) {
  }
}

TEST_CASE("CompressFileBZip", "[compressfiletest3]") {
  try {
    std::ofstream expectfile;
    expectfile.open(EXPECT_COMPRESS_CONTENT);

    std::mt19937 gen(std::random_device { }());
    for (int i = 0; i < 100000; i++) {
      expectfile << std::to_string(gen() % 100);
    }
    expectfile.close();

    TestController testController;
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
    sessionGenFlowFile.import(EXPECT_COMPRESS_CONTENT, flow, true, 0);
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
      std::string flowFileName = std::string(EXPECT_COMPRESS_CONTENT);
      std::ifstream file1;
      file1.open(flowFileName, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.archive_buffer_), callback.archive_buffer_size_);
      REQUIRE(expectContents == contents);
      // write the compress content for next test
      std::ofstream file(COMPRESS_CONTENT);
      file.write(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      file.close();
      file1.close();
    }
    LogTestController::getInstance().reset();
  } catch (...) {
  }
}


TEST_CASE("DecompressFileBZip", "[compressfiletest4]") {
  try {
    TestController testController;
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
    sessionGenFlowFile.import(COMPRESS_CONTENT, flow, true, 0);
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
      std::string flowFileName = std::string(EXPECT_COMPRESS_CONTENT);
      std::ifstream file1;
      file1.open(flowFileName, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      REQUIRE(expectContents == contents);
      file1.close();
    }
    LogTestController::getInstance().reset();
    unlink(COMPRESS_CONTENT);
    unlink(EXPECT_COMPRESS_CONTENT);
  } catch (...) {
  }
}

TEST_CASE("CompressFileLZMA", "[compressfiletest5]") {
  try {
    std::ofstream expectfile;
    expectfile.open(EXPECT_COMPRESS_CONTENT);

    std::mt19937 gen(std::random_device { }());
    for (int i = 0; i < 100000; i++) {
      expectfile << std::to_string(gen() % 100);
    }
    expectfile.close();

    TestController testController;
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
    sessionGenFlowFile.import(EXPECT_COMPRESS_CONTENT, flow, true, 0);
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
      std::string flowFileName = std::string(EXPECT_COMPRESS_CONTENT);
      std::ifstream file1;
      file1.open(flowFileName, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.archive_buffer_), callback.archive_buffer_size_);
      REQUIRE(expectContents == contents);
      // write the compress content for next test
      std::ofstream file(COMPRESS_CONTENT);
      file.write(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      file.close();
      file1.close();
    }
    LogTestController::getInstance().reset();
  } catch (...) {
  }
}


TEST_CASE("DecompressFileLZMA", "[compressfiletest6]") {
  try {
    TestController testController;
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
    sessionGenFlowFile.import(COMPRESS_CONTENT, flow, true, 0);
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
      std::string flowFileName = std::string(EXPECT_COMPRESS_CONTENT);
      std::ifstream file1;
      file1.open(flowFileName, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      REQUIRE(expectContents == contents);
      file1.close();
    }
    LogTestController::getInstance().reset();
    unlink(COMPRESS_CONTENT);
    unlink(EXPECT_COMPRESS_CONTENT);
  } catch (...) {
  }
}

TEST_CASE("CompressFileXYLZMA", "[compressfiletest7]") {
  try {
    std::ofstream expectfile;
    expectfile.open(EXPECT_COMPRESS_CONTENT);

    std::mt19937 gen(std::random_device { }());
    for (int i = 0; i < 100000; i++) {
      expectfile << std::to_string(gen() % 100);
    }
    expectfile.close();

    TestController testController;
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
    sessionGenFlowFile.import(EXPECT_COMPRESS_CONTENT, flow, true, 0);
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
      std::string flowFileName = std::string(EXPECT_COMPRESS_CONTENT);
      std::ifstream file1;
      file1.open(flowFileName, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.archive_buffer_), callback.archive_buffer_size_);
      REQUIRE(expectContents == contents);
      // write the compress content for next test
      std::ofstream file(COMPRESS_CONTENT);
      file.write(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      file.close();
      file1.close();
    }
    LogTestController::getInstance().reset();
  } catch (...) {
  }
}


TEST_CASE("DecompressFileXYLZMA", "[compressfiletest8]") {
  try {
    TestController testController;
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
    sessionGenFlowFile.import(COMPRESS_CONTENT, flow, true, 0);
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
      std::string flowFileName = std::string(EXPECT_COMPRESS_CONTENT);
      std::ifstream file1;
      file1.open(flowFileName, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      REQUIRE(expectContents == contents);
      file1.close();
    }
    LogTestController::getInstance().reset();
    unlink(COMPRESS_CONTENT);
    unlink(EXPECT_COMPRESS_CONTENT);
  } catch (...) {
  }
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

  std::fstream file;
  file.open(src_file, std::ios::out);
  file << content;
  file.close();

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
