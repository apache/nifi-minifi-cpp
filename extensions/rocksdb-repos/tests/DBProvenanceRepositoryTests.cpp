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
#include <filesystem>
#include <memory>
#include <random>
#include <string>
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

std::vector<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> createEvents(size_t count) {
  std::vector<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> events;
  events.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    events.push_back(minifi::provenance::ProvenanceEventRecord::create());
  }
  return events;
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

  auto events = createEvents(8);

  REQUIRE(provdb.appendEvents(events));

  auto cursor = provdb.cursorFromString("");
  REQUIRE(cursor);

  std::vector<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> queried_events;

  appendAll(queried_events, provdb.getEvents(3, cursor.get()).value());
  REQUIRE(queried_events.size() == 3);
  appendAll(queried_events, provdb.getEvents(3, cursor.get()).value());
  REQUIRE(queried_events.size() == 6);
  appendAll(queried_events, provdb.getEvents(3, cursor.get()).value());
  REQUIRE(queried_events.size() == 8);

  for (size_t i = 0; i < queried_events.size(); ++i) {
    REQUIRE(queried_events.at(i)->getEventOrdinal() == i + 1);
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

  auto events = createEvents(8);

  REQUIRE(provdb.appendEvents(events));

  auto cursor = provdb.cursorFromString("");
  REQUIRE(cursor);

  std::vector<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> queried_events;

  appendAll(queried_events, provdb.getEvents(3, cursor.get()).value());
  REQUIRE(queried_events.size() == 3);

  // the cursor is persisted as the ordinal of the last event read
  REQUIRE(cursor->toString() == "3");

  cursor = provdb.cursorFromString(cursor->toString());
  REQUIRE(cursor);

  appendAll(queried_events, provdb.getEvents(3, cursor.get()).value());
  REQUIRE(queried_events.size() == 6);

  for (size_t i = 0; i < queried_events.size(); ++i) {
    REQUIRE(queried_events.at(i)->getEventOrdinal() == i + 1);
    REQUIRE(serializeEvent(*events.at(i)) == serializeEvent(*queried_events.at(i)));
  }
}

TEST_CASE("Test appendEvents assigns consecutive ordinals", "[eventOrdinalTest]") {
  TestController testController;
  auto temp_dir = testController.createTempDirectory();
  REQUIRE(!temp_dir.empty());

  minifi::provenance::RocksDbProvenanceRepository provdb("TestProvRepo", temp_dir.string(), 1s, TEST_MAX_PROVENANCE_STORAGE_SIZE, 1s);

  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, temp_dir.string());

  REQUIRE(provdb.initialize(configuration));

  auto first_batch = createEvents(5);
  REQUIRE(provdb.appendEvents(first_batch));
  for (size_t i = 0; i < first_batch.size(); ++i) {
    REQUIRE(first_batch.at(i)->getEventOrdinal() == i + 1);
  }

  auto second_batch = createEvents(3);
  REQUIRE(provdb.appendEvents(second_batch));
  for (size_t i = 0; i < second_batch.size(); ++i) {
    REQUIRE(second_batch.at(i)->getEventOrdinal() == first_batch.size() + i + 1);
  }
}

TEST_CASE("Test querying events whose ordinals have different number of digits", "[eventOrdinalTest]") {
  TestController testController;
  auto temp_dir = testController.createTempDirectory();
  REQUIRE(!temp_dir.empty());

  minifi::provenance::RocksDbProvenanceRepository provdb("TestProvRepo", temp_dir.string(), 1s, TEST_MAX_PROVENANCE_STORAGE_SIZE, 1s);

  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, temp_dir.string());

  REQUIRE(provdb.initialize(configuration));

  auto events = createEvents(12);
  REQUIRE(provdb.appendEvents(events));

  auto queried_events = provdb.getEvents(12, nullptr).value();
  REQUIRE(queried_events.size() == 12);

  for (size_t i = 0; i < queried_events.size(); ++i) {
    REQUIRE(queried_events.at(i)->getEventOrdinal() == i + 1);
    REQUIRE(serializeEvent(*events.at(i)) == serializeEvent(*queried_events.at(i)));
  }
}

