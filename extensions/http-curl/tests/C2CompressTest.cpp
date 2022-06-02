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

#undef NDEBUG

#include "TestBase.h"
#include "Catch.h"

#include "c2/C2Agent.h"
#include "c2/HeartbeatLogger.h"
#include "protocols/RESTProtocol.h"
#include "protocols/RESTSender.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "range/v3/action/sort.hpp"
#include "range/v3/action/unique.hpp"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/filter.hpp"
#include "range/v3/view/split.hpp"
#include "range/v3/view/transform.hpp"
#include "utils/IntegrationTestUtils.h"
#include "utils/StringUtils.h"
#include "properties/Configuration.h"
#include "io/ZlibStream.h"

class CompressedHeartbeatHandler : public HeartbeatHandler {
 protected:
  std::string readPayload(struct mg_connection* conn) override {
    auto payload = HeartbeatHandler::readPayload(conn);
    const char* encoding = mg_get_header(conn, "content-encoding");
    if (!encoding || std::string_view(encoding).find("gzip") == std::string_view::npos) {
      return payload;
    }
    received_compressed_ = true;
    minifi::io::BufferStream output;
    {
      minifi::io::ZlibDecompressStream decompressor(gsl::make_not_null(&output));
      auto ret = decompressor.write(gsl::span<const char>(payload).as_span<const std::byte>());
      assert(ret == payload.size());
    }
    auto str_span = output.getBuffer().as_span<const char>();
    return {str_span.data(), str_span.size()};
  }

 public:
  using HeartbeatHandler::HeartbeatHandler;

  std::atomic_bool received_compressed_{false};
};

class VerifyCompressedHeartbeat : public VerifyC2Base {
 public:
  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    assert(utils::verifyEventHappenedInPollTime(std::chrono::seconds(10), verify_));
  }

  void configureC2() override {
    VerifyC2Base::configureC2();
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_heartbeat_period, "100");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_rest_request_encoding, "gzip");
  }

  void setVerifier(std::function<bool()> verify) {
    verify_ = std::move(verify);
  }

 private:
  std::function<bool()> verify_;
};

int main() {
  VerifyCompressedHeartbeat harness;
  CompressedHeartbeatHandler responder(harness.getConfiguration());
  harness.setVerifier([&] () -> bool {
    return responder.received_compressed_;
  });
  harness.setUrl("https://localhost:0/heartbeat", &responder);
  harness.run();
}
