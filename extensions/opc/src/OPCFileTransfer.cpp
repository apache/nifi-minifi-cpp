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

#include <array>
#include <string>
#include <utility>

namespace org::apache::nifi::minifi::opc {

namespace {

std::optional<UA_UInt32> readMaxByteStringLength(Client& client, const UA_NodeId& file_node_id) {
  if (auto property_node_id = client.findChild(file_node_id, "MaxByteStringLength", 0, UA_NS0ID_HASPROPERTY)) {
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
    auto result = client.findChild(file.file_node_id_, name, 0, UA_NS0ID_HASCOMPONENT);
    if (!result) {
      return std::unexpected{result.error()};
    }
    *method_id = std::move(*result);
  }
  file.max_byte_string_length_ = readMaxByteStringLength(client, file.file_node_id_);
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

}  // namespace org::apache::nifi::minifi::opc
