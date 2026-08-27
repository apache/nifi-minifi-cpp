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

#include <sstream>
#include <string_view>
#include <utility>

#include "AtlasTypeDefs.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace org::apache::nifi::minifi::extensions::atlas {

namespace {

// URL-encode a qualifiedName so it can travel as a query parameter to
// GET /api/atlas/v2/entity/uniqueAttribute/type/<T>?attr:qualifiedName=<x>@<ns>
std::string urlEncode(std::string_view input) {
  std::string result;
  result.reserve(input.size());
  const auto is_unreserved = [](char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
        || c == '-' || c == '_' || c == '.' || c == '~';
  };
  static constexpr char kHex[] = "0123456789ABCDEF";
  for (unsigned char c : input) {
    if (is_unreserved(static_cast<char>(c))) {
      result.push_back(static_cast<char>(c));
    } else {
      result.push_back('%');
      result.push_back(kHex[c >> 4]);
      result.push_back(kHex[c & 0x0F]);
    }
  }
  return result;
}

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

// Serializes an AtlasEntity into a rapidjson Value shaped like Atlas v2 expects:
//   { "typeName": ..., "attributes": { qualifiedName, name, ...string attrs...,
//     ...ref attrs... }, "guid": "-<unique-negative>" }
// The guid uses a negative integer string as Atlas's convention for "new entity";
// Atlas ignores it when qualifiedName already exists (upsert). We use a running
// counter passed in by the caller so all entities in one bulk POST get distinct
// temporary guids.
rapidjson::Value serializeEntity(const AtlasEntity& entity, int64_t& tmp_guid_seq, rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value obj{rapidjson::kObjectType};
  obj.AddMember("typeName", stringValue(entity.type_name, alloc), alloc);

  rapidjson::Value attrs{rapidjson::kObjectType};
  attrs.AddMember("qualifiedName", stringValue(entity.qualified_name, alloc), alloc);
  if (!entity.display_name.empty()) {
    attrs.AddMember("name", stringValue(entity.display_name, alloc), alloc);
  }
  for (const auto& [key, value] : entity.string_attributes) {
    attrs.AddMember(stringValue(key, alloc), stringValue(value, alloc), alloc);
  }
  for (const auto& [key, ref] : entity.ref_attributes) {
    rapidjson::Value ref_obj{rapidjson::kObjectType};
    ref_obj.AddMember("typeName", stringValue(ref.type_name, alloc), alloc);
    rapidjson::Value unique{rapidjson::kObjectType};
    unique.AddMember("qualifiedName", stringValue(ref.qualified_name, alloc), alloc);
    ref_obj.AddMember("uniqueAttributes", unique, alloc);
    attrs.AddMember(stringValue(key, alloc), ref_obj, alloc);
  }
  for (const auto& [key, refs] : entity.ref_list_attributes) {
    rapidjson::Value list{rapidjson::kArrayType};
    for (const auto& ref : refs) {
      rapidjson::Value ref_obj{rapidjson::kObjectType};
      ref_obj.AddMember("typeName", stringValue(ref.type_name, alloc), alloc);
      rapidjson::Value unique{rapidjson::kObjectType};
      unique.AddMember("qualifiedName", stringValue(ref.qualified_name, alloc), alloc);
      ref_obj.AddMember("uniqueAttributes", unique, alloc);
      list.PushBack(ref_obj, alloc);
    }
    attrs.AddMember(stringValue(key, alloc), list, alloc);
  }
  obj.AddMember("attributes", attrs, alloc);

  const auto guid = std::to_string(--tmp_guid_seq);  // -1, -2, -3, ...
  obj.AddMember("guid", stringValue(guid, alloc), alloc);
  return obj;
}

std::string responseBodyAsString(http::HTTPClient& client) {
  const auto& body = client.getResponseBody();
  return std::string(body.data(), body.size());
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

std::expected<void, std::string> AtlasClient::registerNiFiTypeDefs() {
  http::HTTPClient client;
  prepareClient(client, http::HttpRequestMethod::Post, "/api/atlas/v2/types/typedefs");
  client.setPostFields(std::string{kNiFiTypeDefsPayload});
  if (!client.submit()) {
    return std::unexpected{"Atlas typedef POST failed: transport error"};
  }
  const auto code = client.getResponseCode();
  // 200/204: created. 409: types already exist (Atlas rejects duplicate definitions),
  // which we treat as success — types being present is exactly the goal.
  if (code == 200 || code == 204 || code == 409) {
    return {};
  }
  std::ostringstream msg;
  msg << "Atlas typedef POST returned HTTP " << code << ": " << responseBodyAsString(client);
  return std::unexpected{msg.str()};
}

std::expected<void, std::string> AtlasClient::createOrUpdateEntities(const std::vector<AtlasEntity>& entities) {
  if (entities.empty()) {
    return {};
  }
  rapidjson::Document doc{rapidjson::kObjectType};
  auto& alloc = doc.GetAllocator();
  rapidjson::Value entities_arr{rapidjson::kArrayType};
  int64_t tmp_guid_seq = 0;
  for (const auto& entity : entities) {
    entities_arr.PushBack(serializeEntity(entity, tmp_guid_seq, alloc), alloc);
  }
  doc.AddMember("entities", entities_arr, alloc);

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer{buffer};
  doc.Accept(writer);

  http::HTTPClient client;
  prepareClient(client, http::HttpRequestMethod::Post, "/api/atlas/v2/entity/bulk");
  client.setPostFields(std::string{buffer.GetString(), buffer.GetSize()});
  if (!client.submit()) {
    return std::unexpected{"Atlas entity bulk POST failed: transport error"};
  }
  const auto code = client.getResponseCode();
  if (code == 200 || code == 204) {
    return {};
  }
  std::ostringstream msg;
  msg << "Atlas entity bulk POST returned HTTP " << code << ": " << responseBodyAsString(client);
  return std::unexpected{msg.str()};
}

std::expected<std::optional<AtlasEntityHandle>, std::string> AtlasClient::getEntityByUniqueAttribute(
    const std::string& type_name, const std::string& qualified_name) {
  http::HTTPClient client;
  const auto path = "/api/atlas/v2/entity/uniqueAttribute/type/" + urlEncode(type_name)
      + "?attr:qualifiedName=" + urlEncode(qualified_name);
  prepareClient(client, http::HttpRequestMethod::Get, path);
  if (!client.submit()) {
    return std::unexpected{"Atlas entity lookup failed: transport error"};
  }
  const auto code = client.getResponseCode();
  if (code == 404) {
    return std::optional<AtlasEntityHandle>{};
  }
  if (code != 200) {
    std::ostringstream msg;
    msg << "Atlas entity lookup returned HTTP " << code << ": " << responseBodyAsString(client);
    return std::unexpected{msg.str()};
  }

  const auto body = responseBodyAsString(client);
  rapidjson::Document response;
  if (response.Parse(body.c_str()).HasParseError() || !response.IsObject()) {
    return std::unexpected{"Atlas entity lookup returned non-JSON body"};
  }
  const auto entity_it = response.FindMember("entity");
  if (entity_it == response.MemberEnd() || !entity_it->value.IsObject()) {
    return std::unexpected{"Atlas entity lookup response missing 'entity' object"};
  }
  const auto& entity = entity_it->value;
  AtlasEntityHandle handle;
  if (auto it = entity.FindMember("guid"); it != entity.MemberEnd() && it->value.IsString()) {
    handle.guid = it->value.GetString();
  }
  if (auto it = entity.FindMember("typeName"); it != entity.MemberEnd() && it->value.IsString()) {
    handle.type_name = it->value.GetString();
  }
  if (auto it = entity.FindMember("attributes"); it != entity.MemberEnd() && it->value.IsObject()) {
    if (auto qn = it->value.FindMember("qualifiedName"); qn != it->value.MemberEnd() && qn->value.IsString()) {
      handle.qualified_name = qn->value.GetString();
    }
  }
  return std::optional<AtlasEntityHandle>{std::move(handle)};
}

}  // namespace org::apache::nifi::minifi::extensions::atlas
