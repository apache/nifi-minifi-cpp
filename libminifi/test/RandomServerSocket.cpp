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

#include "./RandomServerSocket.h"

#ifdef WIN32
#pragma comment(lib, "Ws2_32.lib")
#endif /* WIN32 */

#include <sstream>
#include <random>
#include <string>
#include <memory>

#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

  RandomServerSocket::RandomServerSocket(const std::string& host, uint16_t offset, uint16_t range, uint16_t retries) :
      ServerSocket::ServerSocket(std::make_shared<org::apache::nifi::minifi::io::SocketContext>(std::make_shared<minifi::Configure>()), host, 0, 1) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dis(offset, offset + range);
    auto logger = logging::LoggerFactory<RandomServerSocket>::getLogger();
    for (uint16_t i = 0; i < retries; ++i) {
      setPort(dis(gen));
      if (initialize() == 0) {
        logger->log_info("Created socket listens on generated port: %hu", getPort());
        return;
      }
    }
    std::stringstream error;
    error << "Couldn't bind to a port between " << offset << " and " << offset+range << " in " << retries << " try!";
    logger->log_error(error.str().c_str());
    throw error.str();
  }

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
