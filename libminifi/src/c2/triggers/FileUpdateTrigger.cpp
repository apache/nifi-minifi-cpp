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
    resp.operation_arguments["location"] = file_;
    resp.operation_arguments["persist"] = "true";
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
