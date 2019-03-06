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
#include "FlowController.h"
#include "../TestBase.h"
#include "core/Core.h"
#include "../../include/core/FlowFile.h"
#include "../unit/ProvenanceTestHelper.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "MergeContent.h"
#include <sstream>
#include <iostream>
#include "processors/LogAttribute.h"

static const char* FLOW_FILE = "/tmp/minifi-mergecontent";
static const char* EXPECT_MERGE_CONTENT_FIRST = "/tmp/minifi-expect-mergecontent1.txt";
static const char* EXPECT_MERGE_CONTENT_SECOND = "/tmp/minifi-expect-mergecontent2.txt";
static const char* HEADER_FILE = "/tmp/minifi-mergecontent.header";
static const char* FOOTER_FILE = "/tmp/minifi-mergecontent.footer";
static const char* DEMARCATOR_FILE = "/tmp/minifi-mergecontent.demarcator";

class ReadCallback: public org::apache::nifi::minifi::InputStreamCallback {
 public:
  explicit ReadCallback(uint64_t size) :
      read_size_(0) {
    buffer_size_ = size;
    buffer_ = new uint8_t[buffer_size_];
    archive_buffer_num_ = 0;
  }
  ~ReadCallback() {
    if (buffer_)
      delete[] buffer_;
    for (int i = 0; i < archive_buffer_num_; i++) {
      delete[] archive_buffer_[i];
    }
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

    while (archive_read_next_header(a, &ae) == ARCHIVE_OK && archive_buffer_num_ < 10) {
      int size = archive_entry_size(ae);
      archive_buffer_[archive_buffer_num_] = new char[size];
      archive_buffer_size_[archive_buffer_num_] = size;
      archive_read_data(a, archive_buffer_[archive_buffer_num_], size);
      archive_buffer_num_++;
    }
  }

  uint8_t *buffer_;
  uint64_t buffer_size_;
  uint64_t read_size_;
  char *archive_buffer_[10];
  int archive_buffer_size_[10];
  int archive_buffer_num_;
};

TEST_CASE("MergeFileDefragment", "[mergefiletest1]") {
  try {
    std::ofstream expectfileFirst;
    std::ofstream expectfileSecond;

    expectfileFirst.open(EXPECT_MERGE_CONTENT_FIRST);
    expectfileSecond.open(EXPECT_MERGE_CONTENT_SECOND);

    // Create and write to the test file
    for (int i = 0; i < 6; i++) {
      std::ofstream tmpfile;
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      tmpfile.open(flowFileName.c_str());
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        if (i < 3)
          expectfileFirst << std::to_string(i);
        else
          expectfileSecond << std::to_string(i);
      }
      tmpfile.close();
    }
    expectfileFirst.close();
    expectfileSecond.close();

    TestController testController;
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::MergeContent>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<core::ProcessSession>();
    LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::BinFiles>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::Bin>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::BinManager>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();

    std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

    std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::MergeContent>("mergecontent");
    std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
    processor->initialize();
    utils::Identifier processoruuid;
    REQUIRE(true == processor->getUUID(processoruuid));
    utils::Identifier logAttributeuuid;
    REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
    // connection from merge processor to log attribute
    std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
    connection->addRelationship(core::Relationship("merged", "Merge successful output"));
    connection->setSource(processor);
    connection->setDestination(logAttributeProcessor);
    connection->setSourceUUID(processoruuid);
    connection->setDestinationUUID(logAttributeuuid);
    processor->addConnection(connection);
    // connection to merge processor
    std::shared_ptr<minifi::Connection> mergeconnection = std::make_shared<minifi::Connection>(repo, content_repo, "mergeconnection");
    mergeconnection->setDestination(processor);
    mergeconnection->setDestinationUUID(processoruuid);
    processor->addConnection(mergeconnection);

    std::set<core::Relationship> autoTerminatedRelationships;
    core::Relationship original("original", "");
    core::Relationship failure("failure", "");
    autoTerminatedRelationships.insert(original);
    autoTerminatedRelationships.insert(failure);
    processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

    processor->incrementActiveTasks();
    processor->setScheduledState(core::ScheduledState::RUNNING);
    logAttributeProcessor->incrementActiveTasks();
    logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

    std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
    std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
    auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, MERGE_FORMAT_CONCAT_VALUE);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, MERGE_STRATEGY_DEFRAGMENT);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStratgey, DELIMITER_STRATEGY_TEXT);

    core::ProcessSession sessionGenFlowFile(context);
    std::shared_ptr<core::FlowFile> record[6];

    // Generate 6 flowfiles, first threes merged to one, second thress merged to one
    std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
    std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
    for (int i = 0; i < 6; i++) {
      std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
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
      record[i] = flow;
    }
    income_connection->put(record[0]);
    income_connection->put(record[2]);
    income_connection->put(record[5]);
    income_connection->put(record[4]);
    income_connection->put(record[1]);
    income_connection->put(record[3]);

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
    std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
    std::shared_ptr<core::FlowFile> flow2 = connection->poll(expiredFlowRecords);
    REQUIRE(flow1->getSize() == 96);
    {
      ReadCallback callback(flow1->getSize());
      sessionGenFlowFile.read(flow1, &callback);
      std::ifstream file1;
      file1.open(EXPECT_MERGE_CONTENT_FIRST, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      REQUIRE(expectContents == contents);
      file1.close();
    }
    REQUIRE(flow2->getSize() == 96);
    {
      ReadCallback callback(flow2->getSize());
      sessionGenFlowFile.read(flow2, &callback);
      std::ifstream file2;
      file2.open(EXPECT_MERGE_CONTENT_SECOND, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      REQUIRE(expectContents == contents);
      file2.close();
    }
    LogTestController::getInstance().reset();
    for (int i = 0; i < 6; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      unlink(flowFileName.c_str());
    }
    unlink(EXPECT_MERGE_CONTENT_FIRST);
    unlink(EXPECT_MERGE_CONTENT_SECOND);
  } catch (...) {
  }
}

