// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

namespace org::apache::nifi::minifi::core {

/*
 * Indicates the valid values for the state of a entity
 * with respect to scheduling the entity to run.
 */
enum ScheduledState {
  /**
   * Entity cannot be scheduled to run
   */
  DISABLED,
  /**
   * Entity can be scheduled to run but currently is not
   */
  STOPPED,
  /**
   * Entity is currently scheduled to run
   */
  RUNNING
};

enum SchedulingStrategy {
  EVENT_DRIVEN,
  TIMER_DRIVEN,
  CRON_DRIVEN
};

}  // namespace org::apache::nifi::minifi::core
