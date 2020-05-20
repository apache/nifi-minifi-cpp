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

#include "ProvenanceRepository.h"
#include "../TestBase.h"

#define TEST_PROVENANCE_STORAGE_SIZE (1024*100)  // 100 KB
#define TEST_MAX_PROVENANCE_STORAGE_SIZE (100*1024*1024)  // 100 MB

#define TEST_PROVENANCE_ENTRY_LIFE_TIME (1000)  // 1 sec

void generateData(std::vector<char>& data) {
  std::random_device rd;
  std::mt19937 eng(rd());

  std::uniform_int_distribution<> distr(std::numeric_limits<char>::min(), std::numeric_limits<char>::max());
  auto rand = std::bind(distr, eng);
  std::generate_n(data.begin(), data.size(), rand);
}

void provisionRepo(minifi::provenance::ProvenanceRepository& repo, size_t number_of_records, size_t record_size) {
  for (int i = 0; i < number_of_records; ++i) {
    std::vector<char> v(record_size);
    generateData(v);
    REQUIRE(repo.Put(std::to_string(i), reinterpret_cast<const uint8_t*>(v.data()), v.size()));
  }
}

void verifyMaxKeyCount(const minifi::provenance::ProvenanceRepository& repo, uint64_t keyCount) {
  uint64_t k = keyCount;

  for (int i = 0; i < 50; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    k = std::min(k, repo.getKeyCount());
    if (k < keyCount) {
      break;
    }
  }

  REQUIRE(k < keyCount);
}

TEST_CASE("Test size limit", "[sizeLimitTest]") {
  TestController testController;

  char dirtemplate[] = "/var/tmp/db.XXXXXX";
  auto temp_dir = testController.createTempDirectory(dirtemplate);
  REQUIRE(!temp_dir.empty());

  // 60 sec, 100 KB - going to exceed the size limit
  minifi::provenance::ProvenanceRepository provdb("TestProvRepo", temp_dir,
      MAX_PROVENANCE_ENTRY_LIFE_TIME, TEST_PROVENANCE_STORAGE_SIZE, 1000);

  auto configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, temp_dir);

  REQUIRE(provdb.initialize(configuration));

  uint64_t keyCount = 500;

  provisionRepo(provdb, keyCount, 10240);

  verifyMaxKeyCount(provdb, 200);
}

TEST_CASE("Test time limit", "[timeLimitTest]") {
  TestController testController;

  char dirtemplate[] = "/var/tmp/db.XXXXXX";
  auto temp_dir = testController.createTempDirectory(dirtemplate);
  REQUIRE(!temp_dir.empty());

  // 1 sec, 100 MB - going to exceed TTL
  minifi::provenance::ProvenanceRepository provdb("TestProvRepo", temp_dir,
                                                  TEST_PROVENANCE_ENTRY_LIFE_TIME, TEST_MAX_PROVENANCE_STORAGE_SIZE, 1000);

  auto configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, temp_dir);

  REQUIRE(provdb.initialize(configuration));

  uint64_t keyCount = 500;

  provisionRepo(provdb, keyCount / 2, 102400);

  REQUIRE(provdb.getKeyCount() == 250);

  /**
   * Magic: TTL-based DB cleanup only triggers when writeBuffers are serialized to storage
   * To achieve this 250 entries are put to DB with a total size that ensures at least one buffer is serialized
   * Wait more than a sec to make sure the serialized records expire
   * Put another set of entries to trigger cleanup logic to drop the already serialized records
   * This tests relies on the default settings of Provenance repo: a size of a writeBuffer is 16 MB
   * One provisioning call here writes 25 MB to make sure serialization is triggered
   * When the 2nd 50 MB is written the records of the 1st serialization are dropped -> around 160 of them
   * That's why the final check verifies keyCount to be below 400
   */
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  provisionRepo(provdb, keyCount /2, 102400);

  verifyMaxKeyCount(provdb, 400);
}
