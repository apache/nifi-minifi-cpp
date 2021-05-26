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
#ifndef WIN32
#include <signal.h>
#endif

#include <algorithm>
#include <chrono>
#include <string>
#include <memory>
#include <utility>
#include <map>
#include <thread>

#include "io/BaseStream.h"
#include "TestBase.h"
#include "unit/SiteToSiteHelper.h"
#include "sitetosite/CPeer.h"
#include "sitetosite/CRawSocketProtocol.h"
#include "sitetosite/CSiteToSite.h"
#include "sitetosite/RawSocketProtocol.h"
#include "core/cstructs.h"
#include "RandomServerSocket.h"
#include "core/log.h"
#include "utils/gsl.h"

#define FMT_DEFAULT fmt_lower

const char * ATTR_NAME = "some_key";
const char * ATTR_VALUE = "some value";
const char * PAYLOAD = "Test MiNiFi payload";
const char * PAYLOAD_CRC = "2006463717";  // Depends on both payload and attributes, do NOT change manually!
std::string CODEC_NAME = "StandardFlowFileCodec";

struct S2SReceivedData {
  bool request_type_ok = false;
  std::string magic_string;
  uint32_t attr_num = 0;
  std::map<std::string, std::string> attributes;
  std::vector<uint8_t> payload;
};

// This struct is used to sync between simulated clients and servers
struct TransferState {
  std::atomic<bool> handshake_data_processed{false};
  std::atomic<bool> transer_completed{false};
  std::atomic<bool> data_processed{false};
};

void wait_until(std::atomic<bool>& b) {
  while (!b) {
    std::this_thread::sleep_for(std::chrono::milliseconds(0));  // Just yield
  }
}

TEST_CASE("TestSetPortId", "[S2S1]") {
#ifndef WIN32
  signal(SIGPIPE, SIG_IGN);
#endif
  SiteToSiteCPeer peer;
  initPeer(&peer, "fake_host", 65433);
  CRawSiteToSiteClient * protocol = (CRawSiteToSiteClient*)malloc(sizeof(CRawSiteToSiteClient));

  initRawClient(protocol, &peer);

  std::string uuid_str = "c56a4180-65aa-42ec-a945-5fd21dec0538";

  setPortId(protocol, uuid_str.c_str());

  REQUIRE(uuid_str == std::string(getPortId(protocol)));

  tearDown(protocol);

  freePeer(&peer);

  free(protocol);
}

void send_response_code(minifi::io::BaseStream* stream, uint8_t resp) {
  std::array<uint8_t, 3> resp_codes = {'R', 'C', resp};
  for (uint8_t r : resp_codes) {
    stream->write(&r, 1);
  }
}

void accept_transfer(minifi::io::BaseStream* stream, const std::string& crcstr, TransferState& transfer_state, S2SReceivedData& s2s_data) {
  // In long term it would be nice to calculate the crc of the received data here
  send_response_code(stream, 12);  // confirmed
  stream->write(crcstr);
  send_response_code(stream, 13);  // transaction finished

  wait_until(transfer_state.transer_completed);

  std::string requesttype;
  stream->read(requesttype);

  if (requesttype == "SEND_FLOWFILES") {
    s2s_data.request_type_ok = true;
    stream->read(s2s_data.attr_num);
    std::string key, value;
    for (uint32_t i = 0; i < s2s_data.attr_num; ++i) {
      stream->read(key, true);
      stream->read(value, true);
      s2s_data.attributes[key] = value;
    }
    uint64_t content_size = 0;
    stream->read(content_size);
    s2s_data.payload.resize(content_size);
    stream->read(s2s_data.payload, content_size);
  } else {
    s2s_data.request_type_ok = false;
  }
  transfer_state.data_processed = true;
}