TEST_CASE("MergeFileDefragmentDelimiter", "[mergefiletest2]") {
  try {
    std::ofstream expectfileFirst;
    std::ofstream expectfileSecond;
    std::ofstream headerfile, footerfile, demarcatorfile;
    expectfileFirst.open(EXPECT_MERGE_CONTENT_FIRST);
    expectfileSecond.open(EXPECT_MERGE_CONTENT_SECOND);
    headerfile.open(HEADER_FILE);
    headerfile << "header";
    expectfileFirst << "header";
    expectfileSecond << "header";
    headerfile.close();
    footerfile.open(FOOTER_FILE);
    footerfile << "footer";
    footerfile.close();
    demarcatorfile.open(DEMARCATOR_FILE);
    demarcatorfile << "demarcator";
    demarcatorfile.close();

    // Create and write to the test file
    for (int i = 0; i < 6; i++) {
      if (i != 0 && i <= 2)
        expectfileFirst << "demarcator";
      if (i != 3 && i >= 4)
        expectfileSecond << "demarcator";
      std::ofstream tmpfile;
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      tmpfile.open(flowFileName.c_str());
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        if (i < 3)
          expectfileFirst << std::to_string(i);
        else
          expectfileSecond << std::to_string(i);
      }
      tmpfile.close();
    }
    expectfileFirst << "footer";
    expectfileSecond << "footer";
    expectfileFirst.close();
    expectfileSecond.close();

    TestController testController;
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::MergeContent>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<core::ProcessSession>();
    LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::BinFiles>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::Bin>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::BinManager>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();

    std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

    std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::MergeContent>("mergecontent");
    std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
    processor->initialize();
    utils::Identifier processoruuid;
    REQUIRE(true == processor->getUUID(processoruuid));
    utils::Identifier logAttributeuuid;
    REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
    // connection from merge processor to log attribute
    std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
    connection->addRelationship(core::Relationship("merged", "Merge successful output"));
    connection->setSource(processor);
    connection->setDestination(logAttributeProcessor);
    connection->setSourceUUID(processoruuid);
    connection->setDestinationUUID(logAttributeuuid);
    processor->addConnection(connection);
    // connection to merge processor
    std::shared_ptr<minifi::Connection> mergeconnection = std::make_shared<minifi::Connection>(repo, content_repo, "mergeconnection");
    mergeconnection->setDestination(processor);
    mergeconnection->setDestinationUUID(processoruuid);
    processor->addConnection(mergeconnection);

    std::set<core::Relationship> autoTerminatedRelationships;
    core::Relationship original("original", "");
    core::Relationship failure("failure", "");
    autoTerminatedRelationships.insert(original);
    autoTerminatedRelationships.insert(failure);
    processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

    processor->incrementActiveTasks();
    processor->setScheduledState(core::ScheduledState::RUNNING);
    logAttributeProcessor->incrementActiveTasks();
    logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

    std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
    std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
    auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, MERGE_FORMAT_CONCAT_VALUE);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, MERGE_STRATEGY_DEFRAGMENT);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStratgey, DELIMITER_STRATEGY_FILENAME);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::Header, "/tmp/minifi-mergecontent.header");
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::Footer, "/tmp/minifi-mergecontent.footer");
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::Demarcator, "/tmp/minifi-mergecontent.demarcator");

    core::ProcessSession sessionGenFlowFile(context);
    std::shared_ptr<core::FlowFile> record[6];

    // Generate 6 flowfiles, first threes merged to one, second thress merged to one
    std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
    std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
    for (int i = 0; i < 6; i++) {
      std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
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
      record[i] = flow;
    }
    income_connection->put(record[0]);
    income_connection->put(record[2]);
    income_connection->put(record[5]);
    income_connection->put(record[4]);
    income_connection->put(record[1]);
    income_connection->put(record[3]);

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
    std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
    std::shared_ptr<core::FlowFile> flow2 = connection->poll(expiredFlowRecords);
    REQUIRE(flow1->getSize() == 128);
    {
      ReadCallback callback(flow1->getSize());
      sessionGenFlowFile.read(flow1, &callback);
      std::ifstream file1;
      file1.open(EXPECT_MERGE_CONTENT_FIRST, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      REQUIRE(expectContents == contents);
      file1.close();
    }
    REQUIRE(flow2->getSize() == 128);
    {
      ReadCallback callback(flow2->getSize());
      sessionGenFlowFile.read(flow2, &callback);
      std::ifstream file2;
      file2.open(EXPECT_MERGE_CONTENT_SECOND, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      REQUIRE(expectContents == contents);
      file2.close();
    }
    LogTestController::getInstance().reset();
    for (int i = 0; i < 6; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      unlink(flowFileName.c_str());
    }
    unlink(EXPECT_MERGE_CONTENT_FIRST);
    unlink(EXPECT_MERGE_CONTENT_SECOND);
    unlink(FOOTER_FILE);
    unlink(HEADER_FILE);
    unlink(DEMARCATOR_FILE);
  } catch (...) {
  }
}

