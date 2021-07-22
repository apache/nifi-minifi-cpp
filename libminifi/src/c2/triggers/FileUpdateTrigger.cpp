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

#include "c2/triggers/FileUpdateTrigger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

/**
 * Returns a payload implementing a C2 action
 */
C2Payload FileUpdateTrigger::getAction() {
  if (update_) {
    C2Payload response_payload(Operation::UPDATE, state::UpdateState::READ_COMPLETE, true);
    C2ContentResponse resp(Operation::UPDATE);
    resp.ident = "triggered";
    resp.name = "configuration";
    resp.operation_arguments["location"] = file_;
    resp.operation_arguments["persist"] = "true";
    response_payload.addContent(std::move(resp));
    update_ = false;
    return response_payload;
  }
  C2Payload response_payload(Operation::HEARTBEAT, state::UpdateState::READ_COMPLETE, true);
  return response_payload;
}

REGISTER_RESOURCE(FileUpdateTrigger, "Defines a file update trigger when the last write time of a file has been changed.");

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
