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

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "SQLiteProvenanceRepository.h"
#include "core/FlowFile.h"
#include "core/RepositoryFactory.h"
#include "io/BufferStream.h"
#include "properties/Configure.h"
#include "utils/StringUtils.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::provenance::test {

using namespace std::literals::chrono_literals;

namespace {

std::vector<std::shared_ptr<ProvenanceEventRecord>> createEvents(size_t count) {
  std::vector<std::shared_ptr<ProvenanceEventRecord>> events;
  events.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    events.push_back(ProvenanceEventRecord::create());
  }
  return events;
}

std::shared_ptr<ProvenanceEventRecord> createEventFor(const core::FlowFile& flow_file) {
  auto event = ProvenanceEventRecord::create();
  event->fromFlowFile(flow_file);
  return event;
}

std::vector<uint64_t> getEventOrdinals(const std::vector<std::shared_ptr<ProvenanceEventRecord>>& events) {
  std::vector<uint64_t> ordinals;
  ordinals.reserve(events.size());
  for (const auto& event : events) {
    ordinals.push_back(event->getEventOrdinal());
  }
  return ordinals;
}

std::vector<utils::Identifier> getEventIds(const std::vector<std::shared_ptr<ProvenanceEventRecord>>& events) {
  std::vector<utils::Identifier> ids;
  ids.reserve(events.size());
  for (const auto& event : events) {
    ids.push_back(event->getEventId());
  }
  return ids;
}

// a cursor that only iterates the events related to @key (event id, flow file id, lineage, parent or child id)
std::string keyedCursorString(const std::string& key, uint64_t last_event_ordinal = 0) {
  io::BufferStream stream;
  stream.write(last_event_ordinal);
  stream.write(key);
  return utils::string::to_hex(stream.getBuffer());
}

class SQLiteProvenanceRepositoryTestFixture {
 public:
  SQLiteProvenanceRepositoryTestFixture()
    : directory_(test_controller_.createTempDirectory()),
      configuration_(std::make_shared<ConfigureImpl>()) {
  }

  std::unique_ptr<SQLiteProvenanceRepository> createRepository(
      std::chrono::milliseconds max_entry_life_time = SQLiteProvenanceRepository::DefaultMaxEntryLifeTime,
      int64_t max_storage_size = SQLiteProvenanceRepository::DefaultMaxStorageSize,
      std::chrono::milliseconds purge_period = SQLiteProvenanceRepository::DefaultPurgePeriod) {
    auto repository = std::make_unique<SQLiteProvenanceRepository>("TestProvRepo", directory_.string(), max_entry_life_time, max_storage_size, purge_period);
    REQUIRE(repository->initialize(configuration_));
    return repository;
  }

