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
#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "../TestBase.h"
#include <memory>
#include <string>
#include "ProvenanceTestHelper.h"
#include "provenance/Provenance.h"
#include "FlowFileRecord.h"
#include "core/Core.h"
#include "core/repository/FlowFileRepository.h"
#include "properties/Configure.h"

TEST_CASE("Test Repo Empty Value Attribute", "[TestFFR1]") {
  TestController testController;
  char format[] = "/tmp/testRepo.XXXXXX";
  char *dir = testController.createTempDirectory(format);
  std::shared_ptr<core::repository::FlowFileRepository> repository =
      std::make_shared<core::repository::FlowFileRepository>("ff", dir, 0, 0,
                                                             1);

  repository->initialize(std::make_shared<minifi::Configure>());

  minifi::FlowFileRecord record(repository);

  record.addAttribute("keyA", "");

  REQUIRE(true == record.Serialize());

  repository->stop();
}

TEST_CASE("Test Repo Empty Key Attribute ", "[TestFFR2]") {
  TestController testController;
  char format[] = "/tmp/testRepo.XXXXXX";
  char *dir = testController.createTempDirectory(format);
  std::shared_ptr<core::repository::FlowFileRepository> repository =
      std::make_shared<core::repository::FlowFileRepository>("ff", dir, 0, 0,
                                                             1);

  repository->initialize(std::make_shared<minifi::Configure>());

  minifi::FlowFileRecord record(repository);

  record.addAttribute("keyA", "hasdgasdgjsdgasgdsgsadaskgasd");

  record.addAttribute("", "hasdgasdgjsdgasgdsgsadaskgasd");

  REQUIRE(true == record.Serialize());

  repository->stop();
}

TEST_CASE("Test Repo Key Attribute Verify ", "[TestFFR3]") {
  TestController testController;
  char format[] = "/tmp/testRepo.XXXXXX";
  char *dir = testController.createTempDirectory(format);
  std::shared_ptr<core::repository::FlowFileRepository> repository =
      std::make_shared<core::repository::FlowFileRepository>("ff", dir, 0, 0,
                                                             1);

  repository->initialize(
      std::make_shared<org::apache::nifi::minifi::Configure>());

  minifi::FlowFileRecord record(repository);

  minifi::FlowFileRecord record2(repository);

  std::string uuid = record.getUUIDStr();

  record.addAttribute("keyA", "hasdgasdgjsdgasgdsgsadaskgasd");

  record.addAttribute("keyB", "");

  record.addAttribute("", "");

  record.updateAttribute("", "hasdgasdgjsdgasgdsgsadaskgasd2");

  record.addAttribute("", "sdgsdg");

  REQUIRE(true == record.Serialize());

  repository->stop();

  record2.DeSerialize(uuid);

  std::string value;
  REQUIRE(true == record2.getAttribute("", value));

  REQUIRE("hasdgasdgjsdgasgdsgsadaskgasd2" == value);

  REQUIRE(false == record2.getAttribute("key", value));
  REQUIRE(true == record2.getAttribute("keyA", value));
  REQUIRE("hasdgasdgjsdgasgdsgsadaskgasd" == value);

  REQUIRE(true == record2.getAttribute("keyB", value));
  REQUIRE("" == value);
}