TEST_CASE("MergeFileDefragmentDropFlow", "[mergefiletest3]") {
  try {
    std::ofstream expectfileFirst;
    std::ofstream expectfileSecond;

    expectfileFirst.open(EXPECT_MERGE_CONTENT_FIRST);
    expectfileSecond.open(EXPECT_MERGE_CONTENT_SECOND);

    // Create and write to the test file, drop record 4
    for (int i = 0; i < 6; i++) {
      if (i == 4)
        continue;
      std::ofstream tmpfile;
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      tmpfile.open(flowFileName.c_str());
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        if (i < 3)
          expectfileFirst << std::to_string(i);
        else
          expectfileSecond << std::to_string(i);
      }
      tmpfile.close();
    }
    expectfileFirst.close();
    expectfileSecond.close();

    TestController testController;
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::MergeContent>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<core::ProcessSession>();
    LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::BinFiles>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::Bin>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::BinManager>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();

    std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

    std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::MergeContent>("mergecontent");
    std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
    processor->initialize();
    utils::Identifier processoruuid;
    REQUIRE(true == processor->getUUID(processoruuid));
    utils::Identifier logAttributeuuid;
    REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
    // connection from merge processor to log attribute
    std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
    connection->addRelationship(core::Relationship("merged", "Merge successful output"));
    connection->setSource(processor);
    connection->setDestination(logAttributeProcessor);
    connection->setSourceUUID(processoruuid);
    connection->setDestinationUUID(logAttributeuuid);
    processor->addConnection(connection);
    // connection to merge processor
    std::shared_ptr<minifi::Connection> mergeconnection = std::make_shared<minifi::Connection>(repo, content_repo, "mergeconnection");
    mergeconnection->setDestination(processor);
    mergeconnection->setDestinationUUID(processoruuid);
    processor->addConnection(mergeconnection);

    std::set<core::Relationship> autoTerminatedRelationships;
    core::Relationship original("original", "");
    core::Relationship failure("failure", "");
    autoTerminatedRelationships.insert(original);
    autoTerminatedRelationships.insert(failure);
    processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

    processor->incrementActiveTasks();
    processor->setScheduledState(core::ScheduledState::RUNNING);
    logAttributeProcessor->incrementActiveTasks();
    logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

    std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
    std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
    auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, MERGE_FORMAT_CONCAT_VALUE);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, MERGE_STRATEGY_DEFRAGMENT);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStratgey, DELIMITER_STRATEGY_TEXT);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MaxBinAge, "1 sec");

    core::ProcessSession sessionGenFlowFile(context);
    std::shared_ptr<core::FlowFile> record[6];

    // Generate 6 flowfiles, first threes merged to one, second thress merged to one
    std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
    std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
    for (int i = 0; i < 6; i++) {
      if (i == 4)
        continue;
      std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
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
      record[i] = flow;
    }
    income_connection->put(record[0]);
    income_connection->put(record[2]);
    income_connection->put(record[5]);
    income_connection->put(record[1]);
    income_connection->put(record[3]);

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
    }
    // validate the merge content
    std::set<std::shared_ptr<core::FlowFile>> expiredFlowRecords;
    std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
    std::shared_ptr<core::FlowFile> flow2 = connection->poll(expiredFlowRecords);
    REQUIRE(flow1->getSize() == 96);
    {
      ReadCallback callback(flow1->getSize());
      sessionGenFlowFile.read(flow1, &callback);
      std::ifstream file1;
      file1.open(EXPECT_MERGE_CONTENT_FIRST, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      REQUIRE(expectContents == contents);
      file1.close();
    }
    REQUIRE(flow2->getSize() == 64);
    {
      ReadCallback callback(flow2->getSize());
      sessionGenFlowFile.read(flow2, &callback);
      std::ifstream file2;
      file2.open(EXPECT_MERGE_CONTENT_SECOND, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      REQUIRE(expectContents == contents);
      file2.close();
    }
    LogTestController::getInstance().reset();
    for (int i = 0; i < 6; i++) {
      if (i == 4)
        continue;
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      unlink(flowFileName.c_str());
    }
    unlink(EXPECT_MERGE_CONTENT_FIRST);
    unlink(EXPECT_MERGE_CONTENT_SECOND);
  } catch (...) {
  }
}

