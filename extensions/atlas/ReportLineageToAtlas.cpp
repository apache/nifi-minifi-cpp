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
#include <regex>
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
std::string atlasTypeFor(const Dataset& dataset, ReportLineageToAtlas::S3ModelVersion s3_version) {
  if (dataset.system == "kafka") {
    return "kafka_topic";
  }
  if (dataset.system == "s3") {
    return s3_version == ReportLineageToAtlas::S3ModelVersion::V1 ? "aws_s3_pseudo_dir" : "aws_s3_v2_directory";
  }
  if (dataset.system == "file") {
    return "fs_path";
  }
  if (dataset.system == "http") {
    return "http_endpoint";
  }
  if (dataset.system == "jdbc") {
    return "rdbms_instance";
  }
  // The remote port's real kind, agreed on by both instances so their entities merge in Atlas:
  // a SEND targets a remote input port, a RECEIVE pulls from a remote output port.
  if (dataset.system == "site-to-site-input-port") {
    return "nifi_input_port";
  }
  if (dataset.system == "site-to-site-output-port") {
    return "nifi_output_port";
  }
  return dataset.system;
}

// Reduces a filesystem path to its containing directory, i.e. strips the last
// component. Applied only when Filesystem Path Level = DIRECTORY.
std::string dirOf(std::string_view path) {
  const auto last_slash = path.rfind('/');
  if (last_slash == std::string::npos) {
    return std::string{path};
  }
  return std::string{path.substr(0, last_slash + 1)};
}

}  // namespace

void ReportLineageToAtlas::initialize() {
  setSupportedProperties(Properties);
}

AtlasEntity ReportLineageToAtlas::datasetToEntity(const Dataset& dataset, FsPathLevel fs_path_level, S3ModelVersion s3_model_version,
    const NamespaceResolver& namespace_resolver) {
  AtlasEntity entry;
  entry.type_name = atlasTypeFor(dataset, s3_model_version);
  entry.string_attributes = dataset.attributes;
  std::string identifier = dataset.identifier;
  if (dataset.system == "file" && fs_path_level == FsPathLevel::DIRECTORY) {
    identifier = dirOf(identifier);
    // The entity now describes the directory, not the file, so `path` has to follow
    // `name`. Leaving it as the file path would make the directory entity's `path`
    // whichever file happened to be seen last, since Atlas upserts by qualifiedName.
    entry.string_attributes["name"] = identifier;
    entry.string_attributes["path"] = identifier;
  }
  const auto namespace_name = namespace_resolver.resolve(dataset.host.value_or(""));
  entry.qualified_name = identifier + "@" + namespace_name;
  // Atlas' base "Asset" trait declares `name` as mandatory. Every dataset type we emit
  // (fs_path, kafka_topic, nifi_output_port, nifi_input_port, aws_s3_v2_*, hive_table)
  // inherits from Asset, so a missing name gets the whole bulk POST rejected with
  // "mandatory attribute value missing in type Asset". Use the identifier as a
  // sensible default when the extractor didn't already supply a `name`.
  if (!entry.string_attributes.contains("name")) {
    entry.string_attributes["name"] = identifier;
  }
  return entry;
}