 protected:
  TestController test_controller_;
  std::filesystem::path directory_;
  std::shared_ptr<ConfigureImpl> configuration_;
};

}  // namespace

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository can be created by its class name", "[sqliteProvenanceRepository]") {
  for (const auto* class_name : {"SQLiteProvenanceRepository", "sqliteprovenancerepository"}) {
    auto repository = core::createRepository(class_name, "provenance");
    REQUIRE(repository);
    REQUIRE(dynamic_cast<SQLiteProvenanceRepository*>(repository.get()));
  }
}

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository uses the configured directory", "[sqliteProvenanceRepository]") {
  const auto configured_directory = directory_ / "configured" / "provenance";
  configuration_->set(Configure::nifi_provenance_repository_directory_default, configured_directory.string());

  SQLiteProvenanceRepository repository;
  REQUIRE(repository.initialize(configuration_));
  REQUIRE(repository.getDirectory() == configured_directory.string());
  REQUIRE(std::filesystem::exists(configured_directory / "provenance.db"));
}

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository assigns consecutive ordinals", "[sqliteProvenanceRepository]") {
  auto repository = createRepository();

  auto first_batch = createEvents(3);
  REQUIRE(repository->appendEvents(first_batch));
  REQUIRE(getEventOrdinals(first_batch) == std::vector<uint64_t>{1, 2, 3});

  auto second_batch = createEvents(2);
  REQUIRE(repository->appendEvents(second_batch));
  REQUIRE(getEventOrdinals(second_batch) == std::vector<uint64_t>{4, 5});
}

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository counts the stored events", "[sqliteProvenanceRepository]") {
  auto repository = createRepository();
  REQUIRE(repository->getRepositoryEntryCount() == 0);

  REQUIRE(repository->appendEvents({}));
  REQUIRE(repository->getRepositoryEntryCount() == 0);

  REQUIRE(repository->appendEvents(createEvents(5)));
  REQUIRE(repository->getRepositoryEntryCount() == 5);
  REQUIRE(repository->getRepositorySize() > 0);
}

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository returns the events in ordinal order", "[sqliteProvenanceRepository]") {
  auto repository = createRepository();

  auto events = createEvents(12);
  REQUIRE(repository->appendEvents(events));

  auto cursor = repository->cursorFromString("");
  REQUIRE(cursor);

  auto queried_events = repository->getEvents(12, cursor.get());
  REQUIRE(queried_events);
  REQUIRE(getEventOrdinals(*queried_events) == std::vector<uint64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
  REQUIRE(getEventIds(*queried_events) == getEventIds(events));
}

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository continues the iteration from the cursor", "[sqliteProvenanceRepository]") {
  auto repository = createRepository();

  REQUIRE(repository->appendEvents(createEvents(8)));

  auto cursor = repository->cursorFromString("");
  REQUIRE(cursor);

  auto first_page = repository->getEvents(3, cursor.get());
  REQUIRE(first_page);
  REQUIRE(getEventOrdinals(*first_page) == std::vector<uint64_t>{1, 2, 3});

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

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository does not consume the events it returns", "[sqliteProvenanceRepository]") {
  auto repository = createRepository();

  REQUIRE(repository->appendEvents(createEvents(3)));

  auto cursor = repository->cursorFromString("");
  REQUIRE(cursor);
  REQUIRE(getEventOrdinals(repository->getEvents(3, cursor.get()).value()) == std::vector<uint64_t>{1, 2, 3});

  auto new_cursor = repository->cursorFromString("");
  REQUIRE(new_cursor);
  REQUIRE(getEventOrdinals(repository->getEvents(3, new_cursor.get()).value()) == std::vector<uint64_t>{1, 2, 3});
}

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository rejects a cursor of another repository", "[sqliteProvenanceRepository]") {
  class ForeignCursor : public ProvenanceRepository::Cursor {
   public:
    [[nodiscard]] std::string toString() const override { return ""; }
  };

  auto repository = createRepository();
  REQUIRE(repository->appendEvents(createEvents(1)));

  ForeignCursor cursor;
  REQUIRE_FALSE(repository->getEvents(10, &cursor));
}

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository keeps the events and the ordinals across restarts", "[sqliteProvenanceRepository]") {
  std::string cursor_str;
  {
    auto repository = createRepository();
    REQUIRE(repository->appendEvents(createEvents(3)));

    auto cursor = repository->cursorFromString("");
    REQUIRE(getEventOrdinals(repository->getEvents(2, cursor.get()).value()) == std::vector<uint64_t>{1, 2});
    cursor_str = cursor->toString();
  }

  auto repository = createRepository();
  REQUIRE(repository->getRepositoryEntryCount() == 3);
  REQUIRE(repository->appendEvents(createEvents(2)));

  // a persisted cursor continues where it stopped
  auto cursor = repository->cursorFromString(cursor_str);
  REQUIRE(getEventOrdinals(repository->getEvents(10, cursor.get()).value()) == std::vector<uint64_t>{3, 4, 5});
}

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository can iterate the events of a single flow file", "[sqliteProvenanceRepository]") {
  auto repository = createRepository();

  core::FlowFileImpl first_flow_file;
  core::FlowFileImpl second_flow_file;
  const std::vector<std::shared_ptr<ProvenanceEventRecord>> events{
    createEventFor(first_flow_file),
    createEventFor(second_flow_file),
    createEventFor(first_flow_file),
    createEventFor(second_flow_file),
    createEventFor(first_flow_file)
  };
  REQUIRE(repository->appendEvents(events));

  auto cursor = repository->cursorFromString(keyedCursorString(first_flow_file.getUUIDStr()));
  REQUIRE(cursor);

  auto first_page = repository->getEvents(2, cursor.get());
  REQUIRE(first_page);
  REQUIRE(getEventIds(*first_page) == std::vector{events[0]->getEventId(), events[2]->getEventId()});

  // the key is kept when the cursor is persisted
  cursor = repository->cursorFromString(cursor->toString());
  REQUIRE(cursor);

  auto second_page = repository->getEvents(10, cursor.get());
  REQUIRE(second_page);
  REQUIRE(getEventIds(*second_page) == std::vector{events[4]->getEventId()});

  auto third_page = repository->getEvents(10, cursor.get());
  REQUIRE(third_page);
  REQUIRE(third_page->empty());
}

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository can find an event by its id", "[sqliteProvenanceRepository]") {
  auto repository = createRepository();

  auto events = createEvents(3);
  REQUIRE(repository->appendEvents(events));

  auto cursor = repository->cursorFromString(keyedCursorString(events[1]->getUUIDStr()));
  auto queried_events = repository->getEvents(10, cursor.get());
  REQUIRE(queried_events);
  REQUIRE(getEventIds(*queried_events) == std::vector{events[1]->getEventId()});
  REQUIRE(getEventOrdinals(*queried_events) == std::vector<uint64_t>{2});
}

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository can iterate the events related to a parent flow file", "[sqliteProvenanceRepository]") {
  auto repository = createRepository();

  core::FlowFileImpl parent;
  core::FlowFileImpl child;
  child.getlineageIdentifiers().push_back(parent.getUUID());
  core::FlowFileImpl unrelated;

  auto parent_event = createEventFor(parent);
  parent_event->addChildUuid(child.getUUID());
  // the parent is both in the lineage and among the parents of the child's event
  auto child_event = createEventFor(child);
  child_event->addParentUuid(parent.getUUID());
  const std::vector events{parent_event, createEventFor(unrelated), child_event};
  REQUIRE(repository->appendEvents(events));

  auto cursor = repository->cursorFromString(keyedCursorString(parent.getUUIDStr()));
  auto queried_events = repository->getEvents(10, cursor.get());
  REQUIRE(queried_events);
  // each related event is returned exactly once
  REQUIRE(getEventIds(*queried_events) == std::vector{parent_event->getEventId(), child_event->getEventId()});
}

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository purges the events older than the configured max storage time", "[sqliteProvenanceRepository]") {
  configuration_->set(Configure::nifi_provenance_repository_max_storage_time, "1 sec");
  auto repository = createRepository(SQLiteProvenanceRepository::DefaultMaxEntryLifeTime, SQLiteProvenanceRepository::DefaultMaxStorageSize, 100ms);
  REQUIRE(repository->start());

  REQUIRE(repository->appendEvents(createEvents(10)));
  REQUIRE(repository->getRepositoryEntryCount() == 10);

  REQUIRE(minifi::test::utils::verifyEventHappenedInPollTime(10s, [&] { return repository->getRepositoryEntryCount() == 0; }));
  REQUIRE(repository->stop());
}

