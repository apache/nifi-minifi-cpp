// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/state/UpdateController.h"
#include <utility>
#include <string>

namespace org::apache::nifi::minifi::state {

UpdateStatus::UpdateStatus(UpdateState state, int16_t reason)
    : state_(state),
      reason_(reason) {
}

UpdateState UpdateStatus::getState() const {
  return state_;
}

std::string UpdateStatus::getError() const {
  return error_;
}

int16_t UpdateStatus::getReadonCode() const {
  return reason_;
}

}  // namespace org::apache::nifi::minifi::state
