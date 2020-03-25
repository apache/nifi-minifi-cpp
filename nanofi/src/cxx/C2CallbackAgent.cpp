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

#include "cxx/C2CallbackAgent.h"
#include <csignal>
#include <utility>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include "c2/ControllerSocketProtocol.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/file/FileUtils.h"
#include "utils/file/FileManager.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

C2CallbackAgent::C2CallbackAgent(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<state::StateMonitor> &updateSink,
                                 const std::shared_ptr<Configure> &configuration)
    : C2Agent(controller, updateSink, configuration),
      stop(nullptr),
      logger_(logging::LoggerFactory<C2CallbackAgent>::getLogger()) {
}

void C2CallbackAgent::handle_c2_server_response(const C2ContentResponse &resp) {
  switch (resp.op) {
    case Operation::CLEAR:
      break;
    case Operation::UPDATE:
      break;
    case Operation::DESCRIBE:
      break;
    case Operation::RESTART:
      break;
    case Operation::START:
    case Operation::STOP: {
      if (resp.name == "C2" || resp.name == "c2") {
        raise(SIGTERM);
      }

      auto str = resp.name.c_str();

      if (nullptr != stop)
        stop(const_cast<char*>(str));

      break;
    }
      //
      break;
    default:
      break;
      // do nothing
  }
}

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
