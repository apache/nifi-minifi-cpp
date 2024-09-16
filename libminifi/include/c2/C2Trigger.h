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
#ifndef LIBMINIFI_INCLUDE_C2_C2TRIGGER_H_
#define LIBMINIFI_INCLUDE_C2_C2TRIGGER_H_

#include <memory>
#include <utility>
#include <string>

#include "core/Connectable.h"
#include "c2/C2Payload.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::c2 {

/**
 * Purpose: Defines basic triggering mechanism for command and control interfaces
 *
 * Design: Extends Connectable so that we can instantiate with the class name
 *
 * The state machine expects triggered (yes ) -> getAction -> reset(optional)
 */
class C2Trigger : public core::ConnectableImpl {
 public:
  C2Trigger(std::string_view name, const utils::Identifier& uuid)
        : core::ConnectableImpl(name, uuid) {
  }
  ~C2Trigger() override = default;


  /**
   * initializes trigger with minifi configuration.
   */
  virtual void initialize(const std::shared_ptr<minifi::Configure> &configuration) = 0;
  /**
   * returns true if triggered, false otherwise. calling this function multiple times
   * may change internal state.
   */
  virtual bool triggered() = 0;

  /**
   * Resets actions once they have been triggered. The flow of events does not require
   * this to occur after an action has been triggered. Instead this is optional
   * and a feature available to potential triggers that require a reset.
   *
   * This will occur because there are times in which the C2Action may take a significant
   * amount of time and a reset is in order to avoid continual triggering.
   */
  virtual void reset() = 0;

  /**
   * Returns a payload implementing a C2 action. May or may not reset the action.
   * @return C2Payload of the action to perform.
   */
  virtual C2Payload getAction() = 0;
};

}  // namespace org::apache::nifi::minifi::c2

#endif  // LIBMINIFI_INCLUDE_C2_C2TRIGGER_H_
