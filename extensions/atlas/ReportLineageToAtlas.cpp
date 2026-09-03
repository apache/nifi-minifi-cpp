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
#include "ReportLineageToAtlas.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "AtlasClient.h"
#include "FlowPathBuilder.h"
#include "core/Resource.h"
#include "minifi-cpp/Exception.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "utils/ParsingUtils.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::extensions::atlas {

namespace {

// Maps the extractor's neutral system name to the Atlas entity type this
// reporting task chose to represent it with. Configurable systems (S3, filesystem)
// consult the reporting task's properties before mapping.
std::string atlasTypeFor(const Dataset& ds, ReportLineageToAtlas::S3ModelVersion s3_version) {
  if (ds.system == "kafka") return "kafka_topic";
  if (ds.system == "s3") {
    return s3_version == ReportLineageToAtlas::S3ModelVersion::V1 ? "aws_s3_pseudo_dir" : "aws_s3_v2_directory";
  }
  if (ds.system == "file") return "fs_path";
  if (ds.system == "http") return "http_endpoint";
  if (ds.system == "jdbc") return "rdbms_instance";
  if (ds.system == "site-to-site-port") return "nifi_output_port";  // remote sink; remote source would use nifi_input_port
  return ds.system;
}

std::vector<std::string> splitCsv(std::string_view input) {
  return utils::string::splitAndTrimRemovingEmpty(input, ",");
}

// Reduces a filesystem path to its containing directory, i.e. strips the last
// component. Applied only when Filesystem Path Level = DIRECTORY.
std::string dirOf(std::string_view path) {
  const auto last_slash = path.rfind('/');
  if (last_slash == std::string::npos) return std::string{path};
  return std::string{path.substr(0, last_slash + 1)};
}

}  // namespace

void ReportLineageToAtlas::initialize() {
  setSupportedProperties(Properties);
}

void ReportLineageToAtlas::onSchedule(core::reporting::ReportingTaskContext& context) {
  atlas_base_urls_.clear();
  if (auto v = context.getProperty(AtlasUrls); v && !v->empty()) {
    atlas_base_urls_ = splitCsv(*v);
  }
  if (atlas_base_urls_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Atlas URLs property is required");
  }

  atlas_username_ = std::nullopt;
  if (auto v = context.getProperty(AtlasUsername); v && !v->empty()) atlas_username_ = *v;
  atlas_password_ = std::nullopt;
  if (auto v = context.getProperty(AtlasPassword); v && !v->empty()) atlas_password_ = *v;

  ssl_context_service_ = nullptr;
  if (auto v = context.getProperty(SSLContext); v && !v->empty()) {
    auto svc = context.getControllerService(*v, uuid_);
    if (!svc) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "SSL Context Service '" + *v + "' not found");
    }
    ssl_context_service_ = std::dynamic_pointer_cast<controllers::SSLContextServiceInterface>(svc);
    if (!ssl_context_service_) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "SSL Context Service '" + *v + "' is not an SSLContextServiceInterface");
    }
  }

  if (auto v = context.getProperty(NiFiUrl); v) nifi_url_ = *v;

  namespace_resolver_ = NamespaceResolver{};
  if (auto v = context.getProperty(DefaultNamespace); v && !v->empty()) {
    namespace_resolver_.setDefaultNamespace(*v);
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Default Metadata Namespace property is required");
  }
  // Dynamic properties: hostnamePattern.<ns> = <regex1> <regex2> ...
  // ReportingTaskContext doesn't expose a dynamic-property enumeration API, so we
  // rely on the ProcessContext downcast (safe: every reporting task context IS a
  // ProcessContext at runtime — see ProcessContextImpl). If the cast fails (in a
  // test harness with a mock), we simply skip dynamic namespace rules.
  if (auto* proc_ctx = dynamic_cast<core::ProcessContext*>(&context)) {
    for (const auto& key : proc_ctx->getDynamicPropertyKeys()) {
      static constexpr std::string_view prefix = "hostnamePattern.";
      if (!key.starts_with(prefix)) continue;
      const auto ns_name = key.substr(prefix.size());
      if (ns_name.empty()) continue;
      if (auto value = proc_ctx->getDynamicProperty(key); value) {
        namespace_resolver_.addRuleFromWhitespaceList(ns_name, *value);
      }
    }
  }

  if (auto v = context.getProperty(AwsS3ModelVersion); v) {
    s3_model_version_ = (*v == "v1") ? S3ModelVersion::V1 : S3ModelVersion::V2;
  }
  if (auto v = context.getProperty(FilesystemPathLevel); v) {
    fs_path_level_ = (*v == "FILE") ? FsPathLevel::FILE : FsPathLevel::DIRECTORY;
  }
  if (auto v = context.getProperty(ProvenanceBatchSize); v) {
    if (const auto parsed = parsing::parseIntegral<std::size_t>(*v)) {
      provenance_batch_size_ = *parsed;
    }
  }

  type_defs_registered_ = false;
  logger_->log_info("ReportLineageToAtlas scheduled: atlas={} namespace={} batch={} paths={}",
      atlas_base_urls_.front(), namespace_resolver_.defaultNamespace(), provenance_batch_size_,
      fs_path_level_ == FsPathLevel::DIRECTORY ? "DIRECTORY" : "FILE");
}

