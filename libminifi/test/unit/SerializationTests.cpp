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


#include "io/BaseStream.h"
#include "Site2SitePeer.h"
#include "Site2SiteClientProtocol.h"
#include <uuid/uuid.h>
#include <algorithm>
#include <string>
#include <memory>
#include "../TestBase.h"
#define FMT_DEFAULT fmt_lower



TEST_CASE("TestSetPortId", "[S2S1]"){


	Site2SitePeer peer(std::unique_ptr<DataStream>(new DataStream()),"fake_host",65433);

	Site2SiteClientProtocol protocol(&peer);


	std::string uuid_str = "c56a4180-65aa-42ec-a945-5fd21dec0538";

	uuid_t fakeUUID;

	uuid_parse(uuid_str.c_str(),fakeUUID);

	protocol.setPortId(fakeUUID);

	REQUIRE( uuid_str == protocol.getPortId() );



}

TEST_CASE("TestSetPortIdUppercase", "[S2S2]"){


	Site2SitePeer peer(std::unique_ptr<DataStream>(new DataStream()),"fake_host",65433);

	Site2SiteClientProtocol protocol(&peer);


	std::string uuid_str = "C56A4180-65AA-42EC-A945-5FD21DEC0538";

	uuid_t fakeUUID;

	uuid_parse(uuid_str.c_str(),fakeUUID);

	protocol.setPortId(fakeUUID);

	REQUIRE( uuid_str != protocol.getPortId() );

	std::transform(uuid_str.begin(),uuid_str.end(),uuid_str.begin(),::tolower);

	REQUIRE( uuid_str == protocol.getPortId() );



}


TEST_CASE("TestWriteUTF", "[MINIFI193]"){

  DataStream baseStream;

  Serializable ser;

  std::string stringOne = "helo world"; // yes, this has a typo.
  std::string verifyString;
  ser.writeUTF(stringOne,&baseStream,false);


  ser.readUTF(verifyString,&baseStream,false);

  REQUIRE(verifyString == stringOne);




}




TEST_CASE("TestWriteUTF2", "[MINIFI193]"){

  DataStream baseStream;

  Serializable ser;

  std::string stringOne = "hel\xa1o world";
  REQUIRE(11 == stringOne.length());
  std::string verifyString;
  ser.writeUTF(stringOne,&baseStream,false);


  ser.readUTF(verifyString,&baseStream,false);

  REQUIRE(verifyString == stringOne);




}


TEST_CASE("TestWriteUTF3", "[MINIFI193]"){

  DataStream baseStream;

  Serializable ser;

  std::string stringOne = "\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c";
  REQUIRE(12 == stringOne.length());
  std::string verifyString;
  ser.writeUTF(stringOne,&baseStream,false);


  ser.readUTF(verifyString,&baseStream,false);

  REQUIRE(verifyString == stringOne);




}
