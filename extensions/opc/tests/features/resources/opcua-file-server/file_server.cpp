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

/*
 * OPC UA test server exposing the files of a directory through the FileType and FileDirectoryType object types.
 *
 * The "Files" FileDirectoryType node (ns=1;s=Files) contains a FileType node (ns=1;s=<file name>) for every regular file
 * found in the directory at startup. The files are read-only.
 * The FileType methods only define an interface, so their behavior is implemented here.
 */

#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t MAX_OPEN_HANDLES = 16;

// Open mode bits of the FileType Open method (OPC UA Part 20, 4.2.2)
constexpr UA_Byte FILE_MODE_READ = 0x01;
constexpr UA_Byte FILE_MODE_WRITE = 0x02;

struct OpenHandle {
  UA_Byte mode;
  std::ifstream file;
};

struct FileContext {
  std::filesystem::path path;
  std::unordered_map<UA_UInt32, OpenHandle> handles;
};

/** open62541 takes non-const char pointers for strings it only reads */
char* mutableChars(const char* chars) {
  return const_cast<char*>(chars);
}

const UA_Logger* logger = nullptr;
std::vector<std::unique_ptr<FileContext>> file_contexts;
UA_UInt32 next_handle_id = 1;
const UA_NodeId FILES_NODE_ID = UA_NODEID_STRING(1, mutableChars("Files"));

template<typename... Args>
void logInfo(std::format_string<Args...> fmt, Args&&... args) {
  UA_LOG_INFO(logger, UA_LOGCATEGORY_USERLAND, "%s", std::format(fmt, std::forward<Args>(args)...).c_str());
}

template<typename... Args>
void logError(std::format_string<Args...> fmt, Args&&... args) {
  UA_LOG_ERROR(logger, UA_LOGCATEGORY_USERLAND, "%s", std::format(fmt, std::forward<Args>(args)...).c_str());
}

/** The server validates the input arguments against the InputArguments of the method, so their types are known */
template<typename T>
T scalarInput(const UA_Variant& variant) {
  return *static_cast<const T*>(variant.data);
}

OpenHandle* findHandle(FileContext& context, UA_UInt32 handle_id) {
  const auto it = context.handles.find(handle_id);
  return it != context.handles.end() ? &it->second : nullptr;
}

