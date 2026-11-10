// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "c2/triggers/FileUpdateTrigger.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::c2 {

/**
 * Returns a payload implementing a C2 action
 */
C2Payload FileUpdateTrigger::getAction() {
  if (update_) {
    C2Payload response_payload(Operation::update, state::UpdateState::READ_COMPLETE, true);
    C2ContentResponse resp(Operation::update);
    resp.ident = "triggered";
    resp.name = "configuration";
    resp.operation_arguments["location"] = C2Value{file_};
    resp.operation_arguments["persist"] = C2Value{"true"};
    response_payload.addContent(std::move(resp));
    update_ = false;
    return response_payload;
  }
  C2Payload response_payload(Operation::heartbeat, state::UpdateState::READ_COMPLETE, true);
  return response_payload;
}

std::optional<std::chrono::file_clock::time_point> FileUpdateTrigger::getLastUpdate() const {
  std::lock_guard<std::mutex> lock(last_update_lock);
  return last_update_;
}

void FileUpdateTrigger::setLastUpdate(const std::optional<std::chrono::file_clock::time_point> &last_update) {
  std::lock_guard<std::mutex> lock(last_update_lock);
  last_update_ = last_update;
}

REGISTER_RESOURCE(FileUpdateTrigger, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::c2
