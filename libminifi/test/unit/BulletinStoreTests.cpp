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
#include <memory>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/BulletinStore.h"
#include "properties/Configure.h"
#include "unit/DummyProcessor.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class BulletinStoreTestAccessor {
 public:
  static std::deque<core::Bulletin>& getBulletins(core::BulletinStore& store) {
    return store.bulletins_;
  }
};

std::unique_ptr<core::Processor> createDummyProcessor() {
  auto processor = std::make_unique<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse("4d7fa7e6-2459-46dd-b2ba-61517239edf5").value());
  processor->setProcessGroupUUIDStr("68fa9ae4-b9fc-4873-b0d9-edab59fdb0c2");
  processor->setProcessGroupName("sub_group");
  processor->setProcessGroupPath("root/sub_group");
  return processor;
}

TEST_CASE("Create BulletinStore with default max size of 1000", "[bulletinStore]") {
  ConfigureImpl configuration;
  SECTION("No limit is configured") {}
  SECTION("Invalid value is configured") {
    configuration.set(Configure::nifi_c2_flow_info_processor_bulletin_limit, "invalid");
  }
  core::BulletinStore bulletin_store(configuration);
  REQUIRE(bulletin_store.getMaxBulletinCount() == 1000);
}

TEST_CASE("Create BulletinStore with custom max size of 10000", "[bulletinStore]") {
  ConfigureImpl configuration;
  configuration.set(Configure::nifi_c2_flow_info_processor_bulletin_limit, "10000");
  core::BulletinStore bulletin_store(configuration);
  REQUIRE(bulletin_store.getMaxBulletinCount() == 10000);
}

TEST_CASE("Remove oldest entries when limit is reached", "[bulletinStore]") {
  ConfigureImpl configuration;
  configuration.set(Configure::nifi_c2_flow_info_processor_bulletin_limit, "2");
  core::BulletinStore bulletin_store(configuration);
  auto processor = createDummyProcessor();
  for (size_t i = 0; i < 3; ++i) {
    bulletin_store.addProcessorBulletin(*processor, logging::LOG_LEVEL::warn, "Warning message");
  }
  auto bulletins = bulletin_store.getBulletins();
  REQUIRE(bulletins.size() == 2);
  REQUIRE(bulletins[0].id == 2);
  REQUIRE(bulletins[1].id == 3);
  REQUIRE(bulletins[0].message == "Warning message");
}

TEST_CASE("Return all bulletins when no time interval is defined or all entries are part of the time interval", "[bulletinStore]") {
  ConfigureImpl configuration;
  core::BulletinStore bulletin_store(configuration);
  auto processor = createDummyProcessor();
  for (size_t i = 0; i < 3; ++i) {
    bulletin_store.addProcessorBulletin(*processor, logging::LOG_LEVEL::warn, "Warning message");
  }
  auto bulletins = bulletin_store.getBulletins();
  REQUIRE(bulletins.size() == 3);
  REQUIRE(bulletins[0].id == 1);
  REQUIRE(bulletins[1].id == 2);
  REQUIRE(bulletins[2].id == 3);
  REQUIRE(bulletins[2].message == "Warning message");
}

TEST_CASE("Return only bulletins that are inside the defined time interval", "[bulletinStore]") {
  ConfigureImpl configuration;
  core::BulletinStore bulletin_store(configuration);
  auto processor = createDummyProcessor();
  for (size_t i = 0; i < 3; ++i) {
    bulletin_store.addProcessorBulletin(*processor, logging::LOG_LEVEL::warn, "Warning message");
  }
  BulletinStoreTestAccessor::getBulletins(bulletin_store)[0].timestamp -= 5min;

  auto bulletins = bulletin_store.getBulletins(3min);
  REQUIRE(bulletins.size() == 2);
  REQUIRE(bulletins[0].id == 2);
  REQUIRE(bulletins[1].id == 3);
  REQUIRE(bulletins[0].message == "Warning message");
}

}  // namespace org::apache::nifi::minifi::test
