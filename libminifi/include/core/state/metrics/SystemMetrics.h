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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_METRICS_SYSMETRICS_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_METRICS_SYSMETRICS_H_

#include "core/Resource.h"
#include <sstream>
#include <map>
#ifndef _WIN32
#include <sys/utsname.h>
#endif
#include "MetricsBase.h"
#include "Connection.h"
#include "DeviceInformation.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace metrics {

/**
 * Justification and Purpose: Provides Connection queue metrics. Provides critical information to the
 * C2 server.
 *
 */
class SystemInformation : public DeviceInformation {
 public:

  SystemInformation(const std::string &name, uuid_t uuid)
      : DeviceInformation(name, uuid) {
  }

  SystemInformation(const std::string &name)
      : DeviceInformation(name, 0) {
  }

  SystemInformation()
      : DeviceInformation("SystemInformation", 0) {
  }

  std::string getName() {
    return "SystemInformation";
  }

  std::vector<MetricResponse> serialize() {
    std::vector<MetricResponse> serialized;

    MetricResponse vcores;
    vcores.name = "vcores";
    int cpus[2] = { 0 };
    size_t ncpus = std::thread::hardware_concurrency();

    vcores.value = std::to_string(ncpus);

    serialized.push_back(vcores);

    MetricResponse mem;
    mem.name = "physicalmem";
#if defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
    size_t mema = (size_t) sysconf( _SC_PHYS_PAGES) * (size_t) sysconf( _SC_PAGESIZE);
#endif
    mem.value = std::to_string(mema);

    serialized.push_back(mem);

    MetricResponse arch;
    arch.name = "machinearch";

    utsname buf;

    if (uname(&buf) == -1) {
      arch.value = "unknown";
    } else {
      arch.value = buf.machine;
    }

    serialized.push_back(arch);

    return serialized;
  }

 protected:


};

REGISTER_RESOURCE(SystemInformation);
} /* namespace metrics */
} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_STATE_METRICS_QUEUEMETRICS_H_ */
