// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <chrono>
#include <thread>
#include <memory>

#include "c2/triggers/FileUpdateTrigger.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/Processor.h"
#include "core/ClassLoader.h"

TEST_CASE("Empty file", "[t1]") {
  minifi::c2::FileUpdateTrigger trigger("test");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  trigger.initialize(configuration);

  REQUIRE(false == trigger.triggered());
  REQUIRE(minifi::c2::Operation::heartbeat == trigger.getAction().getOperation());
}

TEST_CASE("invalidfile file", "[t2]") {
  minifi::c2::FileUpdateTrigger trigger("test");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_c2_file_watch, "/tmp/blahblahblhalbha");
  trigger.initialize(configuration);

  REQUIRE(false == trigger.triggered());
  REQUIRE(minifi::c2::Operation::heartbeat == trigger.getAction().getOperation());
}

TEST_CASE("test valid  file no update", "[t3]") {
  TestController testController;

  auto path = testController.createTempDirectory() / "tstFile.ext";
  std::fstream file;
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  minifi::c2::FileUpdateTrigger trigger("test");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_c2_file_watch, path.string());
  trigger.initialize(configuration);

  REQUIRE(false == trigger.triggered());
  REQUIRE(minifi::c2::Operation::heartbeat == trigger.getAction().getOperation());
}

TEST_CASE("test valid file update", "[t4]") {
  TestController testController;

  auto path = testController.createTempDirectory() / "tstFile.ext";
  std::fstream file;
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  minifi::c2::FileUpdateTrigger trigger("test");
  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->set(minifi::Configure::nifi_c2_file_watch, path.string());
  trigger.initialize(configuration);

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  file.open(path, std::ios::out);
  file << "tempFiles";
  file.close();

  REQUIRE(true == trigger.triggered());

  REQUIRE(minifi::c2::Operation::update == trigger.getAction().getOperation());
}
