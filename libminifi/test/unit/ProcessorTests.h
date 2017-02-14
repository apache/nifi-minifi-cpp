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
#include "ProvenanceTests.h"
#include "../TestBase.h"
#include "GetFile.h"

#ifndef PROCESSOR_TESTS
#define PROCESSOR_TESTS

TEST_CASE("Test Creation of GetFile", "[getfileCreate]"){
	GetFile processor("processorname");
	REQUIRE( processor.getName() == "processorname");
}


TEST_CASE("Test Find file", "[getfileCreate2]"){

	TestController testController;

	testController.enableDebug();

	ProvenanceTestRepository repo;
	TestFlowController controller(repo);
	FlowControllerFactory::getFlowController( dynamic_cast<FlowController*>(&controller));

	GetFile processor("getfileCreate2");

	char format[] ="/tmp/gt.XXXXXX";
	char *dir = testController.createTempDirectory(format);


	uuid_t processoruuid;
	REQUIRE( true == processor.getUUID(processoruuid) );

	Connection connection("getfileCreate2Connection");
	connection.setRelationship(Relationship("success","description"));

	// link the connections so that we can test results at the end for this

	connection.setSourceProcessor(&processor);


	connection.setSourceProcessorUUID(processoruuid);
	connection.setDestinationProcessorUUID(processoruuid);

	processor.addConnection(&connection);
	REQUIRE( dir != NULL );

	ProcessContext context(&processor);
	context.setProperty(GetFile::Directory,dir);
	ProcessSession session(&context);


	REQUIRE( processor.getName() == "getfileCreate2");

	FlowFileRecord *record;
	processor.setScheduledState(ScheduledState::RUNNING);
	processor.onTrigger(&context,&session);

	ProvenanceReporter *reporter = session.getProvenanceReporter();
	std::set<ProvenanceEventRecord*> records = reporter->getEvents();

	record = session.get();
	REQUIRE( record== 0 );
	REQUIRE( records.size() == 0 );

	std::fstream file;
	std::stringstream ss;
	ss << dir << "/" << "tstFile.ext";
	file.open(ss.str(),std::ios::out);
	file << "tempFile";
	file.close();

	processor.incrementActiveTasks();
	processor.setScheduledState(ScheduledState::RUNNING);
	processor.onTrigger(&context,&session);
	unlink(ss.str().c_str());
	rmdir(dir);

	reporter = session.getProvenanceReporter();

	records = reporter->getEvents();

	for(ProvenanceEventRecord *provEventRecord : records)
	{
		REQUIRE (provEventRecord->getComponentType() == processor.getName());
	}
	session.commit();

	FlowFileRecord *ffr = session.get();

	ffr->getResourceClaim()->decreaseFlowFileRecordOwnedCount();

	delete ffr;

	std::set<FlowFileRecord*> expiredFlows;

	REQUIRE( 2 == repo.getRepoMap().size() );

	for(auto  entry: repo.getRepoMap())
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






}



#endif


