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
#ifndef LIBMINIFI_INCLUDE_C2_RESTSENDER_H_
#define LIBMINIFI_INCLUDE_C2_RESTSENDER_H_

#include "json/json.h"
#include "json/writer.h"
#include <string>
#include <mutex>
#include "CivetServer.h"
#include "../C2Protocol.h"
#include "RESTProtocol.h"
#include "../HeartBeatReporter.h"
#include "controllers/SSLContextService.h"
#include "utils/ByteInputCallBack.h"
#include "utils/HTTPClient.h"

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
class RESTSender : public RESTProtocol, public C2Protocol {
 public:

  explicit RESTSender(std::string name, uuid_t uuid = nullptr);

  virtual C2Payload consumePayload(const std::string &url, const C2Payload &payload, Direction direction, bool async);

  virtual C2Payload consumePayload(const C2Payload &payload, Direction direction, bool async);

  virtual void update(const std::shared_ptr<Configure> &configure);

  virtual void initialize(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<Configure> &configure);

 protected:

  virtual const C2Payload sendPayload(const std::string url, const Direction direction, const C2Payload &payload, const std::string outputConfig);

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;

 private:
  std::shared_ptr<logging::Logger> logger_;
  std::string rest_uri_;
  std::string ack_uri_;
};

REGISTER_RESOURCE(RESTSender);

} /* namesapce c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_RESTPROTOCOL_H_ */
