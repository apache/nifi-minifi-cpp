// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

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
