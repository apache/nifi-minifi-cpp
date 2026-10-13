// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "OPCCommon.h"

namespace org::apache::nifi::minifi::opc {

// Open mode bits of the FileType Open method (OPC UA Part 20, 4.2.2)
inline constexpr UA_Byte FILE_MODE_READ = 0x01;
inline constexpr UA_Byte FILE_MODE_WRITE = 0x02;
inline constexpr UA_Byte FILE_MODE_ERASE_EXISTING = 0x04;
inline constexpr UA_Byte FILE_MODE_APPEND = 0x08;

class RemoteFileHandle;

class FileChunk {
 public:
  FileChunk() = default;
  FileChunk(MethodOutput output, std::span<const std::byte> data) : output_(std::move(output)), data_(data) {
  }

  [[nodiscard]] std::span<const std::byte> data() const noexcept {
    return data_;
  }

 private:
  MethodOutput output_;
  std::span<const std::byte> data_;
};

class RemoteFile {
 public:
  static std::expected<RemoteFile, UA_StatusCode> create(Client& client, NodeId file_node_id);

  std::expected<RemoteFileHandle, UA_StatusCode> open(UA_Byte mode);
  std::expected<FileChunk, UA_StatusCode> read(const RemoteFileHandle& handle, UA_Int32 length);

  [[nodiscard]] const NodeId& nodeId() const noexcept {
    return file_node_id_;
  }

  // The MaxByteStringLength property of the file, or the ServerCapabilities MaxByteStringLength if the file does not specify one (OPC UA Part 20, 4.2.1)
  [[nodiscard]] std::optional<UA_UInt32> maxByteStringLength() const noexcept {
    return max_byte_string_length_;
  }

 private:
  RemoteFile(Client& client, NodeId file_node_id) : client_(&client), file_node_id_(std::move(file_node_id)) {
  }

  friend class RemoteFileHandle;
  std::expected<void, UA_StatusCode> close(RemoteFileHandle& handle);

  Client* client_;
  NodeId file_node_id_;
  NodeId open_method_id_;
  NodeId close_method_id_;
  NodeId read_method_id_;
  NodeId write_method_id_;
  std::optional<UA_UInt32> max_byte_string_length_;
};

class RemoteFileHandle {
 public:
  RemoteFileHandle(RemoteFile& file, UA_UInt32 handle) : file_(file), handle_(handle) {
  }
  RemoteFileHandle(const RemoteFileHandle&) = delete;
  RemoteFileHandle& operator=(const RemoteFileHandle&) = delete;
  RemoteFileHandle(RemoteFileHandle&& other) noexcept : file_(other.file_), handle_(other.handle_), open_(std::exchange(other.open_, false)) {
  }
  RemoteFileHandle& operator=(RemoteFileHandle&&) = delete;
  ~RemoteFileHandle() {
    if (open_) {
      std::ignore = file_.close(*this);
    }
  }

  std::expected<void, UA_StatusCode> close() {
    if (!open_) {
      return {};
    }
    open_ = false;
    return file_.close(*this);
  }

  [[nodiscard]] UA_UInt32 get() const noexcept {
    return handle_;
  }

 private:
  RemoteFile& file_;
  UA_UInt32 handle_;
  bool open_ = true;
};

}  // namespace org::apache::nifi::minifi::opc
