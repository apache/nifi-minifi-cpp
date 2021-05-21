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
#ifndef LIBMINIFI_INCLUDE_C2_PROTOCOLS_RESTPROTOCOL_H_
#define LIBMINIFI_INCLUDE_C2_PROTOCOLS_RESTPROTOCOL_H_

#include <map> // NOLINT
#include <stdexcept> // NOLINT

#include <vector> // NOLINT
#include <string> // NOLINT
#include <mutex> // NOLINT
#include <memory>

#include "utils/ByteArrayCallback.h"
#include "c2/C2Protocol.h"
#include "c2/HeartbeatReporter.h"
#include "controllers/SSLContextService.h"
#include "utils/HTTPClient.h"
#include "Exception.h"
#include "c2/HeartbeatJsonSerializer.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

/**
 * Purpose and Justification: Encapsulates the restful protocol that is built upon C2Protocol.
 *
 * The external interfaces rely solely on send, where send includes a Direction. Transmit will perform a POST
 * and RECEIVE will perform a GET. This does not mean we can't receive on a POST; however, since Direction
 * will encompass other protocols the context of its meaning here simply translates into POST and GET respectively.
 *
 */

class RESTProtocol : public HeartbeatJsonSerializer {
 public:
  RESTProtocol();

 protected:
  void initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<Configure> &configure);

  void serializeNestedPayload(rapidjson::Value& target, const C2Payload& payload, rapidjson::Document::AllocatorType& alloc) override;

  virtual const C2Payload parseJsonResponse(const C2Payload &payload, const std::vector<char> &response);

 private:
  bool containsPayload(const C2Payload &o);

  bool minimize_updates_{false};
  std::map<std::string, C2Payload> nested_payloads_;

  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace c2
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_C2_PROTOCOLS_RESTPROTOCOL_H_