TEST_CASE_METHOD(SQLiteProvenanceRepositoryTestFixture, "SQLiteProvenanceRepository purges the oldest events above the max storage size", "[sqliteProvenanceRepository]") {
  constexpr int64_t max_storage_size = 100_KiB;
  auto repository = createRepository(1h, max_storage_size, 100ms);

  const std::string details(10_KiB, 'a');
  auto events = createEvents(100);
  for (const auto& event : events) {
    event->setDetails(details);
  }
  REQUIRE(repository->appendEvents(events));
  REQUIRE(repository->getRepositorySize() > static_cast<uint64_t>(max_storage_size));

  REQUIRE(repository->start());
  REQUIRE(minifi::test::utils::verifyEventHappenedInPollTime(10s, [&] { return repository->getRepositoryEntryCount() < events.size(); }));
  REQUIRE(repository->stop());

  REQUIRE(repository->getRepositorySize() <= static_cast<uint64_t>(max_storage_size));
  // newly appended events are not purged right away
  auto new_events = createEvents(1);
  REQUIRE(repository->appendEvents(new_events));
  auto cursor = repository->cursorFromString(keyedCursorString(new_events[0]->getUUIDStr()));
  REQUIRE(repository->getEvents(10, cursor.get())->size() == 1);
}

}  // namespace org::apache::nifi::minifi::provenance::test
