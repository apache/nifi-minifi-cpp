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
#include <stdio.h>
#include <algorithm>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <set>

#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "ConvertHeartBeat.h"
#include "c2/PayloadSerializer.h"
#include "utils/ByteArrayCallback.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

void ConvertHeartBeat::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  auto ff = session.get();
  if (ff != nullptr) {
    logger_->log_error("ConvertHeartBeat does not receive flow files");
    session->rollback();
  }
  if (nullptr == mqtt_service_) {
    context->yield();
    return;
  }
  std::vector<uint8_t> heartbeat;
  bool received_heartbeat = false;
  // while we have heartbeats we can continue to loop.
  while (mqtt_service_->get(100, listening_topic, heartbeat)) {
    if (heartbeat.size() > 0) {
      c2::C2Payload payload = c2::PayloadSerializer::deserialize(heartbeat);
      auto serialized = serializeJsonRootPayload(payload);
      logger_->log_debug("Converted JSON output %s", serialized);
      minifi::utils::StreamOutputCallback byteCallback(serialized.size() + 1);
      byteCallback.write(const_cast<char*>(serialized.c_str()), serialized.size());
      auto newff = session->create();
      session->write(newff, &byteCallback);
      session->transfer(newff, Success);
      received_heartbeat = true;
    } else {
      break;
    }
  }
  if (!received_heartbeat) {
    context->yield();
  }
}

REGISTER_INTERNAL_RESOURCE(ConvertHeartBeat);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
