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
#ifndef LIBMINIFI_INCLUDE_CORE_SCHEDULING_H_
#define LIBMINIFI_INCLUDE_CORE_SCHEDULING_H_

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

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

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_CORE_SCHEDULING_H_
