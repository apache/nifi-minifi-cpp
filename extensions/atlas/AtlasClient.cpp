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
#include "AtlasClient.h"

#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "AtlasTypeDefs.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::extensions::atlas {

namespace {

// Strip a trailing slash from a base URL, if any, so we can concatenate paths cleanly.
std::string normalizeBaseUrl(std::string base) {
  while (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  return base;
}

rapidjson::Value stringValue(std::string_view s, rapidjson::Document::AllocatorType& alloc) {
  return rapidjson::Value{s.data(), gsl::narrow<rapidjson::SizeType>(s.size()), alloc};
}

// Emits one reference as a rapidjson object. If the referenced (typeName, qualifiedName)
// is another entity in the same bulk POST, we emit { "guid": "<peer temp guid>" }: Atlas
// resolves this against the temp guid of the peer within the batch, letting the whole
// graph be committed in one call. Otherwise we fall back to the uniqueAttributes shape,
// which points at an entity Atlas already knows (or will create implicitly).
rapidjson::Value serializeReferenceObject(
    const AtlasEntity::Reference& ref,
    const std::unordered_map<AtlasEntity::Reference, std::string>& batch_guid_by_key,
    rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value ref_obj{rapidjson::kObjectType};
  if (const auto it = batch_guid_by_key.find({ref.type_name, ref.qualified_name}); it != batch_guid_by_key.end()) {
    ref_obj.AddMember("guid", stringValue(it->second, alloc), alloc);
    return ref_obj;
  }
  ref_obj.AddMember("typeName", stringValue(ref.type_name, alloc), alloc);
  rapidjson::Value unique_attrs{rapidjson::kObjectType};
  unique_attrs.AddMember("qualifiedName", stringValue(ref.qualified_name, alloc), alloc);
  ref_obj.AddMember("uniqueAttributes", unique_attrs, alloc);
  return ref_obj;
}

// Serializes an AtlasEntity into a rapidjson Value shaped like Atlas v2 expects:
//   { "typeName": ..., "attributes": { qualifiedName, name, ...string attrs...,
//     ...ref attrs... }, "guid": "-<unique-negative>" }
// The guid uses a negative integer string as Atlas's convention for "new entity";
// Atlas ignores it when qualifiedName already exists (upsert). The guid is pre-minted
// by the caller so that same-batch references can point at it via serializeReferenceObject.
rapidjson::Value serializeEntity(
    const AtlasEntity& entity,
    std::string_view entity_guid,
    const std::unordered_map<AtlasEntity::Reference, std::string>& batch_guid_by_key,
    rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value obj{rapidjson::kObjectType};
  obj.AddMember("typeName", stringValue(entity.type_name, alloc), alloc);

  rapidjson::Value attrs{rapidjson::kObjectType};
  attrs.AddMember("qualifiedName", stringValue(entity.qualified_name, alloc), alloc);
  for (const auto& [key, value] : entity.string_attributes) {
    attrs.AddMember(stringValue(key, alloc), stringValue(value, alloc), alloc);
  }
  for (const auto& [key, ref] : entity.ref_attributes) {
    attrs.AddMember(stringValue(key, alloc), serializeReferenceObject(ref, batch_guid_by_key, alloc), alloc);
  }
  for (const auto& [key, refs] : entity.ref_list_attributes) {
    rapidjson::Value list{rapidjson::kArrayType};
    for (const auto& ref : refs) {
      list.PushBack(serializeReferenceObject(ref, batch_guid_by_key, alloc), alloc);
    }
    attrs.AddMember(stringValue(key, alloc), list, alloc);
  }
  obj.AddMember("attributes", attrs, alloc);

  obj.AddMember("guid", stringValue(entity_guid, alloc), alloc);
  return obj;
}

std::string responseBodyAsString(http::HTTPClient& client) {
  const auto& body = client.getResponseBody();
  return {body.data(), body.size()};
}

}  // namespace

AtlasClient::AtlasClient(Config config) : config_(std::move(config)) {}

const std::string& AtlasClient::primaryBaseUrl() const {
  static const std::string kEmpty;
  return config_.base_urls.empty() ? kEmpty : config_.base_urls.front();
}

void AtlasClient::prepareClient(http::HTTPClient& client, http::HttpRequestMethod method, const std::string& path) {
  const auto url = normalizeBaseUrl(primaryBaseUrl()) + path;
  client.initialize(method, url, config_.ssl_context_service);
  client.setContentType("application/json");
  client.setRequestHeader("Accept", "application/json");
  if (config_.username && config_.password) {
    client.setBasicAuth(*config_.username, *config_.password);
  }
}

std::expected<bool, std::string> AtlasClient::typeDefExists(std::string_view type_name) {
  http::HTTPClient client;
  prepareClient(client, http::HttpRequestMethod::Get, "/api/atlas/v2/types/typedef/name/" + std::string{type_name});
  if (!client.submit()) {
    std::ostringstream msg;
    msg << "Atlas typedef lookup for '" << type_name << "' failed: transport error";
    return std::unexpected{msg.str()};
  }
  const auto code = client.getResponseCode();
  if (code == 200) {
    return true;
  }
  if (code == 404) {
    return false;
  }
  std::ostringstream msg;
  msg << "Atlas typedef lookup for '" << type_name << "' returned HTTP " << code << ": " << responseBodyAsString(client);
  return std::unexpected{msg.str()};
}

std::expected<void, std::string> AtlasClient::registerMissingTypeDefs(std::string_view payload, std::span<const std::string_view> bundle_type_names) {
  std::unordered_set<std::string_view> missing;
  for (const auto& type_name : bundle_type_names) {
    auto exists = typeDefExists(type_name);
    if (!exists) {
      return std::unexpected{exists.error()};
    }
    if (!*exists) {
      missing.insert(type_name);
    }
  }
  if (missing.empty()) {
    return {};
  }

  rapidjson::Document doc;
  if (doc.Parse(payload.data(), payload.size()).HasParseError() || !doc.IsObject() || !doc.HasMember("entityDefs") || !doc["entityDefs"].IsArray()) {
    return std::unexpected{"Atlas typedef payload is not valid JSON with an entityDefs array"};
  }

  // Keep only the missing definitions. The retained set plus what Atlas already has
  // covers every dependency, so forward references within the bundle still resolve.
  rapidjson::Document filtered{rapidjson::kObjectType};
  auto& alloc = filtered.GetAllocator();
  rapidjson::Value entity_defs{rapidjson::kArrayType};
  for (const auto& entity_def : doc["entityDefs"].GetArray()) {
    if (!entity_def.IsObject() || !entity_def.HasMember("name") || !entity_def["name"].IsString()) {
      return std::unexpected{"Atlas typedef payload has an entityDef without a string name"};
    }
    if (missing.contains(std::string_view{entity_def["name"].GetString(), entity_def["name"].GetStringLength()})) {
      entity_defs.PushBack(rapidjson::Value{}.CopyFrom(entity_def, alloc), alloc);
    }
  }
  filtered.AddMember("entityDefs", std::move(entity_defs), alloc);

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer{buffer};
  filtered.Accept(writer);

  http::HTTPClient client;
  prepareClient(client, http::HttpRequestMethod::Post, "/api/atlas/v2/types/typedefs");
  client.setPostFields(std::string{buffer.GetString(), buffer.GetSize()});
  if (!client.submit()) {
    return std::unexpected{"Atlas typedef POST failed: transport error"};
  }
  const auto code = client.getResponseCode();
  if (code == 200 || code == 204) {
    return {};
  }
  // A 409 here means someone created one of these between our lookup and our POST.
  // Report it so the caller retries: the POST is all-or-nothing, so the rest of the
  // missing types were not created and treating this as success would leave the schema
  // incomplete with no further attempt to repair it.
  std::ostringstream msg;
  msg << "Atlas typedef POST returned HTTP " << code << ": " << responseBodyAsString(client);
  return std::unexpected{msg.str()};
}

std::expected<std::optional<std::string>, std::string> AtlasClient::getEntityName(std::string_view type_name, std::string_view qualified_name) {
  http::HTTPClient client;
  // escape() only needs the curl handle, which exists from construction, so it is usable
  // before the request is configured. The qualifiedName travels as a query value and can
  // contain '@' and whatever the namespace carries.
  const auto path = utils::string::join_pack("/api/atlas/v2/entity/uniqueAttribute/type/", type_name,
      "?attr:qualifiedName=", client.escape(std::string{qualified_name}));
  prepareClient(client, http::HttpRequestMethod::Get, path);
  if (!client.submit()) {
    return std::unexpected{fmt::format("Atlas entity lookup for '{}' failed: transport error", qualified_name)};
  }
  const auto code = client.getResponseCode();
  if (code == 404) {
    return std::nullopt;
  }
  if (code != 200) {
    return std::unexpected{fmt::format("Atlas entity lookup for '{}' returned HTTP {}: {}", qualified_name, code, responseBodyAsString(client))};
  }
  const auto body = responseBodyAsString(client);
  rapidjson::Document doc;
  if (doc.Parse(body.c_str()).HasParseError() || !doc.IsObject()) {
    return std::unexpected{fmt::format("Atlas entity lookup for '{}' returned unparseable JSON", qualified_name)};
  }
  if (!doc.HasMember("entity") || !doc["entity"].IsObject()) {
    return std::nullopt;
  }
  const auto& entity = doc["entity"];
  if (!entity.HasMember("attributes") || !entity["attributes"].IsObject()) {
    return std::nullopt;
  }
  const auto& attributes = entity["attributes"];
  if (!attributes.HasMember("name") || !attributes["name"].IsString()) {
    return std::nullopt;
  }
  return std::string{attributes["name"].GetString(), attributes["name"].GetStringLength()};
}

std::expected<void, std::string> AtlasClient::registerTypeDefs() {
  // Registered as two independent calls: a conflict or failure in one bundle must not
  // prevent the other from being created.
  if (auto result = registerMissingTypeDefs(MINIFI_TYPEDEFS_PAYLOAD, MINIFI_TYPE_NAMES); !result) {
    return std::unexpected{"minifi typedefs: " + result.error()};
  }
  if (auto result = registerMissingTypeDefs(NIFI_TYPEDEFS_PAYLOAD, NIFI_TYPE_NAMES); !result) {
    return std::unexpected{"nifi typedefs: " + result.error()};
  }
  return {};
}

std::expected<void, std::string> AtlasClient::createOrUpdateEntities(const std::vector<AtlasEntity>& entities) {
  if (entities.empty()) {
    return {};
  }
  rapidjson::Document doc{rapidjson::kObjectType};
  auto& alloc = doc.GetAllocator();

  // First pass: mint one temp guid per entity in this batch and index them by
  // (typeName, qualifiedName) so that references to peers in the same batch can
  // be emitted as { "guid": "-N" } — the only shape Atlas resolves against
  // not-yet-persisted peers within a single bulk POST.
  std::vector<std::string> entity_guids;
  entity_guids.reserve(entities.size());
  std::unordered_map<AtlasEntity::Reference, std::string> batch_guid_by_key;
  batch_guid_by_key.reserve(entities.size());
  for (std::size_t i = 0; i < entities.size(); ++i) {
    entity_guids.push_back(std::to_string(-static_cast<int64_t>(i + 1)));
    batch_guid_by_key.emplace(
        AtlasEntity::Reference{entities[i].type_name, entities[i].qualified_name},
        entity_guids.back());
  }

  rapidjson::Value entities_arr{rapidjson::kArrayType};
  for (std::size_t i = 0; i < entities.size(); ++i) {
    entities_arr.PushBack(serializeEntity(entities[i], entity_guids[i], batch_guid_by_key, alloc), alloc);
  }
  doc.AddMember("entities", entities_arr, alloc);

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer{buffer};
  doc.Accept(writer);

  std::string update_str{buffer.GetString(), buffer.GetSize()};

  http::HTTPClient client;
  prepareClient(client, http::HttpRequestMethod::Post, "/api/atlas/v2/entity/bulk");
  client.setPostFields(update_str);
  if (!client.submit()) {
    return std::unexpected{"Atlas entity bulk POST failed: transport error"};
  }
  const auto code = client.getResponseCode();
  if (code == 200 || code == 204) {
    return {};
  }
  return std::unexpected{fmt::format("Atlas entity bulk POST returned HTTP {}: {}", code, responseBodyAsString(client))};
}

}  // namespace org::apache::nifi::minifi::extensions::atlas
