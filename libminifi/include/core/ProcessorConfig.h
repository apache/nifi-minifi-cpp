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

#include "core/Core.h"
#include "core/Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {


#define DEFAULT_SCHEDULING_STRATEGY "TIMER_DRIVEN"
#define DEFAULT_SCHEDULING_PERIOD_STR "1 sec"
#define DEFAULT_SCHEDULING_PERIOD_MILLIS 1000
#define DEFAULT_RUN_DURATION 0
#define DEFAULT_MAX_CONCURRENT_TASKS 1
#define DEFAULT_PENALIZATION_PERIOD 1
// Default yield period in second
#define DEFAULT_YIELD_PERIOD_SECONDS 1
#define DEFAULT_PENALIZATION_PERIOD_SECONDS 30

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
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_PROCESSORCONFIG_H_ */
