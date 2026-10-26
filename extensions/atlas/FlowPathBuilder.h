/**
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
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "AtlasClient.h"
#include "minifi-cpp/core/reporting/ReportingTaskContext.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::extensions::atlas {

// Turns a FlowTopology snapshot into the set of Atlas entities that describe it:
// one minifi_flow (the root), one minifi_flow_path per fork/join-delimited chain of
// processors, and one minifi_queue per fan-out/fan-in edge.
//
// Process-group ports are not datasets: they arrive in topology.processors like any
// other node, so a path running through one stays a single continuous path.
//
// Everything here is minifi_*: these describe the agent's own flow, which nothing else
// writes. The remote ports on the far side of a site-to-site transfer are the exception
// and keep NiFi's type names, so the entities merge with the receiving NiFi's — those
// come from the extractors, not from here.
//
// The mapping from processor UUID → owning minifi_flow_path qualifiedName is exposed
// so the reporting task can look up which path a given provenance event belongs to.
struct FlowPathAssignment {
  std::string flow_path_qualified_name;
};

class FlowPathBuilder {
 public:
  struct Result {
    std::vector<AtlasEntity> entities;
    // Processor UUID (string form) → owning minifi_flow_path qualifiedName.
    std::unordered_map<std::string, FlowPathAssignment> processor_to_path;
  };

  // Build the Atlas entities from a topology snapshot. The namespace is applied
  // to every qualifiedName. The flow_url is embedded on minifi_flow.url.
  static Result build(const core::reporting::FlowTopology& topology, std::string_view namespace_name, std::string_view flow_url);
};

}  // namespace org::apache::nifi::minifi::extensions::atlas