TEST_CASE("MergeFileBinPack", "[mergefiletest4]") {
  try {
    std::ofstream expectfileFirst;
    std::ofstream expectfileSecond;
    expectfileFirst.open(EXPECT_MERGE_CONTENT_FIRST);
    expectfileSecond.open(EXPECT_MERGE_CONTENT_SECOND);

    // Create and write to the test file
    for (int i = 0; i < 6; i++) {
      std::ofstream tmpfile;
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      tmpfile.open(flowFileName.c_str());
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        if (i < 3)
          expectfileFirst << std::to_string(i);
        else
          expectfileSecond << std::to_string(i);
      }
      tmpfile.close();
    }
    expectfileFirst.close();
    expectfileSecond.close();

    TestController testController;
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::MergeContent>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<core::ProcessSession>();
    LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::BinFiles>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::Bin>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::BinManager>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();

    std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

    std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::MergeContent>("mergecontent");
    std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
    processor->initialize();
    utils::Identifier processoruuid;
    REQUIRE(true == processor->getUUID(processoruuid));
    utils::Identifier logAttributeuuid;
    REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
    // connection from merge processor to log attribute
    std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
    connection->addRelationship(core::Relationship("merged", "Merge successful output"));
    connection->setSource(processor);
    connection->setDestination(logAttributeProcessor);
    connection->setSourceUUID(processoruuid);
    connection->setDestinationUUID(logAttributeuuid);
    processor->addConnection(connection);
    // connection to merge processor
    std::shared_ptr<minifi::Connection> mergeconnection = std::make_shared<minifi::Connection>(repo, content_repo, "mergeconnection");
    mergeconnection->setDestination(processor);
    mergeconnection->setDestinationUUID(processoruuid);
    processor->addConnection(mergeconnection);

    std::set<core::Relationship> autoTerminatedRelationships;
    core::Relationship original("original", "");
    core::Relationship failure("failure", "");
    autoTerminatedRelationships.insert(original);
    autoTerminatedRelationships.insert(failure);
    processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

    processor->incrementActiveTasks();
    processor->setScheduledState(core::ScheduledState::RUNNING);
    logAttributeProcessor->incrementActiveTasks();
    logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

    std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
    std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
    auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, MERGE_FORMAT_CONCAT_VALUE);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, MERGE_STRATEGY_BIN_PACK);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStratgey, DELIMITER_STRATEGY_TEXT);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MinSize, "96");
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::CorrelationAttributeName, "tag");

    core::ProcessSession sessionGenFlowFile(context);
    std::shared_ptr<core::FlowFile> record[6];

    // Generate 6 flowfiles, first threes merged to one, second thress merged to one
    std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
    std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
    for (int i = 0; i < 6; i++) {
      std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      sessionGenFlowFile.import(flowFileName, flow, true, 0);
      flow->setAttribute("tag", "tag");
      record[i] = flow;
    }
    income_connection->put(record[0]);
    income_connection->put(record[1]);
    income_connection->put(record[2]);
    income_connection->put(record[3]);
    income_connection->put(record[4]);
    income_connection->put(record[5]);

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
    std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
    std::shared_ptr<core::FlowFile> flow2 = connection->poll(expiredFlowRecords);
    REQUIRE(flow1->getSize() == 96);
    {
      ReadCallback callback(flow1->getSize());
      sessionGenFlowFile.read(flow1, &callback);
      std::ifstream file1;
      file1.open(EXPECT_MERGE_CONTENT_FIRST, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      REQUIRE(expectContents == contents);
      file1.close();
    }
    REQUIRE(flow2->getSize() == 96);
    {
      ReadCallback callback(flow2->getSize());
      sessionGenFlowFile.read(flow2, &callback);
      std::ifstream file2;
      file2.open(EXPECT_MERGE_CONTENT_SECOND, std::ios::in);
      std::string contents((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());
      std::string expectContents(reinterpret_cast<char *> (callback.buffer_), callback.read_size_);
      REQUIRE(expectContents == contents);
      file2.close();
    }
    LogTestController::getInstance().reset();
    for (int i = 0; i < 6; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      unlink(flowFileName.c_str());
    }
    unlink(EXPECT_MERGE_CONTENT_FIRST);
    unlink(EXPECT_MERGE_CONTENT_SECOND);
  } catch (...) {
  }
}


