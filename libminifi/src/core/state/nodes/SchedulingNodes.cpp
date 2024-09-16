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
#include "core/state/nodes/SchedulingNodes.h"
#include "core/state/Value.h"

namespace org::apache::nifi::minifi::state::response {

std::vector<SerializedResponseNode> SchedulingDefaults::serialize() {
  return {
    {
      .name = "schedulingDefaults",
      .children = {
        {.name = "defaultSchedulingStrategy", .value = core::DEFAULT_SCHEDULING_STRATEGY},
        {.name = "defaultSchedulingPeriodMillis", .value = int64_t{core::DEFAULT_SCHEDULING_PERIOD_MILLIS.count()}},
        {.name = "defaultRunDurationNanos", .value = int64_t{core::DEFAULT_RUN_DURATION.count()}},
        {.name = "defaultMaxConcurrentTasks", .value = core::DEFAULT_MAX_CONCURRENT_TASKS},
        {.name = "yieldDurationMillis", .value = int64_t{std::chrono::milliseconds(core::DEFAULT_YIELD_PERIOD_SECONDS).count()}},
        {.name = "penalizationPeriodMillis", .value = int64_t{std::chrono::milliseconds{core::DEFAULT_PENALIZATION_PERIOD}.count()}},
      }
    }
  };
}

}  // namespace org::apache::nifi::minifi::state::response

