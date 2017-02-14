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


#ifndef PROVENANCE_TESTS
#define PROVENANCE_TESTS
#include "../TestBase.h"

#include "ProvenanceTestHelper.h"
#include "Provenance.h"
#include "FlowFileRecord.h"



TEST_CASE("Test Provenance record create", "[TestProvenanceEventRecord]"){

	ProvenanceEventRecord record1(ProvenanceEventRecord::ProvenanceEventType::CREATE,"blah","blahblah");
	REQUIRE( record1.getAttributes().size() == 0);
	REQUIRE( record1.getAlternateIdentifierUri().length() == 0);

}


TEST_CASE("Test Provenance record serialization", "[TestProvenanceEventRecordSerializeDeser]"){

	ProvenanceEventRecord record1(ProvenanceEventRecord::ProvenanceEventType::CREATE,"componentid","componenttype");

	std::string eventId = record1.getEventId();
	
	std::string smileyface = ":)" ;
	record1.setDetails(smileyface);

	ProvenanceTestRepository repo;
	uint64_t sample = 65555;
	ProvenanceRepository *testRepository = dynamic_cast<ProvenanceRepository*>(&repo);
	record1.setEventDuration(sample);

	record1.Serialize(testRepository);
	ProvenanceEventRecord record2;
	REQUIRE( record2.DeSerialize(testRepository,eventId) == true);
	REQUIRE( record2.getEventId() == record1.getEventId());
	REQUIRE( record2.getComponentId() == record1.getComponentId());
	REQUIRE( record2.getComponentType() == record1.getComponentType());
	REQUIRE( record2.getDetails() == record1.getDetails());
	REQUIRE( record2.getDetails() == smileyface);
	REQUIRE( record2.getEventDuration() == sample);
}


TEST_CASE("Test Flowfile record added to provenance", "[TestFlowAndProv1]"){

	ProvenanceEventRecord record1(ProvenanceEventRecord::ProvenanceEventType::CLONE,"componentid","componenttype");
	std::string eventId = record1.getEventId();
	std::map<std::string, std::string> attributes;
	attributes.insert(std::pair<std::string,std::string>("potato","potatoe"));
	attributes.insert(std::pair<std::string,std::string>("tomato","tomatoe"));
	FlowFileRecord ffr1(attributes);

	record1.addChildFlowFile(&ffr1);

	ProvenanceTestRepository repo;
	uint64_t sample = 65555;
	ProvenanceRepository *testRepository = dynamic_cast<ProvenanceRepository*>(&repo);
	record1.setEventDuration(sample);

	record1.Serialize(testRepository);
	ProvenanceEventRecord record2;
	REQUIRE( record2.DeSerialize(testRepository,eventId) == true);
	REQUIRE( record1.getChildrenUuids().size() == 1);
	REQUIRE( record2.getChildrenUuids().size() == 1);
	std::string childId = record2.getChildrenUuids().at(0);
	REQUIRE( childId == ffr1.getUUIDStr());
	record2.removeChildUuid(childId);
	REQUIRE( record2.getChildrenUuids().size() == 0);


}



#endif
