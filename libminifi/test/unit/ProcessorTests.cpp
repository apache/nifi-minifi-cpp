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
#include "FlowController.h"
#include "ProvenanceTestHelper.h"
#include "../TestBase.h"
<<<<<<< HEAD
#include <memory>
#include "../../include/LogAppenders.h"
#include "GetFile.h"
=======
#include "core/logging/LogAppenders.h"
#include "core/logging/BaseLogger.h"
#include "processors/GetFile.h"
#include "core/core.h"
#include "../../include/core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"

TEST_CASE("Test Creation of GetFile", "[getfileCreate]") {
  org::apache::nifi::minifi::processors::GetFile processor("processorname");
  REQUIRE(processor.getName() == "processorname");
}
>>>>>>> MINIFI-217: First commit. Updates namespaces and removes

TEST_CASE("Test Find file", "[getfileCreate2]") {

  TestController testController;

  testController.enableDebug();

  std::shared_ptr<ProvenanceTestRepository> repo = std::make_shared<
      ProvenanceTestRepository>();

  std::shared_ptr<core::Processor> processor = std::make_shared<
      org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");

<<<<<<< HEAD
	Configure *config = Configure::getConfigure();

	config->set(BaseLogger::nifi_log_appender,"rollingappender");
	config->set(OutputStreamAppender::nifi_log_output_stream_error_stderr,"true");
	std::shared_ptr<Logger> logger = Logger::getLogger();
	std::unique_ptr<BaseLogger> newLogger =LogInstance::getConfiguredLogger(config);
	logger->updateLogger(std::move(newLogger));
	logger->setLogLevel("debug");

	ProvenanceTestRepository provenanceRepo;
	FlowTestRepository flowRepo;
	TestFlowController controller(provenanceRepo, flowRepo);
	FlowControllerFactory::getFlowController( dynamic_cast<FlowController*>(&controller));
=======
  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  uuid_t processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));
>>>>>>> MINIFI-217: First commit. Updates namespaces and removes

  std::shared_ptr<minifi::Connection> connection = std::make_shared<
      minifi::Connection>("getfileCreate2Connection");
  connection->setRelationship(core::Relationship("success", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(processor);

  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(processoruuid);

  processor->addConnection(connection);
  REQUIRE(dir != NULL);

  core::ProcessorNode node(processor);

  core::ProcessContext context(node, repo);
  context.setProperty(org::apache::nifi::minifi::processors::GetFile::Directory,
                      dir);
  core::ProcessSession session(&context);

<<<<<<< HEAD
	connection.setSourceProcessor(&processor);
	connection.setDestinationProcessor(&processor);
=======
  REQUIRE(processor->getName() == "getfileCreate2");

  std::shared_ptr<core::FlowFile> record;
  processor->setScheduledState(core::ScheduledState::RUNNING);
  processor->onTrigger(&context, &session);
>>>>>>> MINIFI-217: First commit. Updates namespaces and removes

  provenance::ProvenanceReporter *reporter = session.getProvenanceReporter();
  std::set<provenance::ProvenanceEventRecord*> records = reporter->getEvents();
  record = session.get();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  processor->onTrigger(&context, &session);
  unlink(ss.str().c_str());
  rmdir(dir);
  reporter = session.getProvenanceReporter();

<<<<<<< HEAD
	REQUIRE( processor.getName() == "getfileCreate2");
=======
  records = reporter->getEvents();

  for (provenance::ProvenanceEventRecord *provEventRecord : records) {
    REQUIRE(provEventRecord->getComponentType() == processor->getName());
  }
  session.commit();
  std::shared_ptr<core::FlowFile> ffr = session.get();
>>>>>>> MINIFI-217: First commit. Updates namespaces and removes

  ffr->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
  REQUIRE(2 == repo->getRepoMap().size());

  for (auto entry : repo->getRepoMap()) {
    provenance::ProvenanceEventRecord newRecord;
    newRecord.DeSerialize((uint8_t*) entry.second.data(),
                          entry.second.length());

    bool found = false;
    for (auto provRec : records) {
      if (provRec->getEventId() == newRecord.getEventId()) {
        REQUIRE(provRec->getEventId() == newRecord.getEventId());
        REQUIRE(provRec->getComponentId() == newRecord.getComponentId());
        REQUIRE(provRec->getComponentType() == newRecord.getComponentType());
        REQUIRE(provRec->getDetails() == newRecord.getDetails());
        REQUIRE(provRec->getEventDuration() == newRecord.getEventDuration());
        found = true;
        break;
      }
    }
    if (!found)
      throw std::runtime_error("Did not find record");

<<<<<<< HEAD
	std::fstream file;
	std::stringstream ss;
	std::string fileName("tstFile.ext");
	ss << dir << "/" << fileName;
	file.open(ss.str(),std::ios::out);
	file << "tempFile";
	int64_t fileSize = file.tellp();
	file.close();

	processor.incrementActiveTasks();
	processor.setScheduledState(ScheduledState::RUNNING);

	processor.onTrigger(&context,&session);
	unlink(ss.str().c_str());
	rmdir(dir);
=======
  }

}
>>>>>>> MINIFI-217: First commit. Updates namespaces and removes

TEST_CASE("LogAttributeTest", "[getfileCreate3]") {
  std::ostringstream oss;
  std::unique_ptr<logging::BaseLogger> outputLogger = std::unique_ptr<
      logging::BaseLogger>(
      new org::apache::nifi::minifi::core::logging::OutputStreamAppender(oss,
                                                                         0));
  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
  logger->updateLogger(std::move(outputLogger));

  TestController testController;

  testController.enableDebug();

<<<<<<< HEAD
        // verify flow file repo
	REQUIRE( 1 == flowRepo.getRepoMap().size() );

	for(auto  entry: flowRepo.getRepoMap())
	{
		FlowFileEventRecord newRecord;
		newRecord.DeSerialize((uint8_t*)entry.second.data(),entry.second.length());
		REQUIRE (fileSize == newRecord.getFileSize());
		REQUIRE (0 == newRecord.getFileOffset());
		std::map<std::string, std::string> attrs = newRecord.getAttributes();
		std::string key = FlowAttributeKey(FILENAME);
		REQUIRE (attrs[key] == fileName);
	}

	FlowFileRecord *ffr = session.get();
=======
  std::shared_ptr<ProvenanceTestRepository> repo = std::make_shared<
      ProvenanceTestRepository>();

  std::shared_ptr<core::Processor> processor = std::make_shared<
      org::apache::nifi::minifi::processors::GetFile>("getfileCreate2");

  std::shared_ptr<core::Processor> logAttribute = std::make_shared<
      org::apache::nifi::minifi::processors::LogAttribute>("logattribute");
>>>>>>> MINIFI-217: First commit. Updates namespaces and removes

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  uuid_t processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));

  uuid_t logattribute_uuid;
  REQUIRE(true == logAttribute->getUUID(logattribute_uuid));

