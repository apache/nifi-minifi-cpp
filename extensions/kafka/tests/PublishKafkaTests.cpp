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
#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>

#include "MockLogger.h"
#include "MockProcessContext.h"
#include "MockProcessSession.h"
#include "MockUtils.h"
#include "PublishKafka.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"

namespace org::apache::nifi::minifi::processors::test {

TEST_CASE("Batch Size cannot be larger than Queue Max Message", "[PublishKafka]") {
  auto publish_kafka = PublishKafka(mock::getMockMetadata());
  auto context = mock::MockProcessContext{};
  context.properties_.emplace(PublishKafka::ClientName.name, "test_client");
  context.properties_.emplace(PublishKafka::SeedBrokers.name, "test_seedbroker");
  context.properties_.emplace(PublishKafka::QueueBufferMaxMessage.name, "1000");
  context.properties_.emplace(PublishKafka::BatchSize.name, "1500");

  REQUIRE_THROWS_WITH(publish_kafka.onScheduleImpl(context),
      "Process Schedule Operation: Invalid configuration: Batch Size cannot be larger than Queue Max Message");
}

TEST_CASE("Trigger without valid broker", "[PublishKafka]") {
  const auto logger = std::make_shared<mock::MockLogger>();
  auto publish_kafka = PublishKafka(mock::metadataWithLogger(logger));
  auto context = mock::MockProcessContext{};
  context.properties_.emplace(PublishKafka::ClientName.name, "test_client");
  context.properties_.emplace(PublishKafka::SeedBrokers.name, "test_seedbroker");
  context.properties_.emplace(PublishKafka::Topic.name, "test_topic");
  context.properties_.emplace(PublishKafka::MessageTimeOut.name, "10 ms");


  REQUIRE_NOTHROW(publish_kafka.onScheduleImpl(context));
  auto session = mock::MockProcessSession{};
  session.addInputFlowFile(std::make_unique<minifi_flow_file>("test_input_flow_file"));
  REQUIRE_NOTHROW(publish_kafka.onTriggerImpl(context, session));

  CHECK_FALSE(logger->logs_.empty());
}

namespace {
bool hasDebugLog(const mock::MockLogger& logger, std::string_view message) {
  const auto debug_logs = logger.logs_.find(core::logging::LOG_LEVEL::debug);
  return debug_logs != logger.logs_.end() && std::ranges::any_of(debug_logs->second, [&](const auto& log) { return log.contains(message); });
}
}  // namespace

TEST_CASE("Security properties are used for SSL connection when no SSL Context Service is set", "[PublishKafka]") {
  const auto logger = std::make_shared<mock::MockLogger>();
  auto publish_kafka = PublishKafka(mock::metadataWithLogger(logger));
  auto context = mock::MockProcessContext{};
  const auto ca_cert_path = (std::filesystem::path(TEST_RESOURCES) / "nifi-cert.pem").string();
  context.properties_.emplace(PublishKafka::ClientName.name, "test_client");
  context.properties_.emplace(PublishKafka::SeedBrokers.name, "test_seedbroker");
  context.properties_.emplace(PublishKafka::SecurityProtocol.name, "ssl");
  context.properties_.emplace(PublishKafka::SecurityCA.name, ca_cert_path);

  REQUIRE_NOTHROW(publish_kafka.onScheduleImpl(context));

  CHECK(hasDebugLog(*logger, "Kafka ssl.ca.location [" + ca_cert_path + "]"));
}

TEST_CASE("System CA certificate locations are probed when no CA certificate is set for SSL connection", "[PublishKafka]") {
  const auto logger = std::make_shared<mock::MockLogger>();
  auto publish_kafka = PublishKafka(mock::metadataWithLogger(logger));
  auto context = mock::MockProcessContext{};
  context.properties_.emplace(PublishKafka::ClientName.name, "test_client");
  context.properties_.emplace(PublishKafka::SeedBrokers.name, "test_seedbroker");
  context.properties_.emplace(PublishKafka::SecurityProtocol.name, "ssl");

  REQUIRE_NOTHROW(publish_kafka.onScheduleImpl(context));

  CHECK(hasDebugLog(*logger, "Kafka ssl.ca.location [probe]"));
}

}  // namespace org::apache::nifi::minifi::processors::test
