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

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
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
#include <iostream>

static const std::string NEWLINE_FILE = ""
    "one,two,three\n"
    "four,five,six, seven";
static const std::string TMP_FILE = "/tmp/minifi-tmpfile.txt";
static const std::string STATE_FILE = "/tmp/minifi-state-file.txt";

TEST_CASE("TailFileWithDelimiter", "[tailfiletest1]") {

    try {
        //Create and write to the test file
        std::ofstream tmpfile;
        tmpfile.open (TMP_FILE);
        tmpfile << NEWLINE_FILE;
        tmpfile.close();

        TestController testController;
        LogTestController::getInstance().setInfo<org::apache::nifi::minifi::processors::TailFile>();

        std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

        std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::TailFile>("tailfile");
        std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");

        uuid_t processoruuid;
        REQUIRE(true == processor->getUUID(processoruuid));
        uuid_t logAttributeuuid;
        REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

        std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, "logattributeconnection");
        connection->setRelationship(core::Relationship("success", "TailFile successful output"));

        // link the connections so that we can test results at the end for this
        connection->setDestination(connection);

        connection->setSourceUUID(processoruuid);

        processor->addConnection(connection);

        core::ProcessorNode node(processor);

        std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
        core::ProcessContext context(node, controller_services_provider, repo);
        context.setProperty(org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");
        context.setProperty(org::apache::nifi::minifi::processors::TailFile::FileName,TMP_FILE);
        context.setProperty(org::apache::nifi::minifi::processors::TailFile::StateFile, STATE_FILE);

        core::ProcessSession session(&context);

        REQUIRE(processor->getName() == "tailfile");

        core::ProcessSessionFactory factory(&context);

        std::shared_ptr<core::FlowFile> record;
        processor->setScheduledState(core::ScheduledState::RUNNING);
        processor->onSchedule(&context, &factory);
        processor->onTrigger(&context, &session);

        provenance::ProvenanceReporter *reporter = session.getProvenanceReporter();
        std::set<provenance::ProvenanceEventRecord*> provRecords = reporter->getEvents();
        record = session.get();
        REQUIRE(record == nullptr);
        std::shared_ptr<core::FlowFile> ff = session.get();
        REQUIRE(provRecords.size() == 4); //2 creates and 2 modifies for flowfiles

        LogTestController::getInstance().reset();
    } catch (...) {

    }

    //Delete the test and state file.
    std::remove(TMP_FILE.c_str());
    std::remove(STATE_FILE.c_str());

}


TEST_CASE("TailFileWithoutDelimiter", "[tailfiletest2]") {

    try {
        //Create and write to the test file
        std::ofstream tmpfile;
        tmpfile.open (TMP_FILE);
        tmpfile << NEWLINE_FILE;
        tmpfile.close();

        TestController testController;
        LogTestController::getInstance().setInfo<org::apache::nifi::minifi::processors::TailFile>();

        std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

        std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::TailFile>("tailfile");
        std::shared_ptr<core::Processor> logAttributeProcessor = std::make_shared<org::apache::nifi::minifi::processors::LogAttribute>("logattribute");

        uuid_t processoruuid;
        REQUIRE(true == processor->getUUID(processoruuid));
        uuid_t logAttributeuuid;
        REQUIRE(true == logAttributeProcessor->getUUID(logAttributeuuid));

        std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, "logattributeconnection");
        connection->setRelationship(core::Relationship("success", "TailFile successful output"));

        // link the connections so that we can test results at the end for this
        connection->setDestination(connection);
        connection->setSourceUUID(processoruuid);

        processor->addConnection(connection);

        core::ProcessorNode node(processor);

        std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;
        core::ProcessContext context(node, controller_services_provider, repo);
        context.setProperty(org::apache::nifi::minifi::processors::TailFile::FileName, TMP_FILE);
        context.setProperty(org::apache::nifi::minifi::processors::TailFile::StateFile, STATE_FILE);

        core::ProcessSession session(&context);

        REQUIRE(processor->getName() == "tailfile");

        core::ProcessSessionFactory factory(&context);

        std::shared_ptr<core::FlowFile> record;
        processor->setScheduledState(core::ScheduledState::RUNNING);
        processor->onSchedule(&context, &factory);
        processor->onTrigger(&context, &session);

        provenance::ProvenanceReporter *reporter = session.getProvenanceReporter();
        std::set<provenance::ProvenanceEventRecord*> provRecords = reporter->getEvents();
        record = session.get();
        REQUIRE(record == nullptr);
        std::shared_ptr<core::FlowFile> ff = session.get();
        REQUIRE(provRecords.size() == 2);

        LogTestController::getInstance().reset();
    } catch (...) {

    }

    //Delete the test and state file.
    std::remove(TMP_FILE.c_str());
    std::remove(STATE_FILE.c_str());
}