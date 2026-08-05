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

#include <array>
#include <chrono>
#include <random>
#include <vector>

#include "RocksDbProvenanceRepository.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

static constexpr size_t TEST_PROVENANCE_STORAGE_SIZE = 100_KiB;
static constexpr size_t TEST_MAX_PROVENANCE_STORAGE_SIZE = 100_MiB;

using namespace std::literals::chrono_literals;

void generateData(std::vector<char>& data) {
  std::random_device rd;
  std::mt19937 eng(rd());

  std::uniform_int_distribution<> distr(std::numeric_limits<char>::min(), std::numeric_limits<char>::max());
  auto rand = [&distr, &eng] { return distr(eng); };
  std::generate_n(data.begin(), data.size(), rand);
}

void provisionRepo(minifi::provenance::ProvenanceRepository& repo, size_t number_of_records, size_t record_size) {
  for (size_t i = 0; i < number_of_records; ++i) {
    std::vector<char> v(record_size);
    generateData(v);
    REQUIRE(repo.Put(std::to_string(i), reinterpret_cast<const uint8_t*>(v.data()), v.size()));
  }
}

void verifyMaxKeyCount(const minifi::provenance::ProvenanceRepository& repo, uint64_t keyCount) {
  uint64_t k = std::numeric_limits<uint64_t>::max();

  for (int i = 0; i < 50; ++i) {
    std::this_thread::sleep_for(100ms);
    k = std::min(k, repo.getRepositoryEntryCount());
    if (k < keyCount) {
      break;
    }
  }

  REQUIRE(k < keyCount);
}

std::vector<std::byte> serializeEvent(minifi::provenance::ProvenanceEventRecord& event) {
  minifi::io::BufferStream stream;
  event.serialize(stream);
  return stream.moveBuffer();
}

template<typename T>
void appendAll(std::vector<T>& sink, const std::vector<T>& source) {
  sink.insert(sink.end(), source.begin(), source.end());
}

TEST_CASE("Test size limit", "[sizeLimitTest]") {
  TestController testController;
  auto temp_dir = testController.createTempDirectory();
  REQUIRE(!temp_dir.empty());

  // 60 sec, 100 KB - going to exceed the size limit
  minifi::provenance::RocksDbProvenanceRepository provdb("TestProvRepo", temp_dir.string(), 1min, TEST_PROVENANCE_STORAGE_SIZE, 1s);

  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, temp_dir.string());

  REQUIRE(provdb.initialize(configuration));

  size_t keyCount = 500;

  provisionRepo(provdb, keyCount, 10240);

  verifyMaxKeyCount(provdb, 200);
}

TEST_CASE("Test time limit", "[timeLimitTest]") {
  TestController testController;
  auto temp_dir = testController.createTempDirectory();
  REQUIRE(!temp_dir.empty());

  // 1 sec, 100 MB - going to exceed TTL
  minifi::provenance::RocksDbProvenanceRepository provdb("TestProvRepo", temp_dir.string(), 1s, TEST_MAX_PROVENANCE_STORAGE_SIZE, 1s);

  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, temp_dir.string());

  REQUIRE(provdb.initialize(configuration));

  size_t keyCount = 500;

  provisionRepo(provdb, keyCount / 2, 102400);

  /**
   * Magic: TTL-based DB cleanup only triggers when writeBuffers are serialized to storage
   * To achieve this 250 entries are put to DB with a total size that ensures at least one buffer is serialized
   * Wait 2 seconds to make sure the serialized records expire
   * Put another set of entries to trigger cleanup logic to drop the already serialized records
   * This tests relies on the default settings of Provenance repo: a size of a writeBuffer is 16 MB
   * One provisioning call here writes 25 MB to make sure serialization is triggered
   * When the 2nd 50 MB is written the records of the 1st serialization are dropped -> around 160 of them
   * That's why the final check verifies keyCount to be below 400
   */
  std::this_thread::sleep_for(2s);

  provisionRepo(provdb, keyCount /2, 102400);

  verifyMaxKeyCount(provdb, 400);
}

