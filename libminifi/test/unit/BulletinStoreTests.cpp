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
#include "unit/ProcessorUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class BulletinStoreTestAccessor {
 public:
  static std::deque<core::Bulletin>& getBulletins(core::BulletinStore& store) {
    return store.bulletins_;
  }
};

std::unique_ptr<core::Processor> createDummyProcessor(const std::string& processor_uuid = "4d7fa7e6-2459-46dd-b2ba-61517239edf5") {
  auto processor = test::utils::make_processor<DummyProcessor>("DummyProcessor", minifi::utils::Identifier::parse(processor_uuid).value());
  processor->setProcessGroupUUIDStr("68fa9ae4-b9fc-4873-b0d9-edab59fdb0c2");
  processor->setProcessGroupName("sub_group");
  processor->setProcessGroupPath("root / sub_group");
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
  CHECK(bulletins[0].id == 2);
  CHECK(bulletins[0].level == "WARN");
  CHECK(bulletins[0].category == "Log Message");
  CHECK(bulletins[0].message == "Warning message");
  CHECK(bulletins[0].group_id == "68fa9ae4-b9fc-4873-b0d9-edab59fdb0c2");
  CHECK(bulletins[0].group_name == "sub_group");
  CHECK(bulletins[0].group_path == "root / sub_group");
  CHECK(bulletins[0].source_id == "4d7fa7e6-2459-46dd-b2ba-61517239edf5");
  CHECK(bulletins[0].source_name == "DummyProcessor");
  CHECK(bulletins[1].id == 3);
}

TEST_CASE("Return all bulletins when no time interval is defined", "[bulletinStore]") {
  ConfigureImpl configuration;
  core::BulletinStore bulletin_store(configuration);
  auto processor = createDummyProcessor();
  for (size_t i = 0; i < 3; ++i) {
    bulletin_store.addProcessorBulletin(*processor, logging::LOG_LEVEL::warn, "Warning message");
  }
  auto bulletins = bulletin_store.getBulletins();
  REQUIRE(bulletins.size() == 3);
  CHECK(bulletins[0].id == 1);
  CHECK(bulletins[0].level == "WARN");
  CHECK(bulletins[0].category == "Log Message");
  CHECK(bulletins[0].message == "Warning message");
  CHECK(bulletins[0].group_id == "68fa9ae4-b9fc-4873-b0d9-edab59fdb0c2");
  CHECK(bulletins[0].group_name == "sub_group");
  CHECK(bulletins[0].group_path == "root / sub_group");
  CHECK(bulletins[0].source_id == "4d7fa7e6-2459-46dd-b2ba-61517239edf5");
  CHECK(bulletins[0].source_name == "DummyProcessor");
  CHECK(bulletins[1].id == 2);
  CHECK(bulletins[2].id == 3);
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
  CHECK(bulletins[0].id == 2);
  CHECK(bulletins[0].level == "WARN");
  CHECK(bulletins[0].category == "Log Message");
  CHECK(bulletins[0].message == "Warning message");
  CHECK(bulletins[0].group_id == "68fa9ae4-b9fc-4873-b0d9-edab59fdb0c2");
  CHECK(bulletins[0].group_name == "sub_group");
  CHECK(bulletins[0].group_path == "root / sub_group");
  CHECK(bulletins[0].source_id == "4d7fa7e6-2459-46dd-b2ba-61517239edf5");
  CHECK(bulletins[0].source_name == "DummyProcessor");
  CHECK(bulletins[1].id == 3);
}

TEST_CASE("Return bulletins for a specific processor", "[bulletinStore]") {
  ConfigureImpl configuration;
  core::BulletinStore bulletin_store(configuration);
  auto processor1 = createDummyProcessor();
  auto processor2 = createDummyProcessor("147a7f22-b65c-48ff-ac19-1b504f6dbaaf");
  for (size_t i = 0; i < 2; ++i) {
    bulletin_store.addProcessorBulletin(*processor1, logging::LOG_LEVEL::warn, "Warning message 1");
  }
  for (size_t i = 0; i < 2; ++i) {
    bulletin_store.addProcessorBulletin(*processor2, logging::LOG_LEVEL::warn, "Warning message 2");
  }
  for (size_t i = 0; i < 2; ++i) {
    bulletin_store.addProcessorBulletin(*processor1, logging::LOG_LEVEL::warn, "Warning message 3");
  }

  auto bulletins = bulletin_store.getBulletinsForProcessor("147a7f22-b65c-48ff-ac19-1b504f6dbaaf");
  REQUIRE(bulletins.size() == 2);
  CHECK(bulletins[0].id == 3);
  CHECK(bulletins[1].id == 4);
  CHECK(bulletins[0].message == "Warning message 2");
}

}  // namespace org::apache::nifi::minifi::test
