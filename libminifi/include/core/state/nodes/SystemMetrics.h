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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_NODES_SYSTEMMETRICS_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_NODES_SYSTEMMETRICS_H_

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/Resource.h"

#ifndef _WIN32
#include <sys/utsname.h>

#endif
#include "../nodes/DeviceInformation.h"
#include "../nodes/MetricsBase.h"
#include "Connection.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

/**
 * Justification and Purpose: Provides system information, including critical device information.
 *
 */
class SystemInformation : public DeviceInformation {
 public:
  SystemInformation(const std::string& name, const utils::Identifier& uuid)
      : DeviceInformation(name, uuid) {
  }

  SystemInformation(const std::string &name) // NOLINT
      : DeviceInformation(name) {
  }

  SystemInformation()
      : DeviceInformation("systeminfo") {
  }

  virtual std::string getName() const {
    return "systeminfo";
  }

  std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;
    SerializedResponseNode identifier;
    identifier.name = "identifier";
    identifier.value = "identifier";
#ifndef WIN32
    SerializedResponseNode systemInfo;
    systemInfo.name = "systemInfo";

    SerializedResponseNode vcores;
    vcores.name = "vCores";
    size_t ncpus = std::thread::hardware_concurrency();

    vcores.value = (uint32_t)ncpus;

    systemInfo.children.push_back(vcores);

    SerializedResponseNode mem;
    mem.name = "physicalMem";
#if defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
    size_t mema = (size_t) sysconf(_SC_PHYS_PAGES) * (size_t) sysconf(_SC_PAGESIZE);
#endif
    mem.value = (uint32_t)mema;

    systemInfo.children.push_back(mem);

    SerializedResponseNode arch;
    arch.name = "machinearch";

    utsname buf;

    if (uname(&buf) == -1) {
      arch.value = "unknown";
    } else {
      arch.value = std::string(buf.machine);
    }

    systemInfo.children.push_back(arch);
    serialized.push_back(systemInfo);
#endif
    serialized.push_back(identifier);

    return serialized;
  }

 protected:
};

}  // namespace response
}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_NODES_SYSTEMMETRICS_H_
