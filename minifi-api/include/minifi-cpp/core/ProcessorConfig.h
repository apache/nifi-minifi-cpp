// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "Property.h"

namespace org::apache::nifi::minifi::core {

constexpr const char* DEFAULT_SCHEDULING_STRATEGY{"TIMER_DRIVEN"};
constexpr const char* DEFAULT_SCHEDULING_PERIOD_STR{"1 sec"};
constexpr std::chrono::milliseconds DEFAULT_SCHEDULING_PERIOD_MILLIS{1000};
constexpr std::chrono::nanoseconds DEFAULT_RUN_DURATION{0};
constexpr int DEFAULT_MAX_CONCURRENT_TASKS{1};
constexpr std::chrono::seconds DEFAULT_YIELD_PERIOD_SECONDS{1};
constexpr std::chrono::seconds DEFAULT_PENALIZATION_PERIOD{30};

struct ProcessorConfig {
  std::string id;
  std::string name;
  std::string javaClass;
  std::string maxConcurrentTasks;
  std::string schedulingStrategy;
  std::string schedulingPeriod;
  std::string penalizationPeriod;
  std::string yieldPeriod;
  std::string bulletinLevel;
  std::string runDurationNanos;
  std::vector<std::string> autoTerminatedRelationships;
  std::vector<core::Property> properties;
  std::string parameterContextName;
};

}  // namespace org::apache::nifi::minifi::core