TEST_CASE("MergeFileTar", "[mergefiletest4]") {
  try {
    std::ofstream expectfileFirst;
    std::ofstream expectfileSecond;
    expectfileFirst.open(EXPECT_MERGE_CONTENT_FIRST);
    expectfileSecond.open(EXPECT_MERGE_CONTENT_SECOND);

    // Create and write to the test file
    for (int i = 0; i < 6; i++) {
      std::ofstream tmpfile;
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      tmpfile.open(flowFileName.c_str());
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        if (i < 3)
          expectfileFirst << std::to_string(i);
        else
          expectfileSecond << std::to_string(i);
      }
      tmpfile.close();
    }
    expectfileFirst.close();
    expectfileSecond.close();

    TestController testController;
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::MergeContent>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<core::ProcessSession>();
    LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::BinFiles>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::Bin>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::BinManager>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();

    std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

    std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::MergeContent>("mergecontent");
    std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
    processor->initialize();
    utils::Identifier processoruuid;
    REQUIRE(true == processor->getUUID(processoruuid));
    utils::Identifier logAttributeuuid;
    REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
    // connection from merge processor to log attribute
    std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
    connection->addRelationship(core::Relationship("merged", "Merge successful output"));
    connection->setSource(processor);
    connection->setDestination(logAttributeProcessor);
    connection->setSourceUUID(processoruuid);
    connection->setDestinationUUID(logAttributeuuid);
    processor->addConnection(connection);
    // connection to merge processor
    std::shared_ptr<minifi::Connection> mergeconnection = std::make_shared<minifi::Connection>(repo, content_repo, "mergeconnection");
    mergeconnection->setDestination(processor);
    mergeconnection->setDestinationUUID(processoruuid);
    processor->addConnection(mergeconnection);

    std::set<core::Relationship> autoTerminatedRelationships;
    core::Relationship original("original", "");
    core::Relationship failure("failure", "");
    autoTerminatedRelationships.insert(original);
    autoTerminatedRelationships.insert(failure);
    processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

    processor->incrementActiveTasks();
    processor->setScheduledState(core::ScheduledState::RUNNING);
    logAttributeProcessor->incrementActiveTasks();
    logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

    std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
    std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
    auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, MERGE_FORMAT_TAR_VALUE);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, MERGE_STRATEGY_BIN_PACK);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStratgey, DELIMITER_STRATEGY_TEXT);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MinSize, "96");
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::CorrelationAttributeName, "tag");

    core::ProcessSession sessionGenFlowFile(context);
    std::shared_ptr<core::FlowFile> record[6];

    // Generate 6 flowfiles, first threes merged to one, second thress merged to one
    std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
    std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
    for (int i = 0; i < 6; i++) {
      std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      sessionGenFlowFile.import(flowFileName, flow, true, 0);
      flow->setAttribute("tag", "tag");
      record[i] = flow;
    }
    income_connection->put(record[0]);
    income_connection->put(record[1]);
    income_connection->put(record[2]);
    income_connection->put(record[3]);
    income_connection->put(record[4]);
    income_connection->put(record[5]);

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
    std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
    std::shared_ptr<core::FlowFile> flow2 = connection->poll(expiredFlowRecords);
    REQUIRE(flow1->getSize() > 0);
    {
      ReadCallback callback(flow1->getSize());
      sessionGenFlowFile.read(flow1, &callback);
      callback.archive_read();
      REQUIRE(callback.archive_buffer_num_ == 3);
      for (int i = 0; i < 3; i++) {
        std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
        std::ifstream file1;
        file1.open(flowFileName, std::ios::in);
        std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
        std::string expectContents(reinterpret_cast<char *> (callback.archive_buffer_[i]), callback.archive_buffer_size_[i]);
        REQUIRE(expectContents == contents);
        file1.close();
      }
    }
    REQUIRE(flow2->getSize() > 0);
    {
      ReadCallback callback(flow2->getSize());
      sessionGenFlowFile.read(flow2, &callback);
      callback.archive_read();
      REQUIRE(callback.archive_buffer_num_ == 3);
      for (int i = 3; i < 6; i++) {
        std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
        std::ifstream file1;
        file1.open(flowFileName, std::ios::in);
        std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
        std::string expectContents(reinterpret_cast<char *> (callback.archive_buffer_[i-3]), callback.archive_buffer_size_[i-3]);
        REQUIRE(expectContents == contents);
        file1.close();
      }
    }
    LogTestController::getInstance().reset();
    for (int i = 0; i < 6; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      unlink(flowFileName.c_str());
    }
    unlink(EXPECT_MERGE_CONTENT_FIRST);
    unlink(EXPECT_MERGE_CONTENT_SECOND);
  } catch (...) {
  }
}