void ReportLineageToAtlas::onTrigger(core::reporting::ReportingTaskContext& context) {
  // 1. Fresh AtlasClient per trigger — matches NiFi's design, keeps auth state
  //    scoped, and makes each trigger independently recoverable.
  AtlasClient atlas{{
      .base_urls = atlas_base_urls_,
      .username = atlas_username_,
      .password = atlas_password_,
      .ssl_context_service = ssl_context_service_,
      .logger = logger_,
  }};

  // 2. Register the six NiFi typedefs on the first successful trigger. If Atlas
  //    is unreachable we back off and try again next trigger.
  if (!type_defs_registered_) {
    if (auto res = atlas.registerNiFiTypeDefs(); !res) {
      logger_->log_error("Failed to register Atlas typedefs: {}", res.error());
      context.yield();
      return;
    }
    type_defs_registered_ = true;
  }

  // 3. Snapshot the flow topology and translate it into Atlas entities.
  const auto topology = context.getFlowTopology();
  const auto flow_result = FlowPathBuilder::build(topology, namespace_resolver_.defaultNamespace(), nifi_url_);

  if (auto res = atlas.createOrUpdateEntities(flow_result.entities); !res) {
    logger_->log_error("Failed to publish flow topology to Atlas: {}", res.error());
    context.yield();
    return;
  }
  logger_->log_debug("Published flow topology to Atlas: {} entities", flow_result.entities.size());

  // 4. Consume provenance events, dispatch each through the extractors, and
  //    accumulate inputs/outputs per owning flow_path. Persist the cursor so
  //    a restart resumes where we left off.
  auto repo = context.getProvenanceRepository();
  if (!repo) {
    logger_->log_error("No provenance repository available; cannot report lineage edges");
    context.yield();
    return;
  }
  auto* state_manager = context.getStateManager();
  if (!state_manager) {
    logger_->log_error("No state manager available; cannot checkpoint provenance cursor");
    context.yield();
    return;
  }

  std::string cursor_str;
  {
    std::unordered_map<std::string, std::string> state;
    if (state_manager->get(state) && state.contains("cursor")) {
      cursor_str = state.at("cursor");
    }
  }
  auto cursor = repo->cursorFromString(cursor_str);
  if (!cursor_str.empty() && !cursor) {
    logger_->log_warn("Failed to parse persisted cursor; starting from beginning");
    cursor = repo->cursorFromString("");
  }

  auto batch_result = repo->getEvents(provenance_batch_size_, cursor.get());
  if (!batch_result) {
    logger_->log_error("Failed to read provenance events: {}", batch_result.error());
    context.yield();
    return;
  }
  const auto& events = batch_result.value();
  if (events.empty()) {
    logger_->log_debug("No new provenance events");
    return;
  }

  std::unordered_map<std::string, AtlasEntity> external_entities;
  // For each event, look up the owning flow path (via componentId) and append
  // the extracted Datasets to that path's accumulated inputs/outputs.
  std::unordered_map<std::string, std::unordered_set<AtlasEntity::Reference>> path_inputs;
  std::unordered_map<std::string, std::unordered_set<AtlasEntity::Reference>> path_outputs;
  for (const auto& event : events) {
    if (!event) continue;
    const auto refs = extractor_dispatcher_.dispatch(*event);
    if (refs.empty()) continue;
    const auto proc_it = flow_result.processor_to_path.find(event->getComponentId());
    if (proc_it == flow_result.processor_to_path.end()) {
      // Event from a processor no longer in the flow (deleted since last trigger?). Skip.
      continue;
    }
    const auto& path_qn = proc_it->second.flow_path_qualified_name;
    const auto convert = [&](const Dataset& ds) {
      const auto type = atlasTypeFor(ds, s3_model_version_);
      std::string identifier = ds.identifier;
      if (ds.system == "file" && fs_path_level_ == FsPathLevel::DIRECTORY) {
        identifier = dirOf(identifier);
      }
      const auto ns = namespace_resolver_.resolve(ds.host.value_or(""));
      AtlasEntity entry;
      entry.type_name = type;
      entry.qualified_name = identifier + "@" + ns;
      entry.string_attributes = ds.attributes;
      external_entities[entry.qualified_name] = entry;
      return AtlasEntity::Reference{entry.type_name, entry.qualified_name};
    };
    for (const auto& in : refs.inputs) path_inputs[path_qn].insert(convert(in));
    for (const auto& out : refs.outputs) path_outputs[path_qn].insert(convert(out));
  }

  // 5. Build partial-update entities: one nifi_flow_path per path that saw new
  //    events, with the accumulated (deduped) inputs/outputs. Atlas upserts by
  //    qualifiedName, so this merges into whatever's already there.
  std::unordered_map<std::string, AtlasEntity> updates;
  const auto touch_path = [&](const std::string& path_qn) -> AtlasEntity& {
    auto it = std::find_if(flow_result.entities.cbegin(), flow_result.entities.cend(), [&] (const AtlasEntity& e) {return e.qualified_name == path_qn;});
    return updates.insert({path_qn, *it}).first->second;
  };

  for (auto& [path_qn, refs] : path_inputs) {
    touch_path(path_qn).ref_list_attributes["inputs"] = refs;
  }
  for (auto& [path_qn, refs] : path_outputs) {
    touch_path(path_qn).ref_list_attributes["outputs"] = refs;
  }

  // Send external entities first, then path partial updates.
  std::vector<AtlasEntity> to_publish;
  to_publish.reserve(external_entities.size() + updates.size());
  for (auto& [_, e] : external_entities) to_publish.push_back(std::move(e));
  for (auto& [_, e] : updates) to_publish.push_back(std::move(e));

  if (!to_publish.empty()) {
    if (auto res = atlas.createOrUpdateEntities(to_publish); !res) {
      logger_->log_error("Failed to publish lineage updates to Atlas: {}", res.error());
      context.yield();
      return;
    }
    logger_->log_debug("Published lineage updates to Atlas: {} entities", to_publish.size());
  }

  // 6. Advance the cursor.
  if (cursor) {
    std::unordered_map<std::string, std::string> state{{"cursor", cursor->toString()}};
    if (!state_manager->set(state)) {
      logger_->log_error("Failed to persist provenance cursor");
    }
  }
}

REGISTER_RESOURCE(ReportLineageToAtlas, ReportingTask);

}  // namespace org::apache::nifi::minifi::extensions::atlas
