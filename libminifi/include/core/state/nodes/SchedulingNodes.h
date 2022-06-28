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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_NODES_SCHEDULINGNODES_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_NODES_SCHEDULINGNODES_H_

#include <string>
#include <vector>


#include "MetricsBase.h"
#include "core/ProcessorConfig.h"

namespace org::apache::nifi::minifi::state::response {

class SchedulingDefaults : public DeviceInformation {
 public:
  SchedulingDefaults(const std::string &name, const utils::Identifier &uuid)
      : DeviceInformation(name, uuid) {
  }

  SchedulingDefaults(const std::string &name) // NOLINT
      : DeviceInformation(name) {
  }

  std::string getName() const override {
    return "schedulingDefaults";
  }

  std::vector<SerializedResponseNode> serialize() override {
    std::vector<SerializedResponseNode> serialized;

    SerializedResponseNode schedulingDefaults;
    schedulingDefaults.name = "schedulingDefaults";

    SerializedResponseNode defaultSchedulingStrategy;
    defaultSchedulingStrategy.name = "defaultSchedulingStrategy";
    defaultSchedulingStrategy.value = DEFAULT_SCHEDULING_STRATEGY;

    schedulingDefaults.children.push_back(defaultSchedulingStrategy);

    SerializedResponseNode defaultSchedulingPeriod;
    defaultSchedulingPeriod.name = "defaultSchedulingPeriodMillis";
    defaultSchedulingPeriod.value = int64_t{core::DEFAULT_SCHEDULING_PERIOD_MILLIS.count()};

    schedulingDefaults.children.push_back(defaultSchedulingPeriod);

    SerializedResponseNode defaultRunDuration;
    defaultRunDuration.name = "defaultRunDurationNanos";
    defaultRunDuration.value = int64_t{core::DEFAULT_RUN_DURATION.count()};

    schedulingDefaults.children.push_back(defaultRunDuration);

    SerializedResponseNode defaultMaxConcurrentTasks;
    defaultMaxConcurrentTasks.name = "defaultMaxConcurrentTasks";
    defaultMaxConcurrentTasks.value = DEFAULT_MAX_CONCURRENT_TASKS;

    schedulingDefaults.children.push_back(defaultMaxConcurrentTasks);

    SerializedResponseNode yieldDuration;
    yieldDuration.name = "yieldDurationMillis";
    yieldDuration.value = int64_t{std::chrono::milliseconds(core::DEFAULT_YIELD_PERIOD_SECONDS).count()};

    schedulingDefaults.children.push_back(yieldDuration);

    SerializedResponseNode penalizationPeriod;
    penalizationPeriod.name = "penalizationPeriodMillis";
    penalizationPeriod.value = int64_t{std::chrono::milliseconds{core::DEFAULT_PENALIZATION_PERIOD}.count()};

    schedulingDefaults.children.push_back(penalizationPeriod);

    serialized.push_back(schedulingDefaults);

    return serialized;
  }
};

}  // namespace org::apache::nifi::minifi::state::response

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_NODES_SCHEDULINGNODES_H_