TEST_CASE("Test query elements after cursor", "[iterationTest]") {
  TestController testController;
  auto temp_dir = testController.createTempDirectory();
  REQUIRE(!temp_dir.empty());

  minifi::provenance::RocksDbProvenanceRepository provdb("TestProvRepo", temp_dir.string(), 1s, TEST_MAX_PROVENANCE_STORAGE_SIZE, 1s);

  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, temp_dir.string());

  REQUIRE(provdb.initialize(configuration));

  std::vector<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> events;
  for (size_t i = 0; i < 8; ++i) {
    events.push_back(minifi::provenance::ProvenanceEventRecord::create());
  }

  REQUIRE(provdb.appendEvents(events));

  auto cursor = provdb.cursorFromString(std::nullopt);
  REQUIRE(cursor);

  std::vector<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> queried_events;

  appendAll(queried_events, provdb.getEvents(3, cursor.get()).value());
  REQUIRE(queried_events.size() == 3);
  appendAll(queried_events, provdb.getEvents(3, cursor.get()).value());
  REQUIRE(queried_events.size() == 6);
  appendAll(queried_events, provdb.getEvents(3, cursor.get()).value());
  REQUIRE(queried_events.size() == 8);

  std::string last_event_id;

  for (size_t i = 0; i < queried_events.size(); ++i) {
    REQUIRE(last_event_id < std::string{queried_events.at(i)->getUUIDStr()});
    last_event_id = queried_events.at(i)->getUUIDStr();
    REQUIRE(serializeEvent(*events.at(i)) == serializeEvent(*queried_events.at(i)));
  }
}

TEST_CASE("Test loading cursor from string", "[cursorSerializationTest]") {
  TestController testController;
  auto temp_dir = testController.createTempDirectory();
  REQUIRE(!temp_dir.empty());

  minifi::provenance::RocksDbProvenanceRepository provdb("TestProvRepo", temp_dir.string(), 1s, TEST_MAX_PROVENANCE_STORAGE_SIZE, 1s);

  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, temp_dir.string());

  REQUIRE(provdb.initialize(configuration));

  std::vector<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> events;
  for (size_t i = 0; i < 8; ++i) {
    events.push_back(minifi::provenance::ProvenanceEventRecord::create());
  }

  REQUIRE(provdb.appendEvents(events));

  auto cursor = provdb.cursorFromString(std::nullopt);
  REQUIRE(cursor);

  std::vector<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> queried_events;

  appendAll(queried_events, provdb.getEvents(3, cursor.get()).value());
  REQUIRE(queried_events.size() == 3);

  cursor = provdb.cursorFromString(cursor->toString());
  REQUIRE(cursor);

  appendAll(queried_events, provdb.getEvents(3, cursor.get()).value());
  REQUIRE(queried_events.size() == 6);

  std::string last_event_id;

  for (size_t i = 0; i < queried_events.size(); ++i) {
    REQUIRE(last_event_id < std::string{queried_events.at(i)->getUUIDStr()});
    last_event_id = queried_events.at(i)->getUUIDStr();
    REQUIRE(serializeEvent(*events.at(i)) == serializeEvent(*queried_events.at(i)));
  }
}

TEST_CASE("Test opening existing database loads monotonic counter", "[eventUuidMonotonicTest]") {
  TestController testController;
  auto temp_dir = testController.createTempDirectory();
  REQUIRE(!temp_dir.empty());

  auto provdb = std::make_unique<minifi::provenance::RocksDbProvenanceRepository>("TestProvRepo", temp_dir.string(), 1s, TEST_MAX_PROVENANCE_STORAGE_SIZE, 1s);

  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, temp_dir.string());

  REQUIRE(provdb->initialize(configuration));

  std::vector<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> events;
  for (size_t i = 0; i < 4; ++i) {
    events.push_back(minifi::provenance::ProvenanceEventRecord::create());
  }

  REQUIRE(provdb->appendEvents(events));

  provdb = std::make_unique<minifi::provenance::RocksDbProvenanceRepository>("TestProvRepo", temp_dir.string(), 1s, TEST_MAX_PROVENANCE_STORAGE_SIZE, 1s);

  REQUIRE(provdb->initialize(configuration));

  std::vector<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> new_events;
  for (size_t i = 0; i < 4; ++i) {
    new_events.push_back(minifi::provenance::ProvenanceEventRecord::create());
    events.push_back(new_events.back());
  }

  REQUIRE(provdb->appendEvents(new_events));

  std::vector<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> queried_events = provdb->getEvents(8, nullptr).value();
  REQUIRE(queried_events.size() == 8);

  std::string last_event_id;

  for (size_t i = 0; i < queried_events.size(); ++i) {
    REQUIRE(last_event_id < std::string{queried_events.at(i)->getUUIDStr()});
    last_event_id = queried_events.at(i)->getUUIDStr();
    REQUIRE(serializeEvent(*events.at(i)) == serializeEvent(*queried_events.at(i)));
  }
}
