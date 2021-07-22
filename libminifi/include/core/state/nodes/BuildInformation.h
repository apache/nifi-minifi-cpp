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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_NODES_BUILDINFORMATION_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_NODES_BUILDINFORMATION_H_

#include <string>
#include <vector>

#ifndef WIN32

#if ( defined(__APPLE__) || defined(__MACH__) || defined(BSD))
#include <net/if_dl.h>
#include <net/if_types.h>
#endif

#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <unistd.h>

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <functional>
#include <map>
#include <sstream>

#include "../../../agent/agent_version.h"
#include "../nodes/MetricsBase.h"
#include "Connection.h"
#include "core/ClassLoader.h"
#include "io/ClientSocket.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

/**
 * Justification and Purpose: Provides build information
 * for this agent.
 */
class BuildInformation : public DeviceInformation {
 public:
  BuildInformation(const std::string &name, const utils::Identifier &uuid)
      : DeviceInformation(name, uuid) {
  }

  BuildInformation(const std::string &name) // NOLINT
      : DeviceInformation(name) {
  }

  std::string getName() const {
    return "BuildInformation";
  }

  std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;

    SerializedResponseNode build_version;
    build_version.name = "build_version";
    build_version.value = AgentBuild::VERSION;

    SerializedResponseNode build_rev;
    build_rev.name = "build_rev";
    build_rev.value = AgentBuild::BUILD_REV;

    SerializedResponseNode build_date;
    build_date.name = "build_date";
    build_date.value = AgentBuild::BUILD_DATE;

    SerializedResponseNode compiler;
    compiler.name = "compiler";
    {
      SerializedResponseNode compiler_command;
      compiler_command.name = "compiler_command";
      compiler_command.value = AgentBuild::COMPILER;

      SerializedResponseNode compiler_version;
      compiler_version.name = "compiler_version";
      compiler_version.value = AgentBuild::COMPILER_VERSION;

      SerializedResponseNode compiler_flags;
      compiler_flags.name = "compiler_flags";
      compiler_flags.value = AgentBuild::COMPILER_FLAGS;

      compiler.children.push_back(compiler_command);
      compiler.children.push_back(compiler_version);
      compiler.children.push_back(compiler_flags);
    }
    SerializedResponseNode device_id;
    device_id.name = "device_id";
    device_id.value = AgentBuild::BUILD_IDENTIFIER;

    serialized.push_back(build_version);
    serialized.push_back(build_rev);
    serialized.push_back(build_date);
    serialized.push_back(compiler);
    serialized.push_back(device_id);

    return serialized;
  }
};

}  // namespace response
}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_NODES_BUILDINFORMATION_H_
