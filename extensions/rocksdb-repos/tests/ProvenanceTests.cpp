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

#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <utility>
#include <string>
#include <vector>

#include "core/Core.h"
#include "io/BufferStream.h"
#include "core/repository/AtomicRepoEntries.h"
#include "core/repository/VolatileProvenanceRepository.h"
#include "core/RepositoryFactory.h"
#include "FlowFileRecord.h"
#include "unit/ProvenanceTestHelper.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

namespace provenance = minifi::provenance;
using namespace std::literals::chrono_literals;

TEST_CASE("Test Provenance record create", "[Testprovenance::ProvenanceEventRecord]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CREATE,
      utils::Identifier::parse("00000000-0000-0000-0000-000000000001").value(), "blahblah");
  REQUIRE(record1->getAttributes().empty());
  REQUIRE(record1->getAlternateIdentifierUri().empty());
}

TEST_CASE("Test Provenance event ordinal serialization round trip", "[Testprovenance::ProvenanceEventRecordSerializeDeser]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CREATE,
      utils::Identifier::parse("00000000-0000-0000-0000-000000000007").value(), "componenttype");
  record1->setDetails("details");

  uint64_t event_ordinal = 0;
  SECTION("Unset ordinal") {
    event_ordinal = 0;
  }
  SECTION("Small ordinal") {
    event_ordinal = 42;
  }
  SECTION("Largest ordinal") {
    event_ordinal = std::numeric_limits<uint64_t>::max();
  }
  record1->setEventOrdinal(event_ordinal);

  minifi::io::BufferStream stream;
  REQUIRE(record1->serialize(stream));

  auto record2 = provenance::ProvenanceEventRecord::create();
  REQUIRE(record2->deserialize(stream));
  REQUIRE(record2->getEventOrdinal() == event_ordinal);
  // the ordinal is serialized last, verify that the preceding fields are intact as well
  REQUIRE(record2->getEventId() == record1->getEventId());
  REQUIRE(record2->getEventType() == record1->getEventType());
  REQUIRE(record2->getDetails() == record1->getDetails());
}

TEST_CASE("Test Provenance event serialized before the ordinal was introduced", "[Testprovenance::ProvenanceEventRecordSerializeDeser]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CREATE,
      utils::Identifier::parse("00000000-0000-0000-0000-000000000008").value(), "componenttype");
  record1->setDetails("details");
  record1->setEventOrdinal(42);

  minifi::io::BufferStream stream;
  REQUIRE(record1->serialize(stream));
  const auto serialized_event = stream.moveBuffer();
  REQUIRE(serialized_event.size() > sizeof(uint64_t));

  // the ordinal is written last, an event stored before it was introduced simply ends earlier
  std::ptrdiff_t missing_bytes = 0;
  SECTION("The ordinal is missing entirely") {
    missing_bytes = sizeof(uint64_t);
  }
  SECTION("The ordinal is truncated") {
    missing_bytes = sizeof(uint64_t) / 2;
  }
  const std::vector<std::byte> old_format_event{serialized_event.begin(), serialized_event.end() - missing_bytes};

  minifi::io::BufferStream old_format_stream{std::span<const std::byte>{old_format_event}};
  auto record2 = provenance::ProvenanceEventRecord::create();
  REQUIRE(record2->deserialize(old_format_stream));
  REQUIRE(record2->getEventOrdinal() == 0);
  REQUIRE(record2->getEventId() == record1->getEventId());
  REQUIRE(record2->getDetails() == record1->getDetails());
}

TEST_CASE("Test Provenance record serialization", "[Testprovenance::ProvenanceEventRecordSerializeDeser]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CREATE,
        utils::Identifier::parse("00000000-0000-0000-0000-000000000002").value(), "componenttype");

  std::string smileyface = ":)";
  record1->setDetails(smileyface);

  auto sample = 65555ms;
  auto testRepository = std::make_shared<TestRepository>();
  record1->setEventDuration(sample);

  REQUIRE(testRepository->appendEvents({record1}));
  REQUIRE(record1->getEventOrdinal() == 1);

  auto events = testRepository->getEvents(1, nullptr);
  REQUIRE(events);
  REQUIRE(events->size() == 1);
  const auto& record2 = events->at(0);
  REQUIRE(record2->getEventId() == record1->getEventId());
  REQUIRE(record2->getComponentId() == record1->getComponentId());
  REQUIRE(record2->getComponentType() == record1->getComponentType());
  REQUIRE(record2->getDetails() == record1->getDetails());
  REQUIRE(record2->getDetails() == smileyface);
  REQUIRE(record2->getEventDuration() == sample);
  REQUIRE(record2->getEventOrdinal() == record1->getEventOrdinal());
}

