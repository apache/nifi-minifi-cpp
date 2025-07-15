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
#include "unit/TestBase.h"

#include "c2/C2Agent.h"
#include "protocols/RESTProtocol.h"
#include "protocols/RESTSender.h"
#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "io/ArchiveStream.h"
#include "unit/EmptyFlow.h"
#include "properties/Configuration.h"
#include "unit/Catch.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class VerifyDebugInfo : public VerifyC2Base {
 public:
  explicit VerifyDebugInfo(std::function<bool()> verify): verify_(std::move(verify)) {}

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTProtocol>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    REQUIRE(utils::verifyEventHappenedInPollTime(std::chrono::seconds(300), verify_));
  }

  void configureC2() override {
    VerifyC2Base::configureC2();
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_heartbeat_period, "100");
  }

  std::function<bool()> verify_;
};

class C2DebugBundleHandler : public ServerAwareHandler {
 public:
  bool handlePost(CivetServer* /*server*/, struct mg_connection *conn) override {
    std::optional<std::string> file_content;
    mg_form_data_handler form_handler{};
    form_handler.field_found = field_found;
    form_handler.field_get = field_get;
    form_handler.user_data = &file_content;
    mg_handle_form_request(conn, &form_handler);
    REQUIRE(file_content);
    {
      std::lock_guard<std::mutex> lock(mtx_);
      bundles_.push_back(std::move(*file_content));
    }
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                      "text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    return true;
  }

  std::vector<std::string> getBundles() {
    std::lock_guard<std::mutex> lock(mtx_);
    return bundles_;
  }

 private:
  static int field_found(const char* key, const char* filename, char* /*path*/, size_t /*pathlen*/, void* user_data) {
    auto& file_content = *static_cast<std::optional<std::string>*>(user_data);
    if (!filename || std::string(filename) != "debug.tar.gz") {
      throw std::runtime_error("Unknown form entry: " + std::string{key});
    }
    if (file_content) {
      throw std::runtime_error("Debug archive has already been extracted: " + std::string{key});
    }
    return MG_FORM_FIELD_STORAGE_GET;
  }
  static int field_get(const char* /*key*/, const char* value, size_t valuelen, void* user_data) {
    auto& file_content = *static_cast<std::optional<std::string>*>(user_data);
    if (!file_content) {
      file_content = "";
    }
    (*file_content) += std::string(value, valuelen);
    return MG_FORM_FIELD_HANDLE_GET;
  }

  std::mutex mtx_;
  std::vector<std::string> bundles_;
};

class C2HeartbeatHandler : public ServerAwareHandler {
 public:
  bool handlePost(CivetServer* /*server*/, struct mg_connection *conn) override {
    if (response_) {
      mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                      "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
                response_->length());
      mg_printf(conn, "%s", response_->c_str());
      response_.reset();
    } else {
      mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                      "text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    }

    return true;
  }

  void setC2RestResponse(const std::string& url) {
    response_ =
      R"({
        "operation" : "heartbeat",
        "requested_operations": [{
          "operation" : "transfer",
          "operationid" : "79",
          "name": "debug",
          "args": {"target": ")" + url + R"("}
        }]
      })";
  }

 private:
  std::optional<std::string> response_;
};

static std::string properties_file = "some.dummy.content = here\n";
static std::string flow_config_file = empty_flow;

TEST_CASE("C2DebugBundleTest", "[c2test]") {
  TestController controller;

  C2HeartbeatHandler heartbeat_handler;
  C2AcknowledgeHandler ack_handler;
  C2DebugBundleHandler bundle_handler;

  std::filesystem::path home_dir = controller.createTempDirectory();
  minifi::utils::file::PathUtils::create_dir(home_dir / "conf");
  std::ofstream{home_dir / "conf/minifi.properties", std::ios::binary} << properties_file;
  std::ofstream{home_dir / "conf/config.yml", std::ios::binary} << flow_config_file;

  VerifyDebugInfo harness([&]() -> bool {
    if (!ack_handler.isAcknowledged("79")) {
      return false;
    }
    auto bundles = bundle_handler.getBundles();
    REQUIRE(bundles.size() == 1);
    // verify the bundle
    auto bundle_stream = std::make_shared<minifi::io::BufferStream>();
    bundle_stream->write(reinterpret_cast<const uint8_t*>(bundles[0].data()), bundles[0].length());
    auto archive_provider = core::ClassLoader::getDefaultClassLoader().instantiate<minifi::io::ArchiveStreamProvider>(
        "ArchiveStreamProvider", "ArchiveStreamProvider");
    REQUIRE(archive_provider);
    auto decompressor = archive_provider->createReadStream(bundle_stream);
    REQUIRE(decompressor);
    std::map<std::string, std::string> archive_content;
    while (auto info = decompressor->nextEntry()) {
      std::string file_content;
      file_content.resize(info->size);
      REQUIRE(decompressor->read(as_writable_bytes(std::span(file_content))) ==
              file_content.length());
      archive_content[info->filename] = std::move(file_content);
    }
    REQUIRE(archive_content["minifi.properties"] == properties_file);
    REQUIRE(archive_content["config.yml"] == flow_config_file);
    auto log_gz = archive_content["minifi.log.gz"];
    auto log_stream = std::make_shared<minifi::io::BufferStream>();
    {
      minifi::io::ZlibDecompressStream log_decompressor(gsl::make_not_null(log_stream.get()));
      log_decompressor.write(reinterpret_cast<const uint8_t*>(log_gz.data()), log_gz.length());
    }
    std::string log_text;
    log_text.resize(log_stream->size());
    log_stream->read(as_writable_bytes(std::span(log_text)));
    REQUIRE(log_text.find("Tis but a scratch") != std::string::npos);
    REQUIRE(archive_content["manifest.json"].find("minifi-archive-extensions") != std::string::npos);
    return true;
  });

  harness.getConfiguration()->loadConfigureFile(home_dir / "conf/minifi.properties");
  harness.setUrl("http://localhost:0/heartbeat", &heartbeat_handler);
  harness.setUrl("http://localhost:0/acknowledge", &ack_handler);
  harness.setUrl("http://localhost:0/debug_bundle", &bundle_handler);
  harness.setC2Url("/heartbeat", "/acknowledge");

  heartbeat_handler.setC2RestResponse("http://localhost:" + harness.getWebPort() + "/debug_bundle");

  logging::LoggerFactory<C2HeartbeatHandler>::getLogger()->log_error("Tis but a scratch");

  harness.run((home_dir / "conf/config.yml").string());
}

}  // namespace org::apache::nifi::minifi::test
