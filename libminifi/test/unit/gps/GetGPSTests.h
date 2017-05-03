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


#ifndef GPS_TESTS
#define GPS_TESTS
#include "../../TestBase.h"

#include "../ProvenanceTestHelper.h"
#include "../Provenance.h"
#include "../FlowFileRecord.h"


TEST_CASE("Test GPSD Create", "[TestGPSEventRecord]"){

	ProvenanceEventRecord record1(ProvenanceEventRecord::ProvenanceEventType::CREATE,"blah","blahblah");
	REQUIRE( record1.getAttributes().size() == 0);
	REQUIRE( record1.getAlternateIdentifierUri().length() == 0);

}

#endif