UA_StatusCode openHandle(FileContext& context, UA_Byte mode, UA_UInt32& handle_id) {
  if (mode & FILE_MODE_WRITE) {
    return UA_STATUSCODE_BADNOTWRITABLE;
  }
  if (!(mode & FILE_MODE_READ)) {
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  }
  if (context.handles.size() >= MAX_OPEN_HANDLES) {
    return UA_STATUSCODE_BADTOOMANYOPERATIONS;
  }

  std::ifstream file(context.path, std::ios::binary);
  if (!file) {
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  handle_id = next_handle_id++;
  context.handles.emplace(handle_id, OpenHandle{mode, std::move(file)});
  return UA_STATUSCODE_GOOD;
}

UA_StatusCode openMethod(FileContext& context, std::span<const UA_Variant> input, UA_Variant* output) {
  const auto mode = scalarInput<UA_Byte>(input[0]);
  UA_UInt32 handle_id = 0;
  const UA_StatusCode status = openHandle(context, mode, handle_id);
  logInfo("Open {} with mode {}: {}, handle {}", context.path.string(), mode, UA_StatusCode_name(status), handle_id);
  if (status != UA_STATUSCODE_GOOD) {
    return status;
  }
  return UA_Variant_setScalarCopy(&output[0], &handle_id, &UA_TYPES[UA_TYPES_UINT32]);
}

UA_StatusCode closeMethod(FileContext& context, std::span<const UA_Variant> input, UA_Variant*) {
  const auto it = context.handles.find(scalarInput<UA_UInt32>(input[0]));
  if (it == context.handles.end()) {
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  }
  it->second.file.close();
  const bool failed = it->second.file.fail();
  logInfo("Close {} handle {}", context.path.string(), it->first);
  context.handles.erase(it);
  return failed ? UA_STATUSCODE_BADINTERNALERROR : UA_STATUSCODE_GOOD;
}

UA_StatusCode readMethod(FileContext& context, std::span<const UA_Variant> input, UA_Variant* output) {
  OpenHandle* handle = findHandle(context, scalarInput<UA_UInt32>(input[0]));
  const auto length = scalarInput<UA_Int32>(input[1]);
  if (!handle || !(handle->mode & FILE_MODE_READ)) {
    return UA_STATUSCODE_BADINVALIDSTATE;
  }
  if (length < 0) {
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  }

  std::vector<UA_Byte> buffer(static_cast<std::size_t>(length));
  handle->file.read(reinterpret_cast<char*>(buffer.data()), length);
  if (handle->file.bad()) {
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  buffer.resize(static_cast<std::size_t>(handle->file.gcount()));
  // Reading past the end of the file sets failbit, which would make the following tellg and read calls fail
  handle->file.clear();

  logInfo("Read {} bytes from {}", buffer.size(), context.path.string());
  const UA_ByteString data{buffer.size(), buffer.data()};
  return UA_Variant_setScalarCopy(&output[0], &data, &UA_TYPES[UA_TYPES_BYTESTRING]);
}

UA_StatusCode getPositionMethod(FileContext& context, std::span<const UA_Variant> input, UA_Variant* output) {
  OpenHandle* handle = findHandle(context, scalarInput<UA_UInt32>(input[0]));
  if (!handle) {
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  }
  const std::streamoff position = handle->file.tellg();
  if (position < 0) {
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  const auto result = static_cast<UA_UInt64>(position);
  return UA_Variant_setScalarCopy(&output[0], &result, &UA_TYPES[UA_TYPES_UINT64]);
}

UA_StatusCode setPositionMethod(FileContext& context, std::span<const UA_Variant> input, UA_Variant*) {
  OpenHandle* handle = findHandle(context, scalarInput<UA_UInt32>(input[0]));
  const auto position = scalarInput<UA_UInt64>(input[1]);
  if (!handle) {
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  }
  if (std::cmp_greater(position, std::numeric_limits<std::streamoff>::max()) || !handle->file.seekg(static_cast<std::streamoff>(position))) {
    handle->file.clear();
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  }
  return UA_STATUSCODE_GOOD;
}

using FileMethod = UA_StatusCode (*)(FileContext& context, std::span<const UA_Variant> input, UA_Variant* output);

/** Adapts a FileMethod to the open62541 method callback signature, the object context being the FileContext of the called file node */
template<FileMethod Method>
UA_StatusCode fileMethodCallback(UA_Server*, const UA_NodeId*, void*, const UA_NodeId*, void*, const UA_NodeId*, void* object_context,
    size_t input_size, const UA_Variant* input, size_t, UA_Variant* output) {
  auto* context = static_cast<FileContext*>(object_context);
  if (!context) {
    return UA_STATUSCODE_BADNOTFOUND;
  }
  return Method(*context, std::span(input, input_size), output);
}

UA_StatusCode addFileNode(UA_Server* server, const std::filesystem::path& path) {
  auto context = std::make_unique<FileContext>(path);
  const std::string name = path.filename().string();

  UA_ObjectAttributes attr = UA_ObjectAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT(mutableChars("en-US"), mutableChars(name.c_str()));
  const UA_StatusCode status = UA_Server_addObjectNode(server,
      UA_NODEID_STRING(1, mutableChars(name.c_str())),
      FILES_NODE_ID,
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, mutableChars(name.c_str())),
      UA_NODEID_NUMERIC(0, UA_NS0ID_FILETYPE),
      attr,
      context.get(),
      nullptr);
  if (status != UA_STATUSCODE_GOOD) {
    return status;
  }
  logInfo("Added file node for {}", path.string());
  file_contexts.push_back(std::move(context));
  return UA_STATUSCODE_GOOD;
}

UA_StatusCode addExistingFiles(UA_Server* server, const std::filesystem::path& data_dir) {
  std::error_code error;
  const std::filesystem::directory_iterator dir(data_dir, error);
  if (error) {
    logError("Cannot open directory {}: {}", data_dir.string(), error.message());
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  for (const auto& entry : dir) {
    std::error_code stat_error;
    if (!entry.is_regular_file(stat_error)) {
      continue;
    }
    if (const UA_StatusCode status = addFileNode(server, entry.path()); status != UA_STATUSCODE_GOOD) {
      return status;
    }
  }
  return UA_STATUSCODE_GOOD;
}

UA_StatusCode setupAddressSpace(UA_Server* server, const std::filesystem::path& data_dir) {
  UA_ObjectAttributes attr = UA_ObjectAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT(mutableChars("en-US"), mutableChars("Files"));
  UA_StatusCode status = UA_Server_addObjectNode(server,
      FILES_NODE_ID,
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, mutableChars("Files")),
      UA_NODEID_NUMERIC(0, UA_NS0ID_FILEDIRECTORYTYPE),
      attr,
      nullptr,
      nullptr);
  if (status != UA_STATUSCODE_GOOD) {
    return status;
  }

  // Instances reference the methods of their type definition, so the callbacks are set on the methods of the types
  constexpr std::array<std::pair<UA_UInt32, UA_MethodCallback>, 5> methods{{
      {UA_NS0ID_FILETYPE_OPEN, fileMethodCallback<openMethod>},
      {UA_NS0ID_FILETYPE_CLOSE, fileMethodCallback<closeMethod>},
      {UA_NS0ID_FILETYPE_READ, fileMethodCallback<readMethod>},
      {UA_NS0ID_FILETYPE_GETPOSITION, fileMethodCallback<getPositionMethod>},
      {UA_NS0ID_FILETYPE_SETPOSITION, fileMethodCallback<setPositionMethod>},
  }};
  for (const auto& [method_id, callback] : methods) {
    status = UA_Server_setMethodNodeCallback(server, UA_NODEID_NUMERIC(0, method_id), callback);
    if (status != UA_STATUSCODE_GOOD) {
      return status;
    }
  }

  return addExistingFiles(server, data_dir);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::println(stderr, "Usage: {} <data directory>", argv[0]);
    return EXIT_FAILURE;
  }

  const std::unique_ptr<UA_Server, decltype(&UA_Server_delete)> server(UA_Server_new(), &UA_Server_delete);
  logger = UA_Server_getConfig(server.get())->logging;

  UA_StatusCode status = setupAddressSpace(server.get(), argv[1]);
  if (status == UA_STATUSCODE_GOOD) {
    status = UA_Server_runUntilInterrupt(server.get());
  } else {
    logError("Failed to set up the address space: {}", UA_StatusCode_name(status));
  }
  return status == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
