// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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

