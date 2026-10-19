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
#include <memory>
#include <string>
#include <vector>

#include "core/repository/VolatileProvenanceRepository.h"
#include "properties/Configure.h"
#include "utils/StringUtils.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::core::repository::test {

std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> createEvents(size_t count) {
  std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> events;
  events.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    events.push_back(provenance::ProvenanceEventRecord::create());
  }
  return events;
}

std::vector<uint64_t> getEventOrdinals(const std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>>& events) {
  std::vector<uint64_t> ordinals;
  ordinals.reserve(events.size());
  for (const auto& event : events) {
    ordinals.push_back(event->getEventOrdinal());
  }
  return ordinals;
}

std::shared_ptr<VolatileProvenanceRepository> createRepository(uint32_t max_count = 0) {
  auto repository = std::make_shared<VolatileProvenanceRepository>();
  std::shared_ptr<Configure> configuration;
  if (max_count > 0) {
    configuration = std::make_shared<ConfigureImpl>();
    configuration->set(utils::string::join_pack(Configure::nifi_volatile_repository_options, repository->getName(), ".", VOLATILE_REPO_MAX_COUNT), std::to_string(max_count));
  }
  REQUIRE(repository->initialize(configuration));
  return repository;
}

TEST_CASE("VolatileProvenanceRepository assigns consecutive ordinals", "[volatileProvenanceRepository]") {
  auto repository = createRepository();

  auto first_batch = createEvents(3);
  REQUIRE(repository->appendEvents(first_batch));
  REQUIRE(getEventOrdinals(first_batch) == std::vector<uint64_t>{1, 2, 3});

  auto second_batch = createEvents(2);
  REQUIRE(repository->appendEvents(second_batch));
  REQUIRE(getEventOrdinals(second_batch) == std::vector<uint64_t>{4, 5});
}

TEST_CASE("VolatileProvenanceRepository restarts the ordinals in a new instance", "[volatileProvenanceRepository]") {
  auto events = createEvents(3);
  {
    auto repository = createRepository();
    REQUIRE(repository->appendEvents(events));
    REQUIRE(events.back()->getEventOrdinal() == 3);
  }

  // unlike the persistent repository, this one does not remember the ordinals it handed out
  auto new_events = createEvents(1);
  auto new_repository = createRepository();
  REQUIRE(new_repository->appendEvents(new_events));
  REQUIRE(new_events.front()->getEventOrdinal() == 1);
}

TEST_CASE("VolatileProvenanceRepository returns the events in ordinal order", "[volatileProvenanceRepository]") {
  auto repository = createRepository();

  // more than nine events, so that the ordinals do not all have the same number of digits
  auto events = createEvents(12);
  REQUIRE(repository->appendEvents(events));

  auto cursor = repository->cursorFromString("");
  REQUIRE(cursor);

  auto queried_events = repository->getEvents(12, cursor.get());
  REQUIRE(queried_events);
  REQUIRE(queried_events->size() == 12);
  REQUIRE(getEventOrdinals(*queried_events) == std::vector<uint64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  for (size_t i = 0; i < queried_events->size(); ++i) {
    REQUIRE(queried_events->at(i)->getEventId() == events.at(i)->getEventId());
  }
}

TEST_CASE("VolatileProvenanceRepository continues the iteration from the cursor", "[volatileProvenanceRepository]") {
  auto repository = createRepository();

  REQUIRE(repository->appendEvents(createEvents(8)));

  auto cursor = repository->cursorFromString("");
  REQUIRE(cursor);

  auto first_page = repository->getEvents(3, cursor.get());
  REQUIRE(first_page);
  REQUIRE(getEventOrdinals(*first_page) == std::vector<uint64_t>{1, 2, 3});
  REQUIRE(cursor->toString() == "3");

  // the cursor can be persisted and restored
  cursor = repository->cursorFromString(cursor->toString());
  REQUIRE(cursor);

  auto second_page = repository->getEvents(3, cursor.get());
  REQUIRE(second_page);
  REQUIRE(getEventOrdinals(*second_page) == std::vector<uint64_t>{4, 5, 6});

  // events appended after the cursor was created are picked up as well
  REQUIRE(repository->appendEvents(createEvents(2)));

  auto third_page = repository->getEvents(10, cursor.get());
  REQUIRE(third_page);
  REQUIRE(getEventOrdinals(*third_page) == std::vector<uint64_t>{7, 8, 9, 10});

  auto fourth_page = repository->getEvents(10, cursor.get());
  REQUIRE(fourth_page);
  REQUIRE(fourth_page->empty());
}

TEST_CASE("VolatileProvenanceRepository does not consume the events it returns", "[volatileProvenanceRepository]") {
  auto repository = createRepository();

  REQUIRE(repository->appendEvents(createEvents(3)));

  auto cursor = repository->cursorFromString("");
  REQUIRE(cursor);
  REQUIRE(getEventOrdinals(repository->getEvents(3, cursor.get()).value()) == std::vector<uint64_t>{1, 2, 3});

  auto new_cursor = repository->cursorFromString("");
  REQUIRE(new_cursor);
  REQUIRE(getEventOrdinals(repository->getEvents(3, new_cursor.get()).value()) == std::vector<uint64_t>{1, 2, 3});
}

TEST_CASE("VolatileProvenanceRepository returns the events that survived an overflow", "[volatileProvenanceRepository]") {
  auto repository = createRepository(4);

  REQUIRE(repository->appendEvents(createEvents(6)));

  auto queried_events = repository->getEvents(10, nullptr);
  REQUIRE(queried_events);
  // the oldest events have been overwritten, the remaining ones are still ordered
  REQUIRE(queried_events->size() <= 4);
  REQUIRE(!queried_events->empty());
  auto ordinals = getEventOrdinals(*queried_events);
  REQUIRE(std::ranges::is_sorted(ordinals));
  REQUIRE(ordinals.back() == 6);
}

}  // namespace org::apache::nifi::minifi::core::repository::test
