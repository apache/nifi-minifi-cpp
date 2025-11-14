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
#include <mutex>

#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "minifi-cpp/utils/gsl.h"
#include "unit/TestUtils.h"
#include "unit/EmptyFlow.h"
#include "fmt/format.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/sinks/dist_sink.h"
#include "properties/PropertiesFile.h"
#include "ConfigTestAccessor.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

struct PropertyChange {
  std::string name;
  std::string value;
  std::optional<bool> persist;
};

class C2HeartbeatHandler : public ServerAwareHandler {
 public:
  bool handlePost(CivetServer* /*server*/, struct mg_connection *conn) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (response_) {
      mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                      "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
                response_->length());
      mg_printf(conn, "%s", response_->c_str());
    } else {
      mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                      "text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    }

    return true;
  }

  void setProperties(const std::vector<PropertyChange>& changes) {
    std::vector<std::string> fields;
    for (const auto& change : changes) {
      if (change.persist.has_value()) {
        fields.push_back(fmt::format(R"("{}": {{"value": "{}", "persist": {}}})", change.name, change.value, change.persist.value()));
      } else {
        fields.push_back(fmt::format(R"("{}": "{}")", change.name, change.value));
      }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    response_ =
        R"({
        "operation" : "heartbeat",
        "requested_operations": [{
          "operation" : "update",
          "operationid" : "79",
          "name": "properties",
          "args": {)" +
            minifi::utils::string::join(", ", fields)
          + R"(}
        }]
      })";
  }

 private:
  std::mutex mutex_;
  std::optional<std::string> response_;
};

class VerifyPropertyUpdate : public HTTPIntegrationBase {
 public:
  VerifyPropertyUpdate() : fn_{[]{}} {}
  explicit VerifyPropertyUpdate(const std::filesystem::path& test_file_path, std::function<void()> fn)
    : HTTPIntegrationBase(test_file_path),
      fn_(std::move(fn)) {}
  VerifyPropertyUpdate(const VerifyPropertyUpdate&) = delete;
  VerifyPropertyUpdate(VerifyPropertyUpdate&&) = default;
  VerifyPropertyUpdate& operator=(const VerifyPropertyUpdate&) = delete;
  VerifyPropertyUpdate& operator=(VerifyPropertyUpdate&&) = default;
  ~VerifyPropertyUpdate() override = default;

  void testSetup() override {}

  void runAssertions() override {
    fn_();
  }

  std::function<void()> fn_;

  [[nodiscard]] int getRestartRequestedCount() const noexcept { return restart_requested_count_; }
};

static const std::string properties_file =
    "nifi.property.one=tree\n"
    "nifi.c2.enable=true\n"
    "nifi.c2.agent.class=test\n"
    "nifi.c2.agent.heartbeat.period=500\n";

static const std::string log_properties_file =
    "logger.root=INFO,ostream\n";

using namespace std::literals::chrono_literals;

struct DummyClass1 {};
struct DummyClass2 {};
namespace dummy {
struct DummyClass3 {};
}  // namespace dummy