void ReportLineageToAtlas::onSchedule(core::reporting::ReportingTaskContext& context) {
  atlas_base_urls_.clear();
  if (auto atlas_urls = context.getProperty(AtlasUrls); atlas_urls && !atlas_urls->empty()) {
    atlas_base_urls_ = utils::string::splitAndTrimRemovingEmpty(*atlas_urls, ",");
  }
  if (atlas_base_urls_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Atlas URLs property is required");
  }

  atlas_username_ = std::nullopt;
  if (auto username = context.getProperty(AtlasUsername); username && !username->empty()) {
    atlas_username_ = *username;
  }
  atlas_password_ = std::nullopt;
  if (auto password = context.getProperty(AtlasPassword); password && !password->empty()) {
    atlas_password_ = *password;
  }

  ssl_context_service_ = nullptr;
  if (auto service_name = context.getProperty(SSLContext); service_name && !service_name->empty()) {
    auto service = context.getControllerService(*service_name, uuid_);
    if (!service) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "SSL Context Service '" + *service_name + "' not found");
    }
    ssl_context_service_ = std::dynamic_pointer_cast<controllers::SSLContextServiceInterface>(service);
    if (!ssl_context_service_) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "SSL Context Service '" + *service_name + "' is not an SSLContextServiceInterface");
    }
  }

  if (auto nifi_url = context.getProperty(NiFiUrl); nifi_url) {
    nifi_url_ = *nifi_url;
  }

  namespace_resolver_ = NamespaceResolver{};
  if (auto default_namespace = context.getProperty(DefaultNamespace); default_namespace && !default_namespace->empty()) {
    namespace_resolver_.setDefaultNamespace(*default_namespace);
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Default Metadata Namespace property is required");
  }
  // Dynamic properties: hostnamePattern.<ns> = <regex1> <regex2> ...
  for (const auto& key : context.getDynamicPropertyKeys()) {
    static constexpr std::string_view prefix = "hostnamePattern.";
    if (!key.starts_with(prefix)) {
      continue;
    }
    const auto namespace_name = key.substr(prefix.size());
    if (namespace_name.empty()) {
      continue;
    }
    if (auto value = context.getRawDynamicProperty(key); value) {
      try {
        namespace_resolver_.addRuleFromWhitespaceList(namespace_name, *value);
      } catch (const std::regex_error& err) {
        // std::regex_error is a std::logic_error, not a minifi Exception, so without this
        // it escapes onSchedule as a bare "Unexpected end of regex" with no hint as to
        // which property is malformed.
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid regex in dynamic property '" + key + "': " + err.what());
      }
    }
  }

  if (auto s3_version = context.getProperty(AwsS3ModelVersion); s3_version) {
    s3_model_version_ = (*s3_version == "v1") ? S3ModelVersion::V1 : S3ModelVersion::V2;
  }
  if (auto path_level = context.getProperty(FilesystemPathLevel); path_level) {
    fs_path_level_ = (*path_level == "FILE") ? FsPathLevel::FILE : FsPathLevel::DIRECTORY;
  }
  if (auto batch_size = context.getProperty(ProvenanceBatchSize); batch_size) {
    if (const auto parsed = parsing::parseIntegral<std::size_t>(*batch_size)) {
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

  // 3. Snapshot the flow topology and translate it into Atlas entities. The flow rarely
  //    changes, so skip both the translation and the (blocking) publish while it matches
  //    what we last published successfully. The caches are updated only on success, so a
  //    failed publish is retried on the next trigger.
  auto topology = context.getFlowTopology();
  if (!published_topology_ || *published_topology_ != topology) {
    auto flow_result = FlowPathBuilder::build(topology, namespace_resolver_.defaultNamespace(), nifi_url_);
    if (auto res = atlas.createOrUpdateEntities(flow_result.entities); !res) {
      logger_->log_error("Failed to publish flow topology to Atlas: {}", res.error());
      context.yield();
      return;
    }
    logger_->log_debug("Published flow topology to Atlas: {} entities", flow_result.entities.size());
    published_flow_result_ = std::move(flow_result);
    published_topology_ = std::move(topology);
  } else {
    logger_->log_debug("Flow topology unchanged since last publish; skipping republish");
  }
  const auto& flow_result = *published_flow_result_;

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
    if (!event) {
      continue;
    }
    const auto refs = extractor_dispatcher_.dispatch(*event);
    if (refs.empty()) {
      continue;
    }
    const auto proc_it = flow_result.processor_to_path.find(event->getComponentId());
    if (proc_it == flow_result.processor_to_path.end()) {
      // Event from a processor no longer in the flow (deleted since last trigger?). Skip.
      continue;
    }
    const auto& path_qn = proc_it->second.flow_path_qualified_name;
    const auto convert = [&](const Dataset& dataset) {
      auto entry = datasetToEntity(dataset, fs_path_level_, s3_model_version_, namespace_resolver_);
      external_entities[entry.qualified_name] = entry;
      return AtlasEntity::Reference{entry.type_name, entry.qualified_name};
    };
    for (const auto& input : refs.inputs) {
      path_inputs[path_qn].insert(convert(input));
    }
    for (const auto& output : refs.outputs) {
      path_outputs[path_qn].insert(convert(output));
    }
  }

  // 5. Build partial-update entities: one nifi_flow_path per path that saw new
  //    events, with the accumulated (deduped) inputs/outputs. Atlas upserts by
  //    qualifiedName, so this merges into whatever's already there.
  std::unordered_map<std::string, AtlasEntity> updates;
  const auto touch_path = [&](const std::string& path_qn) -> AtlasEntity& {
    auto entity_it = std::ranges::find_if(flow_result.entities, [&](const AtlasEntity& entity) {
      return entity.qualified_name == path_qn;
    });
    return updates.insert({path_qn, *entity_it}).first->second;
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
  for (auto& [_, entity] : external_entities) {
    to_publish.push_back(std::move(entity));
  }
  for (auto& [_, entity] : updates) {
    to_publish.push_back(std::move(entity));
  }

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
