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
#include <string>
#include <vector>

#include "http/HTTPClient.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/controllers/SSLContextServiceInterface.h"

namespace org::apache::nifi::minifi::extensions::atlas {

// AtlasEntity is the JSON-shaped payload passed to Atlas's bulk-entity endpoint.
// One instance corresponds to one Atlas entity (nifi_flow, nifi_flow_path, kafka_topic,
// etc.). type_name is the Atlas type; qualified_name is the entity's Atlas-wide unique
// key; attributes carries the remaining typed fields. References to other entities
// (e.g. "inputs": [{typeName, uniqueAttributes.qualifiedName}]) are represented via
// nested Reference objects.
struct AtlasEntity {
  struct Reference {
    std::string type_name;
    std::string qualified_name;
  };

  std::string type_name;
  std::string qualified_name;
  std::string display_name;                                  // maps to "name" attribute
  std::vector<std::pair<std::string, std::string>> string_attributes;
  std::vector<std::pair<std::string, Reference>> ref_attributes;
  std::vector<std::pair<std::string, std::vector<Reference>>> ref_list_attributes;
};

// Minimal typed handle to an Atlas entity fetched via getEntityByUniqueAttribute.
struct AtlasEntityHandle {
  std::string guid;
  std::string type_name;
  std::string qualified_name;
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

  // Registers the six NiFi typedefs (nifi_component, nifi_flow, nifi_flow_path,
  // nifi_queue, nifi_input_port, nifi_output_port). Idempotent on the Atlas side —
  // if the types already exist Atlas returns 409, which this method treats as success.
  std::expected<void, std::string> registerNiFiTypeDefs();

  // Creates or updates a batch of entities via POST /api/atlas/v2/entity/bulk.
  // Atlas performs upsert semantics keyed by (typeName, qualifiedName).
  std::expected<void, std::string> createOrUpdateEntities(const std::vector<AtlasEntity>& entities);

  // Looks up an entity by its typed unique attribute (qualifiedName). Returns a
  // handle if the entity exists, std::nullopt if Atlas returns 404, error otherwise.
  std::expected<std::optional<AtlasEntityHandle>, std::string> getEntityByUniqueAttribute(
      const std::string& type_name, const std::string& qualified_name);

 private:
  // Chooses the primary base URL (first in the list); multi-URL fallback is
  // out of scope for v1. Returns the URL without trailing slash.
  const std::string& primaryBaseUrl() const;

  // Configures a per-request HTTPClient with base URL, auth, and content type set.
  void prepareClient(http::HTTPClient& client, http::HttpRequestMethod method, const std::string& path);

  Config config_;
};

}  // namespace org::apache::nifi::minifi::extensions::atlas