TEST_CASE("MergeFileZip", "[mergefiletest5]") {
  try {
    std::ofstream expectfileFirst;
    std::ofstream expectfileSecond;
    expectfileFirst.open(EXPECT_MERGE_CONTENT_FIRST);
    expectfileSecond.open(EXPECT_MERGE_CONTENT_SECOND);

    // Create and write to the test file
    for (int i = 0; i < 6; i++) {
      std::ofstream tmpfile;
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      tmpfile.open(flowFileName.c_str());
      for (int j = 0; j < 32; j++) {
        tmpfile << std::to_string(i);
        if (i < 3)
          expectfileFirst << std::to_string(i);
        else
          expectfileSecond << std::to_string(i);
      }
      tmpfile.close();
    }
    expectfileFirst.close();
    expectfileSecond.close();

    TestController testController;
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::MergeContent>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<core::ProcessSession>();
    LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::BinFiles>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::Bin>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::BinManager>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();

    std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

    std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::MergeContent>("mergecontent");
    std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
    processor->initialize();
    utils::Identifier processoruuid;
    REQUIRE(true == processor->getUUID(processoruuid));
    utils::Identifier logAttributeuuid;
    REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());
    // connection from merge processor to log attribute
    std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "logattributeconnection");
    connection->addRelationship(core::Relationship("merged", "Merge successful output"));
    connection->setSource(processor);
    connection->setDestination(logAttributeProcessor);
    connection->setSourceUUID(processoruuid);
    connection->setDestinationUUID(logAttributeuuid);
    processor->addConnection(connection);
    // connection to merge processor
    std::shared_ptr<minifi::Connection> mergeconnection = std::make_shared<minifi::Connection>(repo, content_repo, "mergeconnection");
    mergeconnection->setDestination(processor);
    mergeconnection->setDestinationUUID(processoruuid);
    processor->addConnection(mergeconnection);

    std::set<core::Relationship> autoTerminatedRelationships;
    core::Relationship original("original", "");
    core::Relationship failure("failure", "");
    autoTerminatedRelationships.insert(original);
    autoTerminatedRelationships.insert(failure);
    processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

    processor->incrementActiveTasks();
    processor->setScheduledState(core::ScheduledState::RUNNING);
    logAttributeProcessor->incrementActiveTasks();
    logAttributeProcessor->setScheduledState(core::ScheduledState::RUNNING);

    std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);
    std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
    auto context = std::make_shared<core::ProcessContext>(node, controller_services_provider, repo, repo, content_repo);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeFormat, MERGE_FORMAT_ZIP_VALUE);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MergeStrategy, MERGE_STRATEGY_BIN_PACK);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::DelimiterStratgey, DELIMITER_STRATEGY_TEXT);
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::MinSize, "96");
    context->setProperty(org::apache::nifi::minifi::processors::MergeContent::CorrelationAttributeName, "tag");

    core::ProcessSession sessionGenFlowFile(context);
    std::shared_ptr<core::FlowFile> record[6];

    // Generate 6 flowfiles, first threes merged to one, second thress merged to one
    std::shared_ptr<core::Connectable> income = node->getNextIncomingConnection();
    std::shared_ptr<minifi::Connection> income_connection = std::static_pointer_cast<minifi::Connection>(income);
    for (int i = 0; i < 6; i++) {
      std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast < core::FlowFile > (sessionGenFlowFile.create());
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      sessionGenFlowFile.import(flowFileName, flow, true, 0);
      flow->setAttribute("tag", "tag");
      record[i] = flow;
    }
    income_connection->put(record[0]);
    income_connection->put(record[1]);
    income_connection->put(record[2]);
    income_connection->put(record[3]);
    income_connection->put(record[4]);
    income_connection->put(record[5]);

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
    std::shared_ptr<core::FlowFile> flow1 = connection->poll(expiredFlowRecords);
    std::shared_ptr<core::FlowFile> flow2 = connection->poll(expiredFlowRecords);
    REQUIRE(flow1->getSize() > 0);
    {
      ReadCallback callback(flow1->getSize());
      sessionGenFlowFile.read(flow1, &callback);
      callback.archive_read();
      REQUIRE(callback.archive_buffer_num_ == 3);
      for (int i = 0; i < 3; i++) {
        std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
        std::ifstream file1;
        file1.open(flowFileName, std::ios::in);
        std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
        std::string expectContents(reinterpret_cast<char *> (callback.archive_buffer_[i]), callback.archive_buffer_size_[i]);
        REQUIRE(expectContents == contents);
        file1.close();
      }
    }
    REQUIRE(flow2->getSize() > 0);
    {
      ReadCallback callback(flow2->getSize());
      sessionGenFlowFile.read(flow2, &callback);
      callback.archive_read();
      REQUIRE(callback.archive_buffer_num_ == 3);
      for (int i = 3; i < 6; i++) {
        std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
        std::ifstream file1;
        file1.open(flowFileName, std::ios::in);
        std::string contents((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
        std::string expectContents(reinterpret_cast<char *> (callback.archive_buffer_[i-3]), callback.archive_buffer_size_[i-3]);
        REQUIRE(expectContents == contents);
        file1.close();
      }
    }
    LogTestController::getInstance().reset();
    for (int i = 0; i < 6; i++) {
      std::string flowFileName = std::string(FLOW_FILE) + "." + std::to_string(i) + ".txt";
      unlink(flowFileName.c_str());
    }
    unlink(EXPECT_MERGE_CONTENT_FIRST);
    unlink(EXPECT_MERGE_CONTENT_SECOND);
  } catch (...) {
  }
}




