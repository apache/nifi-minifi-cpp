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

#include <stdlib.h>

#include <uuid/uuid.h>
#include <algorithm>
#include <string>
#include <memory>
#include <utility>
#include <map>
#include "io/BaseStream.h"
#include "TestBase.h"
#include "unit/SiteToSiteHelper.h"
#include "sitetosite/CPeer.h"
#include "sitetosite/CRawSocketProtocol.h"
#include "sitetosite/CSiteToSite.h"
#include <algorithm>
#include <core/cxxstructs.h>

#define FMT_DEFAULT fmt_lower

TEST_CASE("TestSetPortId", "[S2S1]") {
  auto stream_ptr = std::unique_ptr<minifi::io::BaseStream>(new org::apache::nifi::minifi::io::BaseStream());

  cstream cstrm;
  cstrm.impl = stream_ptr.get();

  SiteToSiteCPeer peer;
  initPeer(&peer, &cstrm, "fake_host", 65433, "");
  CRawSiteToSiteClient * protocol = (CRawSiteToSiteClient*)malloc(sizeof(CRawSiteToSiteClient));

  initRawClient(protocol, &peer);

  std::string uuid_str = "c56a4180-65aa-42ec-a945-5fd21dec0538";

  setPortId(protocol, uuid_str.c_str());

  REQUIRE(uuid_str == std::string(getPortId(protocol)));

  tearDown(protocol);

  freePeer(&peer);

  free(protocol);
}

TEST_CASE("TestSetPortIdUppercase", "[S2S2]") {
  auto stream_ptr = std::unique_ptr<minifi::io::BaseStream>(new org::apache::nifi::minifi::io::BaseStream());

  cstream cstrm;
  cstrm.impl = stream_ptr.get();

  SiteToSiteCPeer peer;
  initPeer(&peer, &cstrm, "fake_host", 65433, "");
  CRawSiteToSiteClient * protocol = (CRawSiteToSiteClient*)malloc(sizeof(CRawSiteToSiteClient));

  initRawClient(protocol, &peer);

  std::string uuid_str = "C56A4180-65AA-42EC-A945-5FD21DEC0538";

  //setPortId(protocol, uuid_str.c_str());

  //REQUIRE(uuid_str != getPortId(protocol));

  std::transform(uuid_str.begin(), uuid_str.end(), uuid_str.begin(), ::tolower);

  //REQUIRE(uuid_str == std::string(getPortId(protocol)));

  tearDown(protocol);

  freePeer(&peer);

  free(protocol);
}

void sunny_path_bootstrap(SiteToSiteResponder *collector) {
  char a = 0x14;  // RESOURCE_OK
  std::string resp_code;
  resp_code.insert(resp_code.begin(), a);
  collector->push_response(resp_code);

  // Handshake respond code
  resp_code = "R";
  collector->push_response(resp_code);
  resp_code = "C";
  collector->push_response(resp_code);
  char b = 0x1;
  resp_code = b;
  collector->push_response(resp_code);

  // Codec Negotiation
  resp_code = a;
  collector->push_response(resp_code);
}

TEST_CASE("TestSiteToSiteVerifySend", "[S2S3]") {

  SiteToSiteResponder *collector = new SiteToSiteResponder();
  sunny_path_bootstrap(collector);

  auto stream_ptr = std::unique_ptr<minifi::io::BaseStream>(new org::apache::nifi::minifi::io::BaseStream(collector));

  cstream cstrm;
  cstrm.impl = stream_ptr.get();

  SiteToSiteCPeer peer;
  initPeer(&peer, &cstrm, "fake_host", 65433, "");

  CRawSiteToSiteClient * protocol = (CRawSiteToSiteClient*)malloc(sizeof(CRawSiteToSiteClient));

  initRawClient(protocol, &peer);

  std::string uuid_str = "C56A4180-65AA-42EC-A945-5FD21DEC0538";

  setPortId(protocol, uuid_str.c_str());

  REQUIRE(0 == bootstrap(protocol));

  REQUIRE(collector->get_next_client_response() == "NiFi");
  collector->get_next_client_response();
  REQUIRE(collector->get_next_client_response() == "SocketFlowFileProtocol");
  collector->get_next_client_response();
  collector->get_next_client_response();
  collector->get_next_client_response();
  collector->get_next_client_response();
  REQUIRE(collector->get_next_client_response() == "nifi://fake_host:65433");
  collector->get_next_client_response();
  collector->get_next_client_response();
  REQUIRE(collector->get_next_client_response() == "GZIP");
  collector->get_next_client_response();
  REQUIRE(collector->get_next_client_response() == "false");
  collector->get_next_client_response();
  REQUIRE(collector->get_next_client_response() == "PORT_IDENTIFIER");
  collector->get_next_client_response();

  std::string temp_val = collector->get_next_client_response();
  std::transform(temp_val.begin(), temp_val.end(), temp_val.begin(), ::tolower);

  REQUIRE(temp_val == "c56a4180-65aa-42ec-a945-5fd21dec0538");
  collector->get_next_client_response();
  REQUIRE(collector->get_next_client_response() == "REQUEST_EXPIRATION_MILLIS");
  collector->get_next_client_response();
  REQUIRE(collector->get_next_client_response() == "30000");
  collector->get_next_client_response();
  REQUIRE(collector->get_next_client_response() == "NEGOTIATE_FLOWFILE_CODEC");
  collector->get_next_client_response();
  REQUIRE(collector->get_next_client_response() == "StandardFlowFileCodec");
  collector->get_next_client_response();  // codec version

  // start to send the stuff
  // Create the transaction
  const char * transactionID;
  const char * payload = "Test MiNiFi payload";
  CTransaction* transaction;
  transaction = createTransaction(protocol, SEND);
  REQUIRE(transaction != NULL);
  transactionID = getUUIDStr(transaction);
  collector->get_next_client_response();
  REQUIRE(collector->get_next_client_response() == "SEND_FLOWFILES");
  attribute_set as;
  as.size = 0;
  as.attributes = NULL;
  CDataPacket packet;
  initPacket(&packet, transaction, &as, payload);
  REQUIRE(sendPacket(protocol, transactionID, &packet, nullptr) == 0);
  collector->get_next_client_response();
  collector->get_next_client_response();

  std::string rx_payload = collector->get_next_client_response();
  REQUIRE(payload == rx_payload);

  freePeer(&peer);

  free(protocol);
}

TEST_CASE("TestSiteToSiteVerifyNegotiationFail", "[S2S4]") {
  SiteToSiteResponder *collector = new SiteToSiteResponder();

  char a = 0xFF;
  std::string resp_code;
  resp_code.insert(resp_code.begin(), a);
  collector->push_response(resp_code);
  collector->push_response(resp_code);

  auto stream_ptr = std::unique_ptr<minifi::io::BaseStream>(new org::apache::nifi::minifi::io::BaseStream(collector));

  cstream cstrm;
  cstrm.impl = stream_ptr.get();

  SiteToSiteCPeer peer;
  initPeer(&peer, &cstrm, "fake_host", 65433, "");

  CRawSiteToSiteClient * protocol = (CRawSiteToSiteClient*)malloc(sizeof(CRawSiteToSiteClient));

  initRawClient(protocol, &peer);

  std::string uuid_str = "C56A4180-65AA-42EC-A945-5FD21DEC0538";

  setPortId(protocol, uuid_str.c_str());

  REQUIRE(-1 == bootstrap(protocol));

  freePeer(&peer);

  free(protocol);
}
