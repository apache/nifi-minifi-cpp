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

#include "core/state/Value.h"
#include <openssl/evp.h>
#include <utility>
#include <string>
#include "rapidjson/prettywriter.h"

namespace org::apache::nifi::minifi::state::response {

const std::type_index Value::UINT64_TYPE = std::type_index(typeid(uint64_t));
const std::type_index Value::INT64_TYPE = std::type_index(typeid(int64_t));
const std::type_index Value::UINT32_TYPE = std::type_index(typeid(uint32_t));
const std::type_index Value::INT_TYPE = std::type_index(typeid(int));
const std::type_index Value::BOOL_TYPE = std::type_index(typeid(bool));
const std::type_index Value::DOUBLE_TYPE = std::type_index(typeid(double));
const std::type_index Value::STRING_TYPE = std::type_index(typeid(std::string));

void hashNode(const SerializedResponseNode& node, EVP_MD_CTX& ctx) {
  EVP_DigestUpdate(&ctx, node.name.c_str(), node.name.length());
  const auto valueStr = node.value.to_string();
  EVP_DigestUpdate(&ctx, valueStr.c_str(), valueStr.length());
  EVP_DigestUpdate(&ctx, &node.array, sizeof(node.array));
  EVP_DigestUpdate(&ctx, &node.collapsible, sizeof(node.collapsible));
  for (const auto& child : node.children) {
    hashNode(child, ctx);
  }
}

std::string hashResponseNodes(const std::vector<SerializedResponseNode>& nodes) {
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  const auto guard = gsl::finally([&ctx]() {
    EVP_MD_CTX_free(ctx);
  });
  EVP_DigestInit_ex(ctx, EVP_sha512(), nullptr);
  for (const auto& node : nodes) {
    hashNode(node, *ctx);
  }
  std::array<std::byte, EVP_MAX_MD_SIZE> digest{};
  EVP_DigestFinal_ex(ctx, reinterpret_cast<unsigned char*>(digest.data()), nullptr);
  return utils::string::to_hex(digest, true /*uppercase*/);
}

rapidjson::Value nodeToJson(const SerializedResponseNode& node, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>& alloc) {
  if (node.value.empty()) {
    if (node.array) {
      rapidjson::Value result(rapidjson::kArrayType);
      for (const auto& elem: node.children) {
        result.PushBack(nodeToJson(elem, alloc), alloc);
      }
      return result;
    } else {
      rapidjson::Value result(rapidjson::kObjectType);
      for (const auto& elem: node.children) {
        result.AddMember(rapidjson::Value(elem.name.c_str(), alloc), nodeToJson(elem, alloc), alloc);
      }
      return result;
    }
  } else {
    return {node.value.to_string().c_str(), alloc};
  }
}

template<typename Writer>
[[nodiscard]] std::string to_string(const SerializedResponseNode& node) {
  rapidjson::Document doc;
  doc.SetObject();
  doc.AddMember(rapidjson::Value(node.name.c_str(), doc.GetAllocator()), nodeToJson(node, doc.GetAllocator()), doc.GetAllocator());
  rapidjson::StringBuffer buf;
  Writer writer{buf};
  doc.Accept(writer);
  return buf.GetString();
}

std::string SerializedResponseNode::to_string() const {
  return to_string<rapidjson::Writer<rapidjson::StringBuffer>>(*this);
}

std::string SerializedResponseNode::to_pretty_string() const {
  return to_string<rapidjson::PrettyWriter<rapidjson::StringBuffer>>(*this);
}
}  // namespace org::apache::nifi::minifi::state::response