TEST_CASE("Test Flowfile record added to provenance", "[TestFlowAndProv1]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CLONE,
      utils::Identifier::parse("00000000-0000-0000-0000-000000000003").value(), "componenttype");
  std::shared_ptr<minifi::FlowFileRecord> ffr1 = std::make_shared<minifi::FlowFileRecordImpl>();
  ffr1->setAttribute("potato", "potatoe");
  ffr1->setAttribute("tomato", "tomatoe");

  record1->addChildFlowFile(*ffr1);

  auto sample = 65555ms;
  auto testRepository = std::make_shared<TestRepository>();
  record1->setEventDuration(sample);

  REQUIRE(testRepository->appendEvents({record1}));

  auto events = testRepository->getEvents(1, nullptr);
  REQUIRE(events);
  REQUIRE(events->size() == 1);
  const auto& record2 = events->at(0);
  REQUIRE(record1->getChildrenUuids().size() == 1);
  REQUIRE(record2->getChildrenUuids().size() == 1);
  utils::Identifier childId = record2->getChildrenUuids().at(0);
  REQUIRE(childId == ffr1->getUUID());
}

TEST_CASE("Test Provenance record serialization Volatile", "[Testprovenance::ProvenanceEventRecordSerializeDeser]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CREATE,
      utils::Identifier::parse("00000000-0000-0000-0000-000000000004").value(), "componenttype");

  std::string smileyface = ":)";
  record1->setDetails(smileyface);

  auto sample = 65555ms;

  auto testRepository = std::make_shared<core::repository::VolatileProvenanceRepository>();
  testRepository->initialize(nullptr);
  record1->setEventDuration(sample);

  REQUIRE(testRepository->appendEvents({record1}));
  REQUIRE(record1->getEventOrdinal() == 1);

  auto events = testRepository->getEvents(1, nullptr);
  REQUIRE(events);
  REQUIRE(events->size() == 1);
  const auto& record2 = events->at(0);
  REQUIRE(record2->getEventId() == record1->getEventId());
  REQUIRE(record2->getComponentId() == record1->getComponentId());
  REQUIRE(record2->getComponentType() == record1->getComponentType());
  REQUIRE(record2->getDetails() == record1->getDetails());
  REQUIRE(record2->getDetails() == smileyface);
  REQUIRE(record2->getEventDuration() == sample);
  REQUIRE(record2->getEventOrdinal() == record1->getEventOrdinal());
}

TEST_CASE("Test Flowfile record added to provenance using Volatile Repo", "[TestFlowAndProv1]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CLONE,
      utils::Identifier::parse("00000000-0000-0000-0000-000000000005").value(), "componenttype");
  std::shared_ptr<minifi::FlowFileRecord> ffr1 = std::make_shared<minifi::FlowFileRecordImpl>();
  ffr1->setAttribute("potato", "potatoe");
  ffr1->setAttribute("tomato", "tomatoe");

  record1->addChildFlowFile(*ffr1);

  auto sample = 65555ms;
  auto testRepository = std::make_shared<core::repository::VolatileProvenanceRepository>();
  testRepository->initialize(nullptr);
  record1->setEventDuration(sample);

  REQUIRE(testRepository->appendEvents({record1}));

  auto events = testRepository->getEvents(1, nullptr);
  REQUIRE(events);
  REQUIRE(events->size() == 1);
  const auto& record2 = events->at(0);
  REQUIRE(record1->getChildrenUuids().size() == 1);
  REQUIRE(record2->getChildrenUuids().size() == 1);
  utils::Identifier childId = record2->getChildrenUuids().at(0);
  REQUIRE(childId == ffr1->getUUID());
}

TEST_CASE("Test Provenance record serialization NoOp", "[Testprovenance::ProvenanceEventRecordSerializeDeser]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CREATE,
      utils::Identifier::parse("00000000-0000-0000-0000-000000000006").value(), "componenttype");

  std::string smileyface = ":)";
  record1->setDetails(smileyface);

  auto sample = 65555ms;

  std::shared_ptr testRepository = utils::dynamic_unique_cast<minifi::provenance::ProvenanceRepository>(core::createRepository("nooprepository"));
  testRepository->initialize(nullptr);
  record1->setEventDuration(sample);

  REQUIRE(testRepository->appendEvents({record1}));
  // the event is not stored, so it is not assigned an ordinal either
  REQUIRE(record1->getEventOrdinal() == 0);

  auto events = testRepository->getEvents(1, nullptr);
  REQUIRE(events);
  REQUIRE(events->empty());
}