TEST_CASE("C2PropertiesUpdateTests", "[c2test]") {
  TempDirectory tmp_dir;

  std::filesystem::path home_dir = tmp_dir.getPath();

  minifi::utils::file::PathUtils::create_dir(home_dir / "conf");
  std::ofstream{home_dir / "conf/minifi.properties"} << properties_file;
  std::ofstream{home_dir / "conf/minifi-log.properties"} << log_properties_file;
  std::ofstream{home_dir / "conf/config.yml"} << empty_flow;

  C2HeartbeatHandler hb_handler{};
  C2AcknowledgeHandler ack_handler{};

  auto logger_properties = std::make_shared<core::logging::LoggerProperties>(home_dir / "logs");
  // this sets the ostream logger
  auto log_test_controller = LogTestController::getInstance(logger_properties);

  logger_properties->loadConfigureFile(home_dir / "conf/minifi-log.properties");
  core::logging::LoggerConfiguration::getConfiguration().initialize(logger_properties);

  auto logger1 = core::logging::LoggerFactory<DummyClass1>::getLogger();
  auto logger2 = core::logging::LoggerFactory<DummyClass2>::getLogger();
  auto logger3 = core::logging::LoggerFactory<dummy::DummyClass3>::getLogger();

  {
    // verify initial log levels, none of these should be logged
    logger1->log_debug("DummyClass1::before");
    logger2->log_debug("DummyClass2::before");
    logger3->log_debug("DummyClass3::before");

    CHECK(!log_test_controller->contains("DummyClass1::before", 0s));
    CHECK(!log_test_controller->contains("DummyClass2::before", 0s));
    CHECK(!log_test_controller->contains("DummyClass3::before", 0s));
  }

  // On msvc, the passed lambda can't capture a reference to the object under construction, so we need to late-init harness.
  VerifyPropertyUpdate harness;
  harness = VerifyPropertyUpdate(home_dir / "conf/config.yml", [&] {
    REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&] {return ack_handler.isAcknowledged("79");}));
    REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&] {
      return ack_handler.getApplyCount("FULLY_APPLIED") == 1;
    }));

    // Updating the same property will result in a no operation response
    REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&] {
      return ack_handler.getApplyCount("NO_OPERATION") > 0;
    }));

    // Change the update response to 1 invalid and 1 valid value update
    hb_handler.setProperties({{minifi::Configuration::nifi_c2_rest_heartbeat_minimize_updates, "banana", true}, {minifi::Configuration::minifi_disk_space_watchdog_enable, "true", true}});

    // Due to 1 invalid value the result will be partially applied
    REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&] {
      return ack_handler.getApplyCount("PARTIALLY_APPLIED") == 1;
    }));

    // Repeating the previous update request results in 1 no operation and 1 failure which results in not applied response
    REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&] {
      return ack_handler.getApplyCount("NOT_APPLIED") > 0
        && harness.getRestartRequestedCount() == 2;
    }));

    // update operation acknowledged
    {
      // verify final log levels
      logger1->log_debug("DummyClass1::after");
      logger2->log_debug("DummyClass2::after");  // this should still not log
      logger3->log_debug("DummyClass3::after");
    }
    CHECK(log_test_controller->contains("DummyClass1::after", 0s));
    CHECK_FALSE(log_test_controller->contains("DummyClass2::after", 0s));
    CHECK(log_test_controller->contains("DummyClass3::after", 0s));

    {
      const std::shared_ptr<minifi::Configure> minifi_properties = std::make_shared<minifi::ConfigureImpl>();
      minifi_properties->loadConfigureFile(home_dir / "conf" / "minifi.properties");
      CHECK_FALSE(minifi_properties->get("nifi.dummy.property"));
      CHECK(minifi_properties->get("nifi.property.one") == "bush");
      CHECK(minifi_properties->get("nifi.property.two") == "ring");
      CHECK_FALSE(minifi_properties->get(minifi::Configuration::nifi_c2_rest_heartbeat_minimize_updates));
      CHECK(minifi_properties->get(minifi::Configuration::minifi_disk_space_watchdog_enable) == "true");
    }

    {
      const std::shared_ptr<minifi::Configure> minifi_log_properties = std::make_shared<minifi::ConfigureImpl>();
      minifi_log_properties->loadConfigureFile(home_dir / "conf" / "minifi-log.properties");
      CHECK_FALSE(minifi_log_properties->get("logger.org::apache::nifi::minifi::test::dummy"));
      CHECK(minifi_log_properties->get("logger.org::apache::nifi::minifi::test::DummyClass1") == "DEBUG,ostream");
    }
  });

  harness.getConfiguration()->loadConfigureFile(home_dir / "conf/minifi.properties");
  ConfigTestAccessor::call_setLoggerProperties(harness.getConfiguration(), logger_properties);

  harness.setUrl("http://localhost:0/heartbeat", &hb_handler);
  harness.setUrl("http://localhost:0/acknowledge", &ack_handler);
  harness.setC2Url("/heartbeat", "/acknowledge");

  hb_handler.setProperties({
    {"nifi.dummy.property", "banana", false},
    {"nifi.property.one", "bush", std::nullopt},  // default persist = true
    {"nifi.property.two", "ring", true},
    {"nifi.log.logger.org::apache::nifi::minifi::test::dummy", "DEBUG,ostream", false},
    {"nifi.log.logger.org::apache::nifi::minifi::test::DummyClass1", "DEBUG,ostream", true}
  });

  harness.run();
}

}  // namespace org::apache::nifi::minifi::test
