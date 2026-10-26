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

#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "http/HTTPClient.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/controllers/SSLContextServiceInterface.h"

namespace org::apache::nifi::minifi::extensions::atlas {

// AtlasEntity is the JSON-shaped payload passed to Atlas's bulk-entity endpoint.
// One instance corresponds to one Atlas entity (minifi_flow, minifi_flow_path, kafka_topic,
// etc.). type_name is the Atlas type; qualified_name is the entity's Atlas-wide unique
// key; attributes carries the remaining typed fields. References to other entities
// (e.g. "inputs": [{typeName, uniqueAttributes.qualifiedName}]) are represented via
// nested Reference objects.
struct AtlasEntity {
  struct Reference {
    std::string type_name;
    std::string qualified_name;

    bool operator==(const Reference& other) const = default;
  };

  std::string type_name;
  std::string qualified_name;
  std::unordered_map<std::string, std::string> string_attributes;
  std::unordered_map<std::string, Reference> ref_attributes;
  std::unordered_map<std::string, std::unordered_set<Reference>> ref_list_attributes;
};

// AtlasClient is a thin wrapper around http::HTTPClient targeting Atlas REST v2.
// One instance per onTrigger call — mirrors the Java task's fresh-client policy so
// authenticator state is never reused across triggers.
//
// Errors are surfaced as std::expected<T, std::string> where the string is a
// human-readable diagnostic suitable for logging. The client does not throw.
class AtlasClient {
 public:
  struct Config {
    std::vector<std::string> base_urls;                       // e.g. {"http://atlas:21000"}
    std::optional<std::string> username;
    std::optional<std::string> password;
    std::shared_ptr<controllers::SSLContextServiceInterface> ssl_context_service;
    std::shared_ptr<core::logging::Logger> logger;
  };

  explicit AtlasClient(Config config);

  // Ensures both typedef bundles (minifi_* and the nifi_* mirror) are present.
  //
  // Diffs against Atlas first and POSTs only the types that are missing, rather than
  // POSTing a whole bundle and treating the resulting 409 as success. That old approach
  // was wrong in a way that failed silently and permanently: Atlas' typedef POST is
  // all-or-nothing, so a 409 proves only that *at least one* type pre-existed while the
  // rest were never created — after which every entity publish fails with
  // "Type ENTITY ... does not exist".
  //
  // Existing types are left untouched, never updated. For nifi_* that matters: NiFi owns
  // those definitions, and overwriting them would make us the owner of a schema NiFi
  // depends on.
  std::expected<void, std::string> registerTypeDefs();

  // Creates or updates a batch of entities via POST /api/atlas/v2/entity/bulk.
  // Atlas performs upsert semantics keyed by (typeName, qualifiedName).
  std::expected<void, std::string> createOrUpdateEntities(const std::vector<AtlasEntity>& entities);

  // Reads the Asset.name of an existing entity. nullopt means the entity is not there (or
  // carries no name); unexpected means the lookup itself failed.
  //
  // Needed because Atlas requires Asset.name on update as well as create, so every writer
  // of a shared entity has to send a name and the last one wins. Reading the current value
  // first lets us hand it straight back instead of overwriting it.
  std::expected<std::optional<std::string>, std::string> getEntityName(std::string_view type_name, std::string_view qualified_name);

 private:
  // Chooses the primary base URL (first in the list); multi-URL fallback is
  // out of scope for v1. Returns the URL without trailing slash.
  const std::string& primaryBaseUrl() const;

  // Configures a per-request HTTPClient with base URL, auth, and content type set.
  void prepareClient(http::HTTPClient& client, http::HttpRequestMethod method, const std::string& path);

  // True/false if the entity typedef is present; unexpected on transport failure, so a
  // broken connection is not mistaken for "absent" (which would make us POST over it).
  std::expected<bool, std::string> typeDefExists(std::string_view type_name);

  // Registers whichever of bundle_type_names are absent, as a single bundle filtered out
  // of payload. Posting the missing set together lets Atlas resolve forward references
  // between them; dependencies on already-present types resolve against Atlas.
  std::expected<void, std::string> registerMissingTypeDefs(std::string_view payload, std::span<const std::string_view> bundle_type_names);

  Config config_;
};

}  // namespace org::apache::nifi::minifi::extensions::atlas

namespace std {
template<>
struct hash<org::apache::nifi::minifi::extensions::atlas::AtlasEntity::Reference> {
  size_t operator()(const org::apache::nifi::minifi::extensions::atlas::AtlasEntity::Reference& entry_ref) const noexcept {
    return org::apache::nifi::minifi::utils::hash_combine(
        std::hash<std::string_view>{}(entry_ref.type_name),
        std::hash<std::string_view>{}(entry_ref.qualified_name));
  }
};
}  // namespace std