<<<<<<< HEAD
	REQUIRE( 2 == provenanceRepo.getRepoMap().size() );

	for(auto  entry: provenanceRepo.getRepoMap())
	{
		ProvenanceEventRecord newRecord;
		newRecord.DeSerialize((uint8_t*)entry.second.data(),entry.second.length());

		bool found = false;
		for ( auto provRec : records)
		{
			if (provRec->getEventId() == newRecord.getEventId() )
			{
				REQUIRE( provRec->getEventId() == newRecord.getEventId());
				REQUIRE( provRec->getComponentId() == newRecord.getComponentId());
				REQUIRE( provRec->getComponentType() == newRecord.getComponentType());
				REQUIRE( provRec->getDetails() == newRecord.getDetails());
				REQUIRE( provRec->getEventDuration() == newRecord.getEventDuration());
				found = true;
				break;
			}
		}
		if (!found)
		throw std::runtime_error("Did not find record");
	}
=======
  std::shared_ptr<minifi::Connection> connection = std::make_shared<
      minifi::Connection>("getfileCreate2Connection");
  connection->setRelationship(core::Relationship("success", "description"));

  std::shared_ptr<minifi::Connection> connection2 = std::make_shared<
      minifi::Connection>("logattribute");
  connection2->setRelationship(core::Relationship("success", "description"));

  // link the connections so that we can test results at the end for this
  connection->setSource(processor);
>>>>>>> MINIFI-217: First commit. Updates namespaces and removes

  // link the connections so that we can test results at the end for this
  connection->setDestination(logAttribute);

<<<<<<< HEAD
=======
  connection2->setSource(logAttribute);
>>>>>>> MINIFI-217: First commit. Updates namespaces and removes

  connection2->setSourceUUID(logattribute_uuid);
  connection->setSourceUUID(processoruuid);
  connection->setDestinationUUID(logattribute_uuid);

  processor->addConnection(connection);
  logAttribute->addConnection(connection);
  logAttribute->addConnection(connection2);
  REQUIRE(dir != NULL);

  core::ProcessorNode node(processor);
  core::ProcessorNode node2(logAttribute);

  core::ProcessContext context(node, repo);
  core::ProcessContext context2(node2, repo);
  context.setProperty(org::apache::nifi::minifi::processors::GetFile::Directory,
                      dir);
  core::ProcessSession session(&context);
  core::ProcessSession session2(&context2);

  REQUIRE(processor->getName() == "getfileCreate2");

  std::shared_ptr<core::FlowFile> record;
  processor->setScheduledState(core::ScheduledState::RUNNING);
  processor->onTrigger(&context, &session);

  logAttribute->incrementActiveTasks();
  logAttribute->setScheduledState(core::ScheduledState::RUNNING);
  logAttribute->onTrigger(&context2, &session2);

  provenance::ProvenanceReporter *reporter = session.getProvenanceReporter();
  std::set<provenance::ProvenanceEventRecord*> records = reporter->getEvents();
  record = session.get();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();

  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  processor->onTrigger(&context, &session);
  unlink(ss.str().c_str());
  rmdir(dir);
  reporter = session.getProvenanceReporter();

  records = reporter->getEvents();
  session.commit();
  oss.str("");
  oss.clear();

  logAttribute->incrementActiveTasks();
  logAttribute->setScheduledState(core::ScheduledState::RUNNING);
  logAttribute->onTrigger(&context2, &session2);

  //session2.commit();
  records = reporter->getEvents();

  std::string log_attribute_output = oss.str();
  REQUIRE(
      log_attribute_output.find("key:absolute.path value:" + ss.str())
          != std::string::npos);
  REQUIRE(log_attribute_output.find("Size:8 Offset:0") != std::string::npos);
  REQUIRE(
      log_attribute_output.find("key:path value:" + std::string(dir))
          != std::string::npos);

  outputLogger = std::unique_ptr<logging::BaseLogger>(
      new org::apache::nifi::minifi::core::logging::NullAppender());
  logger->updateLogger(std::move(outputLogger));

}
