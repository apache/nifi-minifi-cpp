/**
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
#ifndef LIBMINIFI_INCLUDE_CORE_PROCESSORCONFIG_H_
#define LIBMINIFI_INCLUDE_CORE_PROCESSORCONFIG_H_

#include <string>
#include <vector>

#include "Core.h"
#include "Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {


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
  std::string runDurationNanos;
  std::vector<std::string> autoTerminatedRelationships;
  std::vector<core::Property> properties;
  std::string parameterContextName;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_PROCESSORCONFIG_H_
