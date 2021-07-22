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
#ifndef LIBMINIFI_INCLUDE_C2_CONTROLLERSOCKETPROTOCOL_H_
#define LIBMINIFI_INCLUDE_C2_CONTROLLERSOCKETPROTOCOL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "HeartbeatReporter.h"
#include "io/StreamFactory.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

/**
 * Purpose: Creates a reporter that can handle basic c2 operations for a localized environment
 * through a simple TCP socket.
 */
class ControllerSocketProtocol : public HeartbeatReporter {
 public:
  ControllerSocketProtocol(const std::string& name, const utils::Identifier& uuid = {}) // NOLINT
      : HeartbeatReporter(name, uuid),
        logger_(logging::LoggerFactory<ControllerSocketProtocol>::getLogger()) {
  }

  /**
   * Initialize the socket protocol.
   * @param controller controller service provider.
   * @param updateSink update mechanism that will be used to stop/clear elements
   * @param configuration configuration class.
   */
  virtual void initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<state::StateMonitor> &updateSink,
                          const std::shared_ptr<Configure> &configuration);

  /**
   * Handles the heartbeat
   * @param payload incoming payload. From this function we only care about queue metrics.
   */
  virtual int16_t heartbeat(const C2Payload &payload);

 protected:
  /**
   * Parses content from the content response.
   */
  void parse_content(const std::vector<C2ContentResponse> &content);

  std::mutex controller_mutex_;

  std::map<std::string, bool> queue_full_;

  std::map<std::string, uint64_t> queue_size_;

  std::map<std::string, uint64_t> queue_max_;

  std::map<std::string, bool> component_map_;

  std::unique_ptr<io::BaseServerSocket> server_socket_;

  std::shared_ptr<minifi::io::StreamFactory> stream_factory_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace c2
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_C2_CONTROLLERSOCKETPROTOCOL_H_