TEST_CASE("Test cursor observes events appended with more digits in their ordinal", "[eventOrdinalTest]") {
  TestController testController;
  auto temp_dir = testController.createTempDirectory();
  REQUIRE(!temp_dir.empty());

  minifi::provenance::RocksDbProvenanceRepository provdb("TestProvRepo", temp_dir.string(), 1s, TEST_MAX_PROVENANCE_STORAGE_SIZE, 1s);

  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, temp_dir.string());

  REQUIRE(provdb.initialize(configuration));

  REQUIRE(provdb.appendEvents(createEvents(9)));

  auto cursor = provdb.cursorFromString("");
  REQUIRE(cursor);
  REQUIRE(provdb.getEvents(9, cursor.get()).value().size() == 9);
  REQUIRE(cursor->toString() == "9");

  REQUIRE(provdb.appendEvents(createEvents(3)));

  auto queried_events = provdb.getEvents(9, cursor.get()).value();
  REQUIRE(queried_events.size() == 3);
  for (size_t i = 0; i < queried_events.size(); ++i) {
    REQUIRE(queried_events.at(i)->getEventOrdinal() == i + 10);
  }
}

TEST_CASE("Test opening a database whose options mention the internal state column but does not have it", "[eventOrdinalTest]") {
  TestController testController;
  auto temp_dir = testController.createTempDirectory();
  REQUIRE(!temp_dir.empty());

  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, temp_dir.string());

  {
    minifi::provenance::RocksDbProvenanceRepository provdb("TestProvRepo", temp_dir.string(), 1s, TEST_MAX_PROVENANCE_STORAGE_SIZE, 1s);
    REQUIRE(provdb.initialize(configuration));
    REQUIRE(provdb.appendEvents(createEvents(1)));
  }

  // Leave the directory in the state an interrupted deletion produces: the persisted options still
  // list the internal state column, while the database itself is gone. The column is created on
  // demand, so this must not stop the repository from opening.
  bool options_file_kept = false;
  for (const auto& entry : std::filesystem::directory_iterator{temp_dir}) {
    const auto filename = entry.path().filename().string();
    if (filename.starts_with("CURRENT") || filename.starts_with("MANIFEST") || filename.ends_with(".log")) {
      std::filesystem::remove(entry.path());
    } else if (filename.starts_with("OPTIONS")) {
      options_file_kept = true;
    }
  }
  // the scenario is only reproduced as long as the persisted options are the ones left behind
  REQUIRE(options_file_kept);

  minifi::provenance::RocksDbProvenanceRepository provdb("TestProvRepo", temp_dir.string(), 1s, TEST_MAX_PROVENANCE_STORAGE_SIZE, 1s);
  REQUIRE(provdb.initialize(configuration));
  REQUIRE(provdb.getRocksDbStats());
  REQUIRE(provdb.appendEvents(createEvents(1)));
}

TEST_CASE("Test opening existing database loads monotonic counter", "[eventUuidMonotonicTest]") {
  TestController testController;
  auto temp_dir = testController.createTempDirectory();
  REQUIRE(!temp_dir.empty());

  auto provdb = std::make_unique<minifi::provenance::RocksDbProvenanceRepository>("TestProvRepo", temp_dir.string(), 1s, TEST_MAX_PROVENANCE_STORAGE_SIZE, 1s);

  auto configuration = std::make_shared<org::apache::nifi::minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, temp_dir.string());

  REQUIRE(provdb->initialize(configuration));

  auto events = createEvents(4);

  REQUIRE(provdb->appendEvents(events));

  provdb = std::make_unique<minifi::provenance::RocksDbProvenanceRepository>("TestProvRepo", temp_dir.string(), 1s, TEST_MAX_PROVENANCE_STORAGE_SIZE, 1s);

  REQUIRE(provdb->initialize(configuration));

  auto new_events = createEvents(4);
  appendAll(events, new_events);

  REQUIRE(provdb->appendEvents(new_events));

  // the counter continues where the previous instance left off
  for (size_t i = 0; i < new_events.size(); ++i) {
    REQUIRE(new_events.at(i)->getEventOrdinal() == i + 5);
  }

  std::vector<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> queried_events = provdb->getEvents(8, nullptr).value();
  REQUIRE(queried_events.size() == 8);

  for (size_t i = 0; i < queried_events.size(); ++i) {
    REQUIRE(queried_events.at(i)->getEventOrdinal() == i + 1);
    REQUIRE(serializeEvent(*events.at(i)) == serializeEvent(*queried_events.at(i)));
  }
}
