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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_METRICS_PROCMETRICS_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_METRICS_PROCMETRICS_H_

#include "core/Resource.h"
#include <sstream>
#include <map>
#include <sys/time.h>
#include <sys/resource.h>

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
 * Justification and Purpose: Provides Connection queue metrics. Provides critical information to the
 * C2 server.
 *
 */
class ProcessMetrics : public ResponseNode {
 public:

  ProcessMetrics(const std::string &name, uuid_t uuid)
      : ResponseNode(name, uuid) {
  }

  ProcessMetrics(const std::string &name)
      : ResponseNode(name, 0) {
  }

  ProcessMetrics() {
  }

  virtual std::string getName() const {
    return "ProcessMetrics";
  }

  std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;

    struct rusage my_usage;
    getrusage(RUSAGE_SELF, &my_usage);

    SerializedResponseNode memory;
    memory.name = "MemoryMetrics";

    SerializedResponseNode maxrss;
    maxrss.name = "maxrss";

    maxrss.value = (uint64_t)my_usage.ru_maxrss;

    memory.children.push_back(maxrss);
    serialized.push_back(memory);

    SerializedResponseNode cpu;
    cpu.name = "CpuMetrics";
    SerializedResponseNode ics;
    ics.name = "involcs";

    ics.value = (uint64_t)my_usage.ru_nivcsw;

    cpu.children.push_back(ics);
    serialized.push_back(cpu);

    return serialized;
  }

 protected:

};

REGISTER_RESOURCE(ProcessMetrics);

} /* namespace metrics */
} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_STATE_METRICS_QUEUEMETRICS_H_ */
