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
#include "FlowPathBuilder.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

namespace org::apache::nifi::minifi::extensions::atlas {

namespace {

using Topology = core::reporting::FlowTopology;

std::string qualifiedName(std::string_view id, std::string_view namespace_name) {
  return std::string{id} + "@" + std::string{namespace_name};
}

// The agent's own flow is described with minifi_* types: nothing else writes these
// entities, so we own them outright and they are not confusable with a NiFi instance's.
// The remote ports on the far side of a site-to-site transfer are the exception — those
// keep NiFi's type names so the entities merge with the receiving NiFi's, and they are
// produced by the extractors (see atlasTypeFor), not here.
constexpr const char* MINIFI_FLOW = "minifi_flow";
constexpr const char* MINIFI_FLOWPATH = "minifi_flow_path";
constexpr const char* MINIFI_QUEUE = "minifi_queue";
// Back-reference attribute declared by minifi_component.
constexpr const char* MINIFI_FLOWATTR = "minifiFlow";

}  // namespace

FlowPathBuilder::Result FlowPathBuilder::build(const Topology& topology, std::string_view namespace_name, std::string_view flow_url) {
  Result result;
  const auto ns = std::string{namespace_name};

  // Index processors by UUID for O(1) lookup during the graph walk. Process-group ports
  // arrive in `processors` like any other node, so they take part in the walk and a path
  // running through one stays a single continuous path.
  std::unordered_map<std::string, const Topology::Processor*> proc_by_uuid;
  proc_by_uuid.reserve(topology.processors.size());
  for (const auto& p : topology.processors) {
    proc_by_uuid.emplace(p.uuid.to_string(), &p);
  }

  // Adjacency: only consider connections where both endpoints are known processors, so an
  // endpoint we have no node for (e.g. a funnel, or something added since the snapshot)
  // is skipped rather than producing a dangling edge.
  std::unordered_map<std::string, std::vector<std::string>> successors;    // proc_uuid -> [downstream proc_uuid]
  std::unordered_map<std::string, std::vector<std::string>> predecessors;  // proc_uuid -> [upstream proc_uuid]

  for (const auto& conn : topology.connections) {
    const auto src = std::string{conn.source_uuid.to_string().view()};
    const auto dst = std::string{conn.destination_uuid.to_string().view()};
    if (!proc_by_uuid.contains(src) || !proc_by_uuid.contains(dst)) {
      continue;
    }
    successors[src].push_back(dst);
    predecessors[dst].push_back(src);
  }

  // A processor is a "path head" if:
  //   - it has no processor predecessor, OR
  //   - any predecessor has >1 successor (upstream fan-out), OR
  //   - it has >1 predecessor (fan-in on itself).
  const auto is_path_head = [&](const std::string& uuid) {
    const auto pit = predecessors.find(uuid);
    if (pit == predecessors.end() || pit->second.empty()) return true;
    if (pit->second.size() > 1) return true;
    for (const auto& pred : pit->second) {
      const auto sit = successors.find(pred);
      if (sit != successors.end() && sit->second.size() > 1) return true;
    }
    return false;
  };

  // A path *breaks* right BEFORE a fan-in or fan-out — meaning:
  //   Walk forward from head H. Include H. Then look at H's successors:
  //     - If H has >1 successor (fan-out), stop; H is the only node in this path.
  //     - Else the single successor S: if S itself is a path head (multiple predecessors),
  //       stop and don't include S. Otherwise include S and repeat.
  // The single-predecessor / fan-in rule means the walk terminates on entering any
  // node that other paths also point at.
  const auto walk_from_head = [&](const std::string& head) {
    std::vector<std::string> path{head};
    std::string current = head;
    while (true) {
      const auto sit = successors.find(current);
      if (sit == successors.end() || sit->second.empty()) break;
      if (sit->second.size() > 1) break;  // fan-out: this path ends at `current`
      const auto& next = sit->second.front();
      if (is_path_head(next)) break;      // fan-in on next node: it starts its own path
      path.push_back(next);
      current = next;
    }
    return path;
  };

  // Discover heads and build paths. Iterate processors in insertion order to keep
  // deterministic output regardless of hash-map iteration order.
  struct BuiltPath {
    std::string head_uuid;
    std::string qualified_name;
    std::vector<std::string> processor_uuids;
  };
  std::vector<BuiltPath> paths;
  std::unordered_set<std::string> path_head_seen;

  for (const auto& p : topology.processors) {
    const auto uuid = std::string{p.uuid.to_string().view()};
    if (!is_path_head(uuid)) continue;
    if (!path_head_seen.insert(uuid).second) continue;
    BuiltPath bp;
    bp.head_uuid = uuid;
    bp.qualified_name = qualifiedName(uuid, ns);
    bp.processor_uuids = walk_from_head(uuid);
    paths.push_back(std::move(bp));
  }

  // Assign every walked processor to its owning path so provenance dispatch can
  // find "which path did this event's componentId belong to".
  for (const auto& bp : paths) {
    for (const auto& proc_uuid : bp.processor_uuids) {
      result.processor_to_path.emplace(proc_uuid, FlowPathAssignment{.flow_path_qualified_name = bp.qualified_name});
    }
  }

  // A processor covered by no path (e.g. isolated processor with no connections)
  // becomes its own single-node path. This preserves the invariant that every
  // processor is in exactly one path.
  for (const auto& p : topology.processors) {
    const auto uuid = std::string{p.uuid.to_string().view()};
    if (result.processor_to_path.contains(uuid)) continue;
    BuiltPath bp;
    bp.head_uuid = uuid;
    bp.qualified_name = qualifiedName(uuid, ns);
    bp.processor_uuids = {uuid};
    result.processor_to_path.emplace(uuid, FlowPathAssignment{.flow_path_qualified_name = bp.qualified_name});
    paths.push_back(std::move(bp));
  }

  // Build queue entities: one per inter-path connection edge (fan-out end or
  // fan-in start). qualifiedName is <destinationProcessorId>@<ns>.
  // Track which paths get which inputs/outputs so we can wire them up.
  std::unordered_map<std::string, std::unordered_set<AtlasEntity::Reference>> path_inputs;   // path qname -> refs
  std::unordered_map<std::string, std::unordered_set<AtlasEntity::Reference>> path_outputs;

  std::vector<AtlasEntity> queue_entities;
  std::unordered_set<std::string> queue_qnames;
  for (const auto& conn : topology.connections) {
    const auto src = std::string{conn.source_uuid.to_string().view()};
    const auto dst = std::string{conn.destination_uuid.to_string().view()};
    if (!proc_by_uuid.contains(src) || !proc_by_uuid.contains(dst)) continue;
    const auto src_path_it = result.processor_to_path.find(src);
    const auto dst_path_it = result.processor_to_path.find(dst);
    if (src_path_it == result.processor_to_path.end() || dst_path_it == result.processor_to_path.end()) continue;
    if (src_path_it->second.flow_path_qualified_name == dst_path_it->second.flow_path_qualified_name) continue;

    // Cross-path edge: emit a minifi_queue keyed by the downstream processor UUID.
    const auto queue_qn = qualifiedName(dst, ns);
    if (queue_qnames.insert(queue_qn).second) {
      AtlasEntity q;
      q.type_name = MINIFI_QUEUE;
      q.qualified_name = queue_qn;
      q.string_attributes["name"] = "queue to " + std::string{conn.name.empty() ? proc_by_uuid.at(dst)->name : conn.name};
      queue_entities.push_back(std::move(q));
    }
    path_outputs[src_path_it->second.flow_path_qualified_name].emplace(MINIFI_QUEUE, queue_qn);
    path_inputs[dst_path_it->second.flow_path_qualified_name].emplace(MINIFI_QUEUE, queue_qn);
  }

  const auto flow_url_str = std::string{flow_url};

  // Emit minifi_flow.
  AtlasEntity flow;
  flow.type_name = MINIFI_FLOW;
  flow.qualified_name = qualifiedName(topology.root_group_uuid.to_string().view(), ns);
  flow.string_attributes["name"] = topology.root_group_name.empty() ? "MiNiFi Flow" : topology.root_group_name;
  if (!flow_url.empty()) {
    flow.string_attributes.emplace("url", flow_url_str);
  }

  std::unordered_set<AtlasEntity::Reference> flow_paths_refs;
  flow_paths_refs.reserve(paths.size());
  for (const auto& bp : paths) {
    flow_paths_refs.emplace(MINIFI_FLOWPATH, bp.qualified_name);
  }
  if (!flow_paths_refs.empty()) {
    flow.ref_list_attributes.emplace("flowPaths", std::move(flow_paths_refs));
  }
  if (!queue_entities.empty()) {
    std::unordered_set<AtlasEntity::Reference> queue_refs;
    queue_refs.reserve(queue_entities.size());
    for (const auto& q : queue_entities) {
      queue_refs.emplace(MINIFI_QUEUE, q.qualified_name);
    }
    flow.ref_list_attributes.emplace("queues", std::move(queue_refs));
  }
  result.entities.push_back(std::move(flow));

  // Emit minifi_flow_path entities, each with a comma-joined display name and any
  // inputs/outputs collected above. The minifiFlow back-reference lets Atlas render
  // path ownership.
  const auto root_group_uuid_str = std::string{topology.root_group_uuid.to_string().view()};
  AtlasEntity::Reference flow_ref{MINIFI_FLOW, qualifiedName(root_group_uuid_str, ns)};
  for (auto& bp : paths) {
    AtlasEntity path_entity;
    path_entity.type_name = MINIFI_FLOWPATH;
    path_entity.qualified_name = bp.qualified_name;
    std::string name;
    for (const auto& proc_uuid : bp.processor_uuids) {
      const auto* proc = proc_by_uuid.at(proc_uuid);
      if (!name.empty()) name += ", ";
      name += proc->name;
    }
    path_entity.string_attributes["name"] = std::move(name);
    path_entity.ref_attributes.emplace(MINIFI_FLOWATTR, flow_ref);
    // Atlas rejects the bulk POST with a 404 if minifi_flow_path.url is missing (the type
    // declares it mandatory). NiFi's shape is `<flow_url>?processGroupId=<root>&componentIds=<head>`;
    // we mirror that. flow_url is passed in unconditionally by ReportLineageToAtlas, so we don't
    // guard on emptiness here - a blank URL still satisfies Atlas' mandatory-attribute check.
    if (!bp.processor_uuids.empty()) {
      path_entity.string_attributes["url"] = flow_url_str + "?processGroupId=" +
          root_group_uuid_str + "&componentIds=" + bp.processor_uuids.front();
    } else {
      path_entity.string_attributes["url"] = flow_url_str;
    }
    if (auto it = path_inputs.find(bp.qualified_name); it != path_inputs.end()) {
      path_entity.ref_list_attributes.emplace("inputs", std::move(it->second));
    }
    if (auto it = path_outputs.find(bp.qualified_name); it != path_outputs.end()) {
      path_entity.ref_list_attributes.emplace("outputs", std::move(it->second));
    }
    result.entities.push_back(std::move(path_entity));
  }

  // Emit queue entities.
  for (auto& q : queue_entities) {
    q.ref_attributes.emplace(MINIFI_FLOWATTR, flow_ref);
    result.entities.push_back(std::move(q));
  }

  return result;
}

}  // namespace org::apache::nifi::minifi::extensions::atlas
