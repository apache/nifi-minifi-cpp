// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <chrono>

#include "unit/TestBase.h"
#include "unit/Catch.h"

#include "properties/Configure.h"
#include "DiskSpaceWatchdog.h"

namespace minifi = org::apache::nifi::minifi;
namespace dsg = minifi::disk_space_watchdog;
namespace chr = std::chrono;

TEST_CASE("disk_space_watchdog::read_config", "[dsg::read_config]") {
  const auto mebibytes = 1024 * 1024;

  SECTION("defaults are present") {
    const minifi::ConfigureImpl configure;
    const auto conf = dsg::read_config(configure);
    REQUIRE(conf.interval >= chr::nanoseconds{0});
    REQUIRE(conf.restart_threshold_bytes > conf.stop_threshold_bytes);
  }

  SECTION("basic") {
    minifi::ConfigureImpl configure;
    configure.set(minifi::Configure::minifi_disk_space_watchdog_stop_threshold, std::to_string(10 * mebibytes));
    configure.set(minifi::Configure::minifi_disk_space_watchdog_restart_threshold, std::to_string(25 * mebibytes));
    configure.set(minifi::Configure::minifi_disk_space_watchdog_interval, "2000 millis");
    const auto conf = dsg::read_config(configure);
    REQUIRE(conf.stop_threshold_bytes == 10 * mebibytes);
    REQUIRE(conf.restart_threshold_bytes == 25 * mebibytes);
    REQUIRE(conf.interval == chr::seconds{2});
  }

  SECTION("units") {
    minifi::ConfigureImpl configure;
    configure.set(minifi::Configure::minifi_disk_space_watchdog_stop_threshold, "100 MB");
    configure.set(minifi::Configure::minifi_disk_space_watchdog_restart_threshold, "250 MB");
    configure.set(minifi::Configure::minifi_disk_space_watchdog_interval, "7 sec");
    const auto conf = dsg::read_config(configure);
    REQUIRE(conf.stop_threshold_bytes == 100 * mebibytes);
    REQUIRE(conf.restart_threshold_bytes == 250 * mebibytes);
    REQUIRE(conf.interval == chr::seconds{7});
  }
}
