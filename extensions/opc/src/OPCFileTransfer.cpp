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
#include "OPCFileTransfer.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace org::apache::nifi::minifi::opc {

namespace {

constexpr UA_UInt32 DEFAULT_CHUNK_SIZE = 32 * 1024;

std::optional<UA_UInt32> readMaxByteStringLength(Client& client, const UA_NodeId& file_node_id) {
  if (auto property_node_id = client.findChildByQualifiedName(file_node_id, "MaxByteStringLength", 0, UA_NS0ID_HASPROPERTY)) {
    if (auto value = client.readUInt32Value(*property_node_id); value && *value > 0) {
      return *value;
    }
  }
  if (auto value = client.readUInt32Value(UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_SERVERCAPABILITIES_MAXBYTESTRINGLENGTH)); value && *value > 0) {
    return *value;
  }
  return std::nullopt;
}

}  // namespace

std::expected<RemoteFile, UA_StatusCode> RemoteFile::create(Client& client, NodeId file_node_id) {
  RemoteFile file(client, std::move(file_node_id));
  const std::array<std::pair<std::string_view, NodeId*>, 4> methods{{
      {"Open", &file.open_method_id_},
      {"Close", &file.close_method_id_},
      {"Read", &file.read_method_id_},
      {"Write", &file.write_method_id_}}};
  for (const auto& [name, method_id] : methods) {
    auto result = client.findChildByQualifiedName(file.file_node_id_, name, 0, UA_NS0ID_HASCOMPONENT);
    if (!result) {
      return std::unexpected{result.error()};
    }
    *method_id = std::move(*result);
  }
  // The Length argument of the Read method is an Int32
  file.chunk_size_ = gsl::narrow<UA_Int32>(std::min<UA_UInt32>(readMaxByteStringLength(client, file.file_node_id_).value_or(DEFAULT_CHUNK_SIZE),
      static_cast<UA_UInt32>(std::numeric_limits<UA_Int32>::max())));
  return file;
}

std::expected<RemoteFileHandle, UA_StatusCode> RemoteFile::open(UA_Byte mode) {
  UA_Variant input;
  UA_Variant_setScalar(&input, &mode, &UA_TYPES[UA_TYPES_BYTE]);
  auto output = client_->callMethod(file_node_id_, open_method_id_, std::span(&input, 1));
  if (!output) {
    return std::unexpected{output.error()};
  }
  const auto values = output->values();
  if (values.size() != 1) {
    return std::unexpected{UA_STATUSCODE_BADTYPEMISMATCH};
  }
  const UA_Variant& handle = values.front();
  if (!UA_Variant_hasScalarType(&handle, &UA_TYPES[UA_TYPES_UINT32])) {
    return std::unexpected{UA_STATUSCODE_BADTYPEMISMATCH};
  }
  return RemoteFileHandle(*this, *static_cast<const UA_UInt32*>(handle.data));
}

std::expected<void, UA_StatusCode> RemoteFile::close(RemoteFileHandle& handle) {
  UA_Variant input;
  UA_UInt32 handle_value = handle.get();
  UA_Variant_setScalar(&input, &handle_value, &UA_TYPES[UA_TYPES_UINT32]);
  auto output = client_->callMethod(file_node_id_, close_method_id_, std::span(&input, 1));
  return output ? std::expected<void, UA_StatusCode>{} : std::unexpected{output.error()};
}

std::expected<FileChunk, UA_StatusCode> RemoteFile::read(const RemoteFileHandle& handle, UA_Int32 length) {
  std::array<UA_Variant, 2> inputs{};
  auto& [handle_input, length_input] = inputs;
  UA_UInt32 handle_value = handle.get();
  UA_Variant_setScalar(&handle_input, &handle_value, &UA_TYPES[UA_TYPES_UINT32]);
  UA_Variant_setScalar(&length_input, &length, &UA_TYPES[UA_TYPES_INT32]);
  auto output = client_->callMethod(file_node_id_, read_method_id_, inputs);
  if (!output) {
    return std::unexpected{output.error()};
  }
  const auto values = output->values();
  if (values.size() != 1) {
    return std::unexpected{UA_STATUSCODE_BADTYPEMISMATCH};
  }
  const UA_Variant& data = values.front();
  // Some servers signal the end of the file with an empty variant instead of an empty byte string
  if (UA_Variant_isEmpty(&data)) {
    return {};
  }
  if (!UA_Variant_hasScalarType(&data, &UA_TYPES[UA_TYPES_BYTESTRING])) {
    return std::unexpected{UA_STATUSCODE_BADTYPEMISMATCH};
  }
  const auto* byte_string = static_cast<const UA_ByteString*>(data.data);
  if (byte_string->data == nullptr || byte_string->length == 0) {
    return {};
  }
  return FileChunk(std::move(*output), std::as_bytes(std::span(byte_string->data, byte_string->length)));
}

std::expected<void, UA_StatusCode> RemoteFile::write(const RemoteFileHandle& handle, std::span<const std::byte> data) {
  UA_ByteString byte_string {.length = data.size(), .data = const_cast<UA_Byte*>(reinterpret_cast<const UA_Byte*>(data.data()))};
  std::array<UA_Variant, 2> inputs{};
  auto& [handle_input, data_input] = inputs;
  UA_UInt32 handle_value = handle.get();
  UA_Variant_setScalar(&handle_input, &handle_value, &UA_TYPES[UA_TYPES_UINT32]);
  UA_Variant_setScalar(&data_input, &byte_string, &UA_TYPES[UA_TYPES_BYTESTRING]);
  auto output = client_->callMethod(file_node_id_, write_method_id_, inputs);
  return output ? std::expected<void, UA_StatusCode>{} : std::unexpected{output.error()};
}

std::expected<NodeId, UA_StatusCode> createFileInDirectory(Client& client, const UA_NodeId& directory_node_id, std::string_view file_name) {
  auto method_id = client.findChildByQualifiedName(directory_node_id, "CreateFile", 0, UA_NS0ID_HASCOMPONENT);
  if (!method_id) {
    return std::unexpected{method_id.error()};
  }

  std::string name(file_name);
  UA_String ua_file_name = UA_STRING(name.data());
  UA_Boolean request_file_open = false;
  std::array<UA_Variant, 2> inputs{};
  auto& [file_name_input, request_file_open_input] = inputs;
  UA_Variant_setScalar(&file_name_input, &ua_file_name, &UA_TYPES[UA_TYPES_STRING]);
  UA_Variant_setScalar(&request_file_open_input, &request_file_open, &UA_TYPES[UA_TYPES_BOOLEAN]);
  auto output = client.callMethod(directory_node_id, *method_id, inputs);
  if (!output) {
    return std::unexpected{output.error()};
  }
  const auto values = output->values();
  if (values.empty()) {
    return std::unexpected{UA_STATUSCODE_BADTYPEMISMATCH};
  }
  const UA_Variant& file_node_id = values.front();
  if (!UA_Variant_hasScalarType(&file_node_id, &UA_TYPES[UA_TYPES_NODEID])) {
    return std::unexpected{UA_STATUSCODE_BADTYPEMISMATCH};
  }
  return NodeId::copyOf(*static_cast<const UA_NodeId*>(file_node_id.data));
}

}  // namespace org::apache::nifi::minifi::opc