void sunny_path_bootstrap(minifi::io::BaseStream* stream, TransferState& transfer_state, S2SReceivedData& s2s_data) {
  // Verify the magic string
  char c_array[4];
  stream->read((uint8_t*)c_array, 4);
  s2s_data.magic_string = std::string(c_array, 4);
  uint8_t success = 0x14;
  stream->write(&success, 1);
  send_response_code(stream, 0x1);
  stream->write(&success, 1);

  // just consume handshake data
  bool found_codec = false;
  size_t read_len = 0;
  while (!found_codec) {
    uint8_t handshake_data[1000];
    const auto actual_len = stream->read(handshake_data+read_len, 1000-read_len);
    if(actual_len == 0 || minifi::io::isError(actual_len)) {
      continue;
    }
    read_len += actual_len;
    const std::string incoming_data(reinterpret_cast<const char *>(handshake_data), read_len);
    const auto it = std::search(incoming_data.begin(), incoming_data.end(), CODEC_NAME.begin(), CODEC_NAME.end());
    if(it != incoming_data.end()){
      const auto idx = gsl::narrow<size_t>(std::distance(incoming_data.begin(), it));
      // Actual version follows the string as an uint32_t // that should be the end of the buffer
      found_codec = idx + CODEC_NAME.length() + sizeof(uint32_t) == read_len;
    }
  }

  transfer_state.handshake_data_processed = true;
}

void different_version_bootstrap(minifi::io::BaseStream* stream, TransferState& transfer_state, S2SReceivedData& s2s_data) {
  uint8_t resp_code = 0x15;
  stream->write(&resp_code, 1);

  uint32_t version = 4;
  stream->write(version);

  sunny_path_bootstrap(stream, transfer_state, s2s_data);
}

TEST_CASE("TestSiteToBootStrap", "[S2S3]") {
#ifndef WIN32
  signal(SIGPIPE, SIG_IGN);
#endif
  std::array<std::function<void(minifi::io::BaseStream*, TransferState&, S2SReceivedData&)>, 2> bootstrap_functions =
      {sunny_path_bootstrap, different_version_bootstrap};

  for (const auto& bootstrap_func : bootstrap_functions) {
    TransferState transfer_state;
    S2SReceivedData received_data;
    std::unique_ptr<minifi::io::ServerSocket> sckt(new minifi::io::RandomServerSocket("localhost"));
    uint16_t port = sckt->getPort();

    sckt->registerCallback([]() -> bool { return true; },  [&bootstrap_func, &transfer_state, &received_data](minifi::io::BaseStream* stream)
      {bootstrap_func(stream, transfer_state, received_data); accept_transfer(stream, PAYLOAD_CRC, transfer_state, received_data); });

    bool c_handshake_ok = false;
    bool c_transfer_ok = false;

    auto c_client_thread = [&transfer_state, &c_handshake_ok, &c_transfer_ok, port]() {
      SiteToSiteCPeer cpeer;
      initPeer(&cpeer, "localhost", port);

      CRawSiteToSiteClient cprotocol;

      initRawClient(&cprotocol, &cpeer);

      std::string uuid_str = "C56A4180-65AA-42EC-A945-5FD21DEC0538";

      c_handshake_ok = bootstrap(&cprotocol) == 0;

      const char * payload = PAYLOAD;

      attribute attribute1;

      attribute1.key = ATTR_NAME;
      const char * attr_value = ATTR_VALUE;
      attribute1.value = (void *)attr_value;
      attribute1.value_size = strlen(attr_value);

      attribute_set as;
      as.size = 1;
      as.attributes = &attribute1;

      wait_until(transfer_state.handshake_data_processed);
      c_transfer_ok = (transmitPayload(&cprotocol, payload, &as) == 0);
      transfer_state.transer_completed = true;

      destroyClient(&cprotocol);
      freePeer(&cpeer);
    };

    std::thread c_thread(c_client_thread);
    c_thread.join();
    wait_until(transfer_state.data_processed);

    REQUIRE(c_handshake_ok == true);
    REQUIRE(c_transfer_ok == true);

    REQUIRE(received_data.magic_string == "NiFi");
    REQUIRE(received_data.request_type_ok);
    REQUIRE(received_data.attr_num == 1);
    REQUIRE(received_data.attributes[ATTR_NAME] == ATTR_VALUE);
    REQUIRE(std::string(reinterpret_cast<const char*>(received_data.payload.data()), received_data.payload.size()) == PAYLOAD);
  }
}
