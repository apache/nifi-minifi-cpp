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

#include <map>
#include <memory>
#include <utility>
#include <string>

#include "core/Core.h"
#include "core/repository/AtomicRepoEntries.h"
#include "core/repository/VolatileProvenanceRepository.h"
#include "core/RepositoryFactory.h"
#include "FlowFileRecord.h"
#include "provenance/Provenance.h"
#include "unit/ProvenanceTestHelper.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

namespace provenance = minifi::provenance;
using namespace std::literals::chrono_literals;

TEST_CASE("Test Provenance record create", "[Testprovenance::ProvenanceEventRecord]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CREATE, "blah", "blahblah");
  REQUIRE(record1->getAttributes().empty());
  REQUIRE(record1->getAlternateIdentifierUri().length() == 0);
}

TEST_CASE("Test Provenance record serialization", "[Testprovenance::ProvenanceEventRecordSerializeDeser]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CREATE, "componentid", "componenttype");

  utils::Identifier eventId = record1->getEventId();

  std::string smileyface = ":)";
  record1->setDetails(smileyface);

  auto sample = 65555ms;
  std::shared_ptr<core::Repository> testRepository = std::make_shared<TestRepository>();
  record1->setEventDuration(sample);

  testRepository->storeElement(record1);
  auto record2 = std::make_shared<provenance::ProvenanceEventRecordImpl>();
  record2->setEventId(eventId);
  REQUIRE(record2->loadFromRepository(testRepository) == true);
  REQUIRE(record2->getEventId() == record1->getEventId());
  REQUIRE(record2->getComponentId() == record1->getComponentId());
  REQUIRE(record2->getComponentType() == record1->getComponentType());
  REQUIRE(record2->getDetails() == record1->getDetails());
  REQUIRE(record2->getDetails() == smileyface);
  REQUIRE(record2->getEventDuration() == sample);
}

TEST_CASE("Test Flowfile record added to provenance", "[TestFlowAndProv1]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CLONE, "componentid", "componenttype");
  utils::Identifier eventId = record1->getEventId();
  std::shared_ptr<minifi::FlowFileRecord> ffr1 = std::make_shared<minifi::FlowFileRecordImpl>();
  ffr1->setAttribute("potato", "potatoe");
  ffr1->setAttribute("tomato", "tomatoe");

  record1->addChildFlowFile(*ffr1);

  auto sample = 65555ms;
  std::shared_ptr<core::Repository> testRepository = std::make_shared<TestRepository>();
  record1->setEventDuration(sample);

  testRepository->storeElement(record1);
  auto record2 = std::make_shared<provenance::ProvenanceEventRecordImpl>();
  record2->setEventId(eventId);
  REQUIRE(record2->loadFromRepository(testRepository) == true);
  REQUIRE(record1->getChildrenUuids().size() == 1);
  REQUIRE(record2->getChildrenUuids().size() == 1);
  utils::Identifier childId = record2->getChildrenUuids().at(0);
  REQUIRE(childId == ffr1->getUUID());
  record2->removeChildUuid(childId);
  REQUIRE(record2->getChildrenUuids().empty());
}

TEST_CASE("Test Provenance record serialization Volatile", "[Testprovenance::ProvenanceEventRecordSerializeDeser]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CREATE, "componentid", "componenttype");

  utils::Identifier eventId = record1->getEventId();

  std::string smileyface = ":)";
  record1->setDetails(smileyface);

  auto sample = 65555ms;

  std::shared_ptr<core::Repository> testRepository = std::make_shared<core::repository::VolatileProvenanceRepository>();
  testRepository->initialize(nullptr);
  record1->setEventDuration(sample);

  testRepository->storeElement(record1);
  auto record2 = std::make_shared<provenance::ProvenanceEventRecordImpl>();
  record2->setEventId(eventId);
  REQUIRE(record2->loadFromRepository(testRepository) == true);
  REQUIRE(record2->getEventId() == record1->getEventId());
  REQUIRE(record2->getComponentId() == record1->getComponentId());
  REQUIRE(record2->getComponentType() == record1->getComponentType());
  REQUIRE(record2->getDetails() == record1->getDetails());
  REQUIRE(record2->getDetails() == smileyface);
  REQUIRE(record2->getEventDuration() == sample);
}

TEST_CASE("Test Flowfile record added to provenance using Volatile Repo", "[TestFlowAndProv1]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CLONE, "componentid", "componenttype");
  utils::Identifier eventId = record1->getEventId();
  std::shared_ptr<minifi::FlowFileRecord> ffr1 = std::make_shared<minifi::FlowFileRecordImpl>();
  ffr1->setAttribute("potato", "potatoe");
  ffr1->setAttribute("tomato", "tomatoe");

  record1->addChildFlowFile(*ffr1);

  auto sample = 65555ms;
  std::shared_ptr<core::Repository> testRepository = std::make_shared<core::repository::VolatileProvenanceRepository>();
  testRepository->initialize(nullptr);
  record1->setEventDuration(sample);

  testRepository->storeElement(record1);
  auto record2 = std::make_shared<provenance::ProvenanceEventRecordImpl>();
  record2->setEventId(eventId);
  REQUIRE(record2->loadFromRepository(testRepository) == true);
  REQUIRE(record1->getChildrenUuids().size() == 1);
  REQUIRE(record2->getChildrenUuids().size() == 1);
  utils::Identifier childId = record2->getChildrenUuids().at(0);
  REQUIRE(childId == ffr1->getUUID());
  record2->removeChildUuid(childId);
  REQUIRE(record2->getChildrenUuids().empty());
}

TEST_CASE("Test Provenance record serialization NoOp", "[Testprovenance::ProvenanceEventRecordSerializeDeser]") {
  auto record1 = std::make_shared<provenance::ProvenanceEventRecordImpl>(provenance::ProvenanceEventRecord::ProvenanceEventType::CREATE, "componentid", "componenttype");

  utils::Identifier eventId = record1->getEventId();

  std::string smileyface = ":)";
  record1->setDetails(smileyface);

  auto sample = 65555ms;

  std::shared_ptr<core::Repository> testRepository = core::createRepository("nooprepository");
  testRepository->initialize(nullptr);
  record1->setEventDuration(sample);

  REQUIRE(testRepository->storeElement(record1));
  auto record2 = std::make_shared<provenance::ProvenanceEventRecordImpl>();
  record2->setEventId(eventId);
  REQUIRE(record2->loadFromRepository(testRepository) == false);
}
