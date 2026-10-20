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

#include <open62541/plugin/historydatabase.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "include/OPCCommon.h"
#include "include/OPCFileTransfer.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"

extern "C" int mp_vsnprintf(char* s, size_t count, const char* format, va_list arg);

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

struct HistoryModificationRecord {
  int32_t value = 0;
  std::string username;
  UA_HistoryUpdateType update_type = UA_HISTORYUPDATETYPE_INSERT;
  UA_DateTime modification_time = 0;
  std::optional<std::vector<uint8_t>> byte_string_value = std::nullopt;
};

enum class FileMethod {
  Open,
  Close,
  Read,
  Write
};

enum class MalformedResponse {
  WrongOutputType,
  ExtraOutput
};

class OpcUaTestServer {
 public:
  explicit OpcUaTestServer(UA_UInt16 port = 4840) : server_(UA_Server_new()) {
    UA_ServerConfig_setDefault(UA_Server_getConfig(server_));

    auto config = UA_Server_getConfig(server_);
    UA_ServerConfig_setMinimal(config, port, nullptr);
    config->logging->log = [](void* log_context, UA_LogLevel level, UA_LogCategory /*category*/, const char* msg, va_list args) {
      char buffer[1024];
      mp_vsnprintf(buffer, sizeof(buffer), msg, args);

      std::string level_str;
      switch (level) {
        case UA_LOGLEVEL_TRACE:
          return;
        case UA_LOGLEVEL_DEBUG:
          level_str = "DEBUG";
          break;
        case UA_LOGLEVEL_INFO:
          level_str = "INFO";
          break;
        case UA_LOGLEVEL_WARNING:
          level_str = "WARNING";
          break;
        case UA_LOGLEVEL_ERROR:
          level_str = "ERROR";
          break;
        case UA_LOGLEVEL_FATAL:
          level_str = "FATAL";
          break;
        default:
          level_str = "UNKNOWN";
          break;
      }

      std::string log_message = "[" + level_str + "] " + buffer + "\n";
      auto server = static_cast<OpcUaTestServer*>(log_context);
      server->addLog(log_message);
    };

    config->logging->context = this;

    UA_HistoryDatabase history_database;
    memset(&history_database, 0, sizeof(history_database));
    history_database.context = this;
    history_database.readRaw = &OpcUaTestServer::readRawCallback;
    history_database.readModified = &OpcUaTestServer::readModifiedCallback;
    config->historyDatabase = history_database;

    ns_index_ = UA_Server_addNamespace(server_, "custom.namespace");

    UA_NodeId simulator_node = addObject("Simulator", UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER));
    UA_NodeId default_node = addObject("Default", simulator_node);
    UA_NodeId device1_node = addObject("Device1", default_node);
    UA_NodeId device2_node = addObject("Device2", default_node);

    addIntVariable("INT1", opc::OPCNodeIDType::String, device1_node, 1);
    addIntVariable("INT2", opc::OPCNodeIDType::String, device1_node, 2);
    UA_NodeId int3_node = addIntVariable("INT3", opc::OPCNodeIDType::String, device1_node, 3);
    addIntVariable("INT4", opc::OPCNodeIDType::String, int3_node, 4);
    addStringVariable("StringNode", "the.answer.node", UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), 42);
    addIntVariable("666", opc::OPCNodeIDType::Int, device2_node, 256);
    addIntVariable("72962b91-fa75-4ae6-8d28-b404dc7daf63", opc::OPCNodeIDType::Guid, device2_node, 7);
    addObject("GuidObject", device2_node, UA_NODEID_GUID(ns_index_, UA_GUID("aabbccdd-1122-3344-5566-778899aabbcc")));

    UA_NodeId ambiguous_parent_node = addObject("AmbiguousParent", default_node);
    addObject("Ambiguous", ambiguous_parent_node);
    addObject("Ambiguous", ambiguous_parent_node);

    addEventSource("EventSource", opc::OPCNodeIDType::String, "EventSource");
    addEventSource("IntEventSource", opc::OPCNodeIDType::Int, "4200");
    addEventSource("GuidEventSource", opc::OPCNodeIDType::Guid, "aabbccdd-eeff-0a1b-2c3d-4e5f6a7b8c9d");

    setHistory("INT1",
        {HistoryModificationRecord{.value = 1,
            .username = "test_user",
            .update_type = UA_HISTORYUPDATETYPE_REPLACE,
            .modification_time = makeDateTime(2024, 6, 15, 10, 30, 0, 0)}});
    setHistory("INT2",
        {HistoryModificationRecord{.value = 2,
             .username = "admin_user",
             .update_type = UA_HISTORYUPDATETYPE_INSERT,
             .modification_time = makeDateTime(2021, 3, 15, 11, 30, 0, 0)},
            HistoryModificationRecord{.value = 3,
                .username = "admin_user",
                .update_type = UA_HISTORYUPDATETYPE_UPDATE,
                .modification_time = makeDateTime(2025, 11, 11, 11, 30, 0, 0)},
            HistoryModificationRecord{.value = 4,
                .username = "test_user",
                .update_type = UA_HISTORYUPDATETYPE_REPLACE,
                .modification_time = makeDateTime(2026, 3, 11, 11, 30, 0, 0)}});
    setHistory("666",
        {HistoryModificationRecord{.value = 256,
            .username = "integer_user",
            .update_type = UA_HISTORYUPDATETYPE_INSERT,
            .modification_time = makeDateTime(2001, 1, 1, 22, 22, 0, 0)}});
    setHistory("72962b91-fa75-4ae6-8d28-b404dc7daf63",
        {HistoryModificationRecord{.value = 7,
            .username = "guid_user",
            .update_type = UA_HISTORYUPDATETYPE_INSERT,
            .modification_time = makeDateTime(2020, 5, 20, 12, 0, 0, 0)}});
  }

  void start() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
    server_thread_ = std::thread([this]() { UA_Server_run(server_, &running_); });
    ensureConnection();
  }

  void stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
      return;
    }
    running_ = false;
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  ~OpcUaTestServer() {
    stop();
    UA_Server_delete(server_);
  }

  UA_UInt16 getNamespaceIndex() const {
    return ns_index_;
  }

  void setHistory(const std::string& node_id, std::vector<HistoryModificationRecord> records) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_records_[node_id] = std::move(records);
  }

  // When set to a non-zero value the server returns at most this many entries per history read response and hands out a
  // continuation point for the rest, forcing the client to drive the read across multiple callback invocations.
  void setHistoryPageSize(size_t page_size) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_page_size_ = page_size;
  }

  static UA_DateTime makeDateTime(uint16_t year, uint16_t month, uint16_t day, uint16_t hour, uint16_t min, uint16_t sec, uint16_t milli) {
    UA_DateTimeStruct dts{};
    dts.year = year;
    dts.month = month;
    dts.day = day;
    dts.hour = hour;
    dts.min = min;
    dts.sec = sec;
    dts.milliSec = milli;
    return UA_DateTime_fromStruct(dts);
  }

  void addLog(const std::string& log) {
    std::lock_guard<std::mutex> lock(server_logs_mutex_);
    server_logs_.push_back(log);
  }

  std::vector<std::string> getLogs() const {
    std::lock_guard<std::mutex> lock(server_logs_mutex_);
    return server_logs_;
  }

  void updateNodeTimestamp(const std::string& full_path) {
    UA_Int32 new_value = full_path[full_path.size() - 1] - '0';
    updateNodeValue(full_path, new_value);
  }

  void triggerEvent(const std::string& message, UA_UInt16 severity, UA_UInt32 event_type = UA_NS0ID_BASEEVENTTYPE,
      opc::OPCNodeIDType source_id_type = opc::OPCNodeIDType::String) {
    raiseEvent(event_source_nodes_.at(source_id_type), message, severity, event_type, nullptr);
  }

  void triggerEventWithFields(const std::string& message, UA_UInt16 severity, const std::vector<std::pair<std::string, std::string>>& fields) {
    std::vector<UA_KeyValuePair> pairs(fields.size());
    std::vector<UA_String> values(fields.size());
    for (size_t i = 0; i < fields.size(); ++i) {
      pairs[i].key = UA_QUALIFIEDNAME(0, const_cast<char*>(fields[i].first.c_str()));
      values[i] = UA_STRING(const_cast<char*>(fields[i].second.c_str()));
      UA_Variant_setScalar(&pairs[i].value, &values[i], &UA_TYPES[UA_TYPES_STRING]);
    }
    UA_KeyValueMap field_map{pairs.size(), pairs.data()};
    raiseEvent(event_source_nodes_.at(opc::OPCNodeIDType::String), message, severity, UA_NS0ID_BASEEVENTTYPE, &field_map);
  }

  void triggerEventWithUnconvertibleField(const std::string& message, UA_UInt16 severity, const std::string& field_name) {
    UA_Range range{.low = 0.0, .high = 100.0};
    UA_KeyValuePair pair;
    pair.key = UA_QUALIFIEDNAME(0, const_cast<char*>(field_name.c_str()));
    UA_Variant_setScalar(&pair.value, &range, &UA_TYPES[UA_TYPES_RANGE]);
    UA_KeyValueMap field_map{1, &pair};
    raiseEvent(event_source_nodes_.at(opc::OPCNodeIDType::String), message, severity, UA_NS0ID_BASEEVENTTYPE, &field_map);
  }

  void updateNodeValue(const std::string& full_path, int32_t new_value) {
    std::lock_guard<std::mutex> lock(mutex_);

    UA_Variant variant;
    UA_Variant_init(&variant);
    UA_Variant_setScalar(&variant, &new_value, &UA_TYPES[UA_TYPES_INT32]);

    UA_StatusCode status = UA_Server_writeValue(server_, node_ids_[full_path], variant);

    if (status != UA_STATUSCODE_GOOD) {
      throw std::runtime_error("Failed to write value to node");
    }
  }

  opc::NodeId addFile(const std::string& name, UA_NodeId requested_node_id, UA_NodeId parent, const std::string& content,
      std::optional<UA_UInt32> max_byte_string_length = std::nullopt, bool writable = true, std::optional<UA_UInt16> browse_name_namespace_index = std::nullopt) {
    TestFile* file = nullptr;
    {
      std::lock_guard<std::mutex> lock(files_mutex_);
      file = &files_[name];
      file->content = content;
      file->writable = writable;
    }

    UA_ObjectAttributes attr = UA_ObjectAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT(const_cast<char*>("en-US"), const_cast<char*>(name.c_str()));
    UA_NodeId file_node_id;
    auto status = UA_Server_addObjectNode(server_, requested_node_id, parent, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(browse_name_namespace_index.value_or(ns_index_), const_cast<char*>(name.c_str())), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE), attr, file,
        &file_node_id);
    if (status != UA_STATUSCODE_GOOD) {
      throw std::runtime_error("Failed to add file node");
    }

    std::array<UA_Argument, 1> mode_arguments{makeArgument("Mode", UA_TYPES_BYTE)};
    std::array<UA_Argument, 1> handle_arguments{makeArgument("FileHandle", UA_TYPES_UINT32)};
    std::array<UA_Argument, 2> read_arguments{makeArgument("FileHandle", UA_TYPES_UINT32), makeArgument("Length", UA_TYPES_INT32)};
    std::array<UA_Argument, 2> write_arguments{makeArgument("FileHandle", UA_TYPES_UINT32), makeArgument("Data", UA_TYPES_BYTESTRING)};
    std::array<UA_Argument, 1> data_arguments{makeArgument("Data", UA_TYPES_BYTESTRING)};
    std::unordered_map<FileMethod, opc::NodeId> method_node_ids;
    method_node_ids[FileMethod::Open] = addMethod(file_node_id, "Open", &OpcUaTestServer::fileOpenCallback, mode_arguments, handle_arguments);
    method_node_ids[FileMethod::Close] = addMethod(file_node_id, "Close", &OpcUaTestServer::fileCloseCallback, handle_arguments, {});
    method_node_ids[FileMethod::Read] = addMethod(file_node_id, "Read", &OpcUaTestServer::fileReadCallback, read_arguments, data_arguments);
    method_node_ids[FileMethod::Write] = addMethod(file_node_id, "Write", &OpcUaTestServer::fileWriteCallback, write_arguments, {});

    UA_UInt64 size = content.size();
    auto size_node_id = addProperty(file_node_id, "Size", &size, UA_TYPES[UA_TYPES_UINT64]);
    std::optional<opc::NodeId> max_byte_string_length_node_id;
    if (max_byte_string_length) {
      UA_UInt32 value = *max_byte_string_length;
      max_byte_string_length_node_id = addProperty(file_node_id, "MaxByteStringLength", &value, UA_TYPES[UA_TYPES_UINT32]);
    }
    std::lock_guard<std::mutex> lock(files_mutex_);
    file->node_id = opc::NodeId::copyOf(file_node_id);
    file->method_node_ids = std::move(method_node_ids);
    file->size_node_id = std::move(size_node_id);
    file->max_byte_string_length_node_id = std::move(max_byte_string_length_node_id);
    return opc::NodeId{file_node_id};
  }

  opc::NodeId addFileDirectory(const std::string& name, UA_NodeId requested_node_id, UA_NodeId parent) {
    UA_ObjectAttributes attr = UA_ObjectAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT(const_cast<char*>("en-US"), const_cast<char*>(name.c_str()));
    UA_NodeId directory_node_id;
    auto status = UA_Server_addObjectNode(server_, requested_node_id, parent, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(ns_index_, const_cast<char*>(name.c_str())), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE), attr, nullptr, &directory_node_id);
    if (status != UA_STATUSCODE_GOOD) {
      throw std::runtime_error("Failed to add file directory node");
    }

    std::array<UA_Argument, 2> create_file_arguments{makeArgument("FileName", UA_TYPES_STRING), makeArgument("RequestFileOpen", UA_TYPES_BOOLEAN)};
    std::array<UA_Argument, 2> create_file_outputs{makeArgument("FileNodeId", UA_TYPES_NODEID), makeArgument("FileHandle", UA_TYPES_UINT32)};
    addMethod(directory_node_id, "CreateFile", &OpcUaTestServer::createFileCallback, create_file_arguments, create_file_outputs);
    return opc::NodeId{directory_node_id};
  }

  void removeFile(const std::string& name) {
    UA_NodeId file_node_id;
    {
      std::lock_guard<std::mutex> lock(files_mutex_);
      file_node_id = files_.at(name).node_id.get();
    }
    if (UA_Server_deleteNode(server_, file_node_id, true) != UA_STATUSCODE_GOOD) {
      throw std::runtime_error("Failed to remove file node");
    }
    std::lock_guard<std::mutex> lock(files_mutex_);
    files_.erase(name);
  }

  std::optional<std::string> getFileContent(const std::string& name) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    auto it = files_.find(name);
    if (it == files_.end()) {
      return std::nullopt;
    }
    return it->second.content;
  }

  size_t getOpenFileHandleCount(const std::string& name) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    return files_.at(name).handles.size();
  }

  size_t getMaxChunkSize(const std::string& name) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    return files_.at(name).max_chunk_size;
  }

  void setFileFailure(const std::string& name, FileMethod method, UA_StatusCode status, size_t successful_calls_before_failure = 0) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    files_.at(name).failures[method] = TestFile::Failure{.status = status, .remaining_successful_calls = successful_calls_before_failure};
  }

  void clearFileFailure(const std::string& name, FileMethod method) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    files_.at(name).failures.erase(method);
  }

  // Makes the Open or Read method of the file return an output of the wrong type, or an additional output
  void setFileMalformedResponse(const std::string& name, FileMethod method, MalformedResponse response) {
    gsl_Expects(method == FileMethod::Open || method == FileMethod::Read);
    UA_NodeId file_node_id;
    UA_NodeId method_node_id;
    {
      std::lock_guard<std::mutex> lock(files_mutex_);
      auto& file = files_.at(name);
      file.malformed_responses[method] = response;
      file_node_id = file.node_id.get();  // shallow copies, the nodes outlive this call
      method_node_id = file.method_node_ids.at(method).get();
    }
    if (response != MalformedResponse::ExtraOutput) {
      return;
    }
    // The server allocates as many outputs for a method call as the method declares, so the method is replaced with one declaring an extra output
    if (UA_Server_deleteNode(server_, method_node_id, true) != UA_STATUSCODE_GOOD) {
      throw std::runtime_error("Failed to remove the method node");
    }
    opc::NodeId new_method_node_id;
    if (method == FileMethod::Open) {
      std::array<UA_Argument, 1> inputs{makeArgument("Mode", UA_TYPES_BYTE)};
      std::array<UA_Argument, 2> outputs{makeArgument("FileHandle", UA_TYPES_UINT32), makeArgument("ExtraOutput", UA_TYPES_UINT32)};
      new_method_node_id = addMethod(file_node_id, "Open", &OpcUaTestServer::fileOpenCallback, inputs, outputs);
    } else {
      std::array<UA_Argument, 2> inputs{makeArgument("FileHandle", UA_TYPES_UINT32), makeArgument("Length", UA_TYPES_INT32)};
      std::array<UA_Argument, 2> outputs{makeArgument("Data", UA_TYPES_BYTESTRING), makeArgument("ExtraOutput", UA_TYPES_BYTESTRING)};
      new_method_node_id = addMethod(file_node_id, "Read", &OpcUaTestServer::fileReadCallback, inputs, outputs);
    }
    std::lock_guard<std::mutex> lock(files_mutex_);
    files_.at(name).method_node_ids[method] = std::move(new_method_node_id);
  }

  // Makes the CreateFile method of every directory fail with the given status without creating the file
  void setCreateFileFailure(UA_StatusCode status) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    create_file_failure_ = status;
  }

  // Makes the CreateFile method of every directory return the node id of the created file with the wrong type
  void setCreateFileMalformedResponse(MalformedResponse response) {
    gsl_Expects(response == MalformedResponse::WrongOutputType);
    std::lock_guard<std::mutex> lock(files_mutex_);
    create_file_malformed_response_ = response;
  }

  // Signals the end of the file with an empty variant instead of an empty ByteString, as some servers do
  void setFileEndOfFileAsEmptyVariant(const std::string& name) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    files_.at(name).end_of_file_as_empty_variant = true;
  }

  // Returns at most the given number of bytes from a Read call, even if more is requested and available
  void setFileMaxReadLength(const std::string& name, size_t max_read_length) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    files_.at(name).max_read_length = max_read_length;
  }

  std::optional<UA_Byte> getLastOpenMode(const std::string& name) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    return files_.at(name).last_open_mode;
  }

  void addFileProperty(const std::string& name, const char* property_name, void* value, const UA_DataType& type) {
    UA_NodeId file_node_id;
    {
      std::lock_guard<std::mutex> lock(files_mutex_);
      file_node_id = files_.at(name).node_id.get();  // shallow copy, the file node outlives this call
    }
    addProperty(file_node_id, property_name, value, type);
  }

  void setFileMaxByteStringLength(const std::string& name, UA_UInt32 value) {
    writeProperty(getFileProperty(name, &TestFile::max_byte_string_length_node_id), &value, UA_TYPES[UA_TYPES_UINT32]);
  }

  void setFileContent(const std::string& name, const std::string& content) {
    {
      std::lock_guard<std::mutex> lock(files_mutex_);
      files_.at(name).content = content;
    }
    UA_UInt64 size = content.size();
    writeProperty(getFileProperty(name, &TestFile::size_node_id), &size, UA_TYPES[UA_TYPES_UINT64]);
  }

  void removeFileSizeProperty(const std::string& name) {
    const auto size_node_id = getFileProperty(name, &TestFile::size_node_id);
    if (UA_Server_deleteNode(server_, size_node_id, true) != UA_STATUSCODE_GOOD) {
      throw std::runtime_error("Failed to remove Size property");
    }
    std::lock_guard<std::mutex> lock(files_mutex_);
    files_.at(name).size_node_id.reset();
  }

  void setFileLastModifiedTime(const std::string& name, UA_DateTime last_modified_time) {
    UA_NodeId file_node_id;
    bool has_property = false;
    {
      std::lock_guard<std::mutex> lock(files_mutex_);
      const auto& file = files_.at(name);
      file_node_id = file.node_id.get();
      has_property = file.last_modified_time_node_id.has_value();
    }
    if (has_property) {
      writeProperty(getFileProperty(name, &TestFile::last_modified_time_node_id), &last_modified_time, UA_TYPES[UA_TYPES_DATETIME]);
      return;
    }
    auto property_node_id = addProperty(file_node_id, "LastModifiedTime", &last_modified_time, UA_TYPES[UA_TYPES_DATETIME]);
    std::lock_guard<std::mutex> lock(files_mutex_);
    files_.at(name).last_modified_time_node_id = std::move(property_node_id);
  }

  void setServerMaxByteStringLength(UA_UInt32 value) {
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT(const_cast<char*>("en-US"), const_cast<char*>("MaxByteStringLength"));
    UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_UINT32]);
    auto status = UA_Server_addVariableNode(server_, UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_SERVERCAPABILITIES_MAXBYTESTRINGLENGTH),
        UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_SERVERCAPABILITIES), UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
        UA_QUALIFIEDNAME(0, const_cast<char*>("MaxByteStringLength")), UA_NODEID_NUMERIC(0, UA_NS0ID_PROPERTYTYPE), attr, nullptr, nullptr);
    if (status != UA_STATUSCODE_GOOD) {
      throw std::runtime_error("Failed to add ServerCapabilities MaxByteStringLength property");
    }
  }

  // References exceeding the limit are returned on further pages of a Browse request, which have to be requested with BrowseNext
  void setMaxReferencesPerNode(UA_UInt32 value) {
    UA_Server_getConfig(server_)->maxReferencesPerNode = value;
  }

 private:
  struct TestFile {
    struct OpenHandle {
      UA_Byte mode = 0;
      size_t position = 0;
    };

    struct Failure {
      UA_StatusCode status = UA_STATUSCODE_GOOD;
      size_t remaining_successful_calls = 0;
    };

    std::string content;
    std::unordered_map<UA_UInt32, OpenHandle> handles;
    size_t max_chunk_size = 0;
    std::unordered_map<FileMethod, Failure> failures;
    std::unordered_map<FileMethod, MalformedResponse> malformed_responses;
    bool end_of_file_as_empty_variant = false;
    std::optional<size_t> max_read_length;
    std::optional<UA_Byte> last_open_mode;
    opc::NodeId node_id;
    std::unordered_map<FileMethod, opc::NodeId> method_node_ids;
    std::optional<opc::NodeId> size_node_id;
    std::optional<opc::NodeId> max_byte_string_length_node_id;
    std::optional<opc::NodeId> last_modified_time_node_id;
    bool writable = true;

    UA_StatusCode injectedFailure(FileMethod method) {
      auto it = failures.find(method);
      if (it == failures.end()) {
        return UA_STATUSCODE_GOOD;
      }
      if (it->second.remaining_successful_calls > 0) {
        --it->second.remaining_successful_calls;
        return UA_STATUSCODE_GOOD;
      }
      return it->second.status;
    }

    std::optional<MalformedResponse> malformedResponse(FileMethod method) const {
      auto it = malformed_responses.find(method);
      return it == malformed_responses.end() ? std::nullopt : std::optional<MalformedResponse>(it->second);
    }
  };

  opc::NodeId addProperty(const UA_NodeId& parent_node_id, const char* name, void* value, const UA_DataType& type) {
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT(const_cast<char*>("en-US"), const_cast<char*>(name));
    UA_Variant_setScalar(&attr.value, value, &type);
    opc::NodeId property_node_id;
    auto status = UA_Server_addVariableNode(server_, UA_NODEID_NUMERIC(ns_index_, 0), parent_node_id, UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
        UA_QUALIFIEDNAME(0, const_cast<char*>(name)), UA_NODEID_NUMERIC(0, UA_NS0ID_PROPERTYTYPE), attr, nullptr, property_node_id.receive());
    if (status != UA_STATUSCODE_GOOD) {
      throw std::runtime_error(std::string("Failed to add property ") + name);
    }
    return property_node_id;
  }

  void writeProperty(const UA_NodeId& property_node_id, void* value, const UA_DataType& type) {
    UA_Variant variant;
    UA_Variant_setScalar(&variant, value, &type);
    if (UA_Server_writeValue(server_, property_node_id, variant) != UA_STATUSCODE_GOOD) {
      throw std::runtime_error("Failed to update property value");
    }
  }

  UA_NodeId getFileProperty(const std::string& name, std::optional<opc::NodeId> TestFile::*property) {
    std::lock_guard<std::mutex> lock(files_mutex_);
    const auto& property_node_id = files_.at(name).*property;
    if (!property_node_id) {
      throw std::runtime_error("File " + name + " does not have the requested property");
    }
    return property_node_id->get();
  }

  static UA_Argument makeArgument(const char* name, size_t type_index) {
    UA_Argument argument;
    UA_Argument_init(&argument);
    argument.name = UA_STRING(const_cast<char*>(name));
    argument.dataType = UA_TYPES[type_index].typeId;
    argument.valueRank = UA_VALUERANK_SCALAR;
    return argument;
  }

  opc::NodeId addMethod(const UA_NodeId& object_node_id, const char* name, UA_MethodCallback callback, std::span<const UA_Argument> inputs,
      std::span<const UA_Argument> outputs) {
    UA_MethodAttributes attr = UA_MethodAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT(const_cast<char*>("en-US"), const_cast<char*>(name));
    attr.executable = true;
    attr.userExecutable = true;
    opc::NodeId method_node_id;
    auto status = UA_Server_addMethodNode(server_, UA_NODEID_NUMERIC(ns_index_, 0), object_node_id, UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
        UA_QUALIFIEDNAME(0, const_cast<char*>(name)), attr, callback, inputs.size(), inputs.data(), outputs.size(), outputs.data(), this,
        method_node_id.receive());
    if (status != UA_STATUSCODE_GOOD) {
      throw std::runtime_error(std::string("Failed to add method node ") + name);
    }
    return method_node_id;
  }

  static UA_StatusCode fileOpenCallback(UA_Server* /*server*/, const UA_NodeId* /*session_id*/, void* /*session_context*/, const UA_NodeId* /*method_id*/,
      void* method_context, const UA_NodeId* /*object_id*/, void* object_context, size_t /*input_size*/, const UA_Variant* input, size_t output_size,
      UA_Variant* output) {
    auto* self = static_cast<OpcUaTestServer*>(method_context);
    auto* file = static_cast<TestFile*>(object_context);
    const auto mode = *static_cast<const UA_Byte*>(input[0].data);
    std::lock_guard<std::mutex> lock(self->files_mutex_);
    if (auto status = file->injectedFailure(FileMethod::Open); status != UA_STATUSCODE_GOOD) {
      return status;
    }

    if ((mode & opc::FILE_MODE_WRITE) && !file->writable) {
      return UA_STATUSCODE_BADNOTWRITABLE;
    }
    // No handle is registered for a malformed response, as the client cannot use it to close the file
    if (const auto malformed_response = file->malformedResponse(FileMethod::Open)) {
      if (*malformed_response == MalformedResponse::WrongOutputType) {
        UA_Int32 handle = 0;
        return UA_Variant_setScalarCopy(&output[0], &handle, &UA_TYPES[UA_TYPES_INT32]);
      }
      UA_UInt32 handle = 0;
      for (size_t i = 0; i < output_size; ++i) {
        if (auto status = UA_Variant_setScalarCopy(&output[i], &handle, &UA_TYPES[UA_TYPES_UINT32]); status != UA_STATUSCODE_GOOD) {
          return status;
        }
      }
      return UA_STATUSCODE_GOOD;
    }

    if (mode & opc::FILE_MODE_ERASE_EXISTING) {
      file->content.clear();
    }
    UA_UInt32 handle = self->next_file_handle_++;
    file->handles[handle] = TestFile::OpenHandle{.mode = mode, .position = (mode & opc::FILE_MODE_APPEND) ? file->content.size() : 0};
    file->last_open_mode = mode;
    return UA_Variant_setScalarCopy(&output[0], &handle, &UA_TYPES[UA_TYPES_UINT32]);
  }

  static UA_StatusCode fileCloseCallback(UA_Server* /*server*/, const UA_NodeId* /*session_id*/, void* /*session_context*/, const UA_NodeId* /*method_id*/,
      void* method_context, const UA_NodeId* /*object_id*/, void* object_context, size_t /*input_size*/, const UA_Variant* input, size_t /*output_size*/,
      UA_Variant* /*output*/) {
    auto* self = static_cast<OpcUaTestServer*>(method_context);
    auto* file = static_cast<TestFile*>(object_context);
    const auto handle = *static_cast<const UA_UInt32*>(input[0].data);
    std::lock_guard<std::mutex> lock(self->files_mutex_);
    if (auto status = file->injectedFailure(FileMethod::Close); status != UA_STATUSCODE_GOOD) {
      return status;
    }
    return file->handles.erase(handle) == 1 ? UA_STATUSCODE_GOOD : UA_STATUSCODE_BADINVALIDARGUMENT;
  }

  static UA_StatusCode fileReadCallback(UA_Server* /*server*/, const UA_NodeId* /*session_id*/, void* /*session_context*/, const UA_NodeId* /*method_id*/,
      void* method_context, const UA_NodeId* /*object_id*/, void* object_context, size_t /*input_size*/, const UA_Variant* input, size_t output_size,
      UA_Variant* output) {
    auto* self = static_cast<OpcUaTestServer*>(method_context);
    auto* file = static_cast<TestFile*>(object_context);
    const auto handle = *static_cast<const UA_UInt32*>(input[0].data);
    const auto length = *static_cast<const UA_Int32*>(input[1].data);
    std::lock_guard<std::mutex> lock(self->files_mutex_);
    if (auto status = file->injectedFailure(FileMethod::Read); status != UA_STATUSCODE_GOOD) {
      return status;
    }
    auto it = file->handles.find(handle);
    if (it == file->handles.end() || !(it->second.mode & opc::FILE_MODE_READ) || length < 0) {
      return UA_STATUSCODE_BADINVALIDSTATE;
    }
    file->max_chunk_size = std::max(file->max_chunk_size, gsl::narrow<size_t>(length));
    if (file->malformedResponse(FileMethod::Read) == MalformedResponse::WrongOutputType) {
      UA_String string = UA_STRING(const_cast<char*>("not a byte string"));
      return UA_Variant_setScalarCopy(&output[0], &string, &UA_TYPES[UA_TYPES_STRING]);
    }
    auto& position = it->second.position;
    const auto read_length = std::min(gsl::narrow<size_t>(length), file->max_read_length.value_or(std::numeric_limits<size_t>::max()));
    const auto data = file->content.substr(std::min(position, file->content.size()), read_length);
    position += data.size();
    if (data.empty() && file->end_of_file_as_empty_variant) {
      return UA_STATUSCODE_GOOD;
    }
    UA_ByteString byte_string{data.size(), reinterpret_cast<UA_Byte*>(const_cast<char*>(data.data()))};
    // For an ExtraOutput malformed response the server allocates more than one output, all of them are filled
    for (size_t i = 0; i < output_size; ++i) {
      if (auto status = UA_Variant_setScalarCopy(&output[i], &byte_string, &UA_TYPES[UA_TYPES_BYTESTRING]); status != UA_STATUSCODE_GOOD) {
        return status;
      }
    }
    return UA_STATUSCODE_GOOD;
  }

  static UA_StatusCode fileWriteCallback(UA_Server* /*server*/, const UA_NodeId* /*session_id*/, void* /*session_context*/, const UA_NodeId* /*method_id*/,
      void* method_context, const UA_NodeId* /*object_id*/, void* object_context, size_t /*input_size*/, const UA_Variant* input, size_t /*output_size*/,
      UA_Variant* /*output*/) {
    auto* self = static_cast<OpcUaTestServer*>(method_context);
    auto* file = static_cast<TestFile*>(object_context);
    const auto handle = *static_cast<const UA_UInt32*>(input[0].data);
    const auto* data = static_cast<const UA_ByteString*>(input[1].data);
    std::lock_guard<std::mutex> lock(self->files_mutex_);
    if (auto status = file->injectedFailure(FileMethod::Write); status != UA_STATUSCODE_GOOD) {
      return status;
    }
    auto it = file->handles.find(handle);
    if (it == file->handles.end() || !(it->second.mode & opc::FILE_MODE_WRITE)) {
      return UA_STATUSCODE_BADINVALIDSTATE;
    }
    file->max_chunk_size = std::max(file->max_chunk_size, data->length);
    auto& position = it->second.position;
    file->content.replace(position, data->length, reinterpret_cast<const char*>(data->data), data->length);
    position += data->length;
    return UA_STATUSCODE_GOOD;
  }

  static UA_StatusCode createFileCallback(UA_Server* /*server*/, const UA_NodeId* /*session_id*/, void* /*session_context*/, const UA_NodeId* /*method_id*/,
      void* method_context, const UA_NodeId* object_id, void* /*object_context*/, size_t /*input_size*/, const UA_Variant* input, size_t /*output_size*/,
      UA_Variant* output) {
    auto* self = static_cast<OpcUaTestServer*>(method_context);
    const auto* ua_file_name = static_cast<const UA_String*>(input[0].data);
    const std::string file_name(reinterpret_cast<const char*>(ua_file_name->data), ua_file_name->length);
    std::optional<MalformedResponse> malformed_response;
    {
      std::lock_guard<std::mutex> lock(self->files_mutex_);
      if (self->create_file_failure_) {
        return *self->create_file_failure_;
      }
      if (self->files_.contains(file_name)) {
        return UA_STATUSCODE_BADBROWSENAMEDUPLICATED;
      }
      malformed_response = self->create_file_malformed_response_;
    }
    const auto file_node_id = self->addFile(file_name, UA_NODEID_NUMERIC(self->ns_index_, 0), *object_id, "");
    UA_UInt32 file_handle = 0;
    if (malformed_response == MalformedResponse::WrongOutputType) {
      return UA_Variant_setScalarCopy(&output[0], &file_handle, &UA_TYPES[UA_TYPES_UINT32]);
    }
    auto status = UA_Variant_setScalarCopy(&output[0], &file_node_id.get(), &UA_TYPES[UA_TYPES_NODEID]);
    if (status != UA_STATUSCODE_GOOD) {
      return status;
    }
    return UA_Variant_setScalarCopy(&output[1], &file_handle, &UA_TYPES[UA_TYPES_UINT32]);
  }

  void raiseEvent(const UA_NodeId& source_node, const std::string& message, UA_UInt16 severity, UA_UInt32 event_type, const UA_KeyValueMap* fields) {
    UA_LocalizedText message_text = UA_LOCALIZEDTEXT(const_cast<char*>("en-US"), const_cast<char*>(message.c_str()));
    auto status = UA_Server_createEvent(server_, source_node, UA_NODEID_NUMERIC(0, event_type), severity, message_text, fields, nullptr, nullptr);
    if (status != UA_STATUSCODE_GOOD) {
      throw std::runtime_error("Failed to create event");
    }
  }

  static std::string nodeIdToString(const UA_NodeId& id) {
    if (id.identifierType == UA_NODEIDTYPE_STRING) {
      return std::string(reinterpret_cast<const char*>(id.identifier.string.data), id.identifier.string.length);
    } else if (id.identifierType == UA_NODEIDTYPE_NUMERIC) {
      return std::to_string(id.identifier.numeric);
    } else if (id.identifierType == UA_NODEIDTYPE_GUID) {
      std::array<char, 37> guid_str;
      snprintf(guid_str.data(),
          guid_str.size(),
          "%08x-%04x-%04x-%04x-%012" PRIx64,
          id.identifier.guid.data1,
          id.identifier.guid.data2,
          id.identifier.guid.data3,
          (id.identifier.guid.data4[0] << 8) | id.identifier.guid.data4[1],
          (gsl::narrow<uint64_t>(id.identifier.guid.data4[2]) << 40) | (gsl::narrow<uint64_t>(id.identifier.guid.data4[3]) << 32) |
              (gsl::narrow<uint64_t>(id.identifier.guid.data4[4]) << 24) | (gsl::narrow<uint64_t>(id.identifier.guid.data4[5]) << 16) |
              (gsl::narrow<uint64_t>(id.identifier.guid.data4[6]) << 8) | gsl::narrow<uint64_t>(id.identifier.guid.data4[7]));
      return std::string(guid_str.data());
    }
    return {};
  }

  static std::vector<const HistoryModificationRecord*> selectRecords(const std::vector<HistoryModificationRecord>& records, UA_DateTime start_time,
      UA_DateTime end_time, size_t offset, size_t page_size, bool& has_more_data) {
    has_more_data = false;
    std::vector<const HistoryModificationRecord*> matching;
    for (const auto& record : records) {
      if (record.modification_time >= start_time && record.modification_time < end_time) {
        matching.push_back(&record);
      }
    }
    if (offset >= matching.size()) {
      return {};
    }
    std::vector<const HistoryModificationRecord*> selected(matching.begin() + offset, matching.end());
    if (page_size > 0 && selected.size() > page_size) {
      selected.resize(page_size);
      has_more_data = true;
    }
    return selected;
  }

  static size_t readContinuationOffset(const UA_ByteString& continuation_point) {
    size_t offset = 0;
    if (continuation_point.length == sizeof(size_t)) {
      memcpy(&offset, continuation_point.data, sizeof(size_t));
    }
    return offset;
  }

  static void writeContinuationPoint(UA_HistoryReadResult& result, size_t next_offset) {
    UA_ByteString_allocBuffer(&result.continuationPoint, sizeof(size_t));
    memcpy(result.continuationPoint.data, &next_offset, sizeof(size_t));
  }

  static UA_DataValue* makeDataValues(const std::vector<const HistoryModificationRecord*>& records) {
    auto* values = static_cast<UA_DataValue*>(UA_Array_new(records.size(), &UA_TYPES[UA_TYPES_DATAVALUE]));
    for (size_t i = 0; i < records.size(); ++i) {
      if (records[i]->byte_string_value) {
        const auto& bytes = *records[i]->byte_string_value;
        UA_ByteString value{bytes.size(),
            const_cast<UA_Byte*>(bytes.data())};  // NOLINT(cppcoreguidelines-pro-type-const-cast) setScalarCopy only reads it
        UA_Variant_setScalarCopy(&values[i].value, &value, &UA_TYPES[UA_TYPES_BYTESTRING]);
      } else {
        UA_Int32 value = records[i]->value;
        UA_Variant_setScalarCopy(&values[i].value, &value, &UA_TYPES[UA_TYPES_INT32]);
      }
      values[i].hasValue = true;
      values[i].hasSourceTimestamp = true;
      values[i].sourceTimestamp = records[i]->modification_time;
    }
    return values;
  }

  static UA_ModificationInfo* makeModificationInfos(const std::vector<const HistoryModificationRecord*>& records) {
    auto* mods = static_cast<UA_ModificationInfo*>(UA_Array_new(records.size(), &UA_TYPES[UA_TYPES_MODIFICATIONINFO]));
    for (size_t i = 0; i < records.size(); ++i) {
      mods[i].updateType = records[i]->update_type;
      mods[i].modificationTime = records[i]->modification_time;
      mods[i].userName = UA_STRING_ALLOC(records[i]->username.c_str());
    }
    return mods;
  }

  std::optional<std::vector<const HistoryModificationRecord*>> readNodeHistory(const UA_HistoryReadValueId& node_to_read,
      const UA_ReadRawModifiedDetails& details, UA_HistoryReadResult& result, size_t offset, bool& has_more_data) {
    auto node_id_str = nodeIdToString(node_to_read.nodeId);
    auto it = history_records_.find(node_id_str);
    auto node_id_it = node_ids_.find(node_id_str);
    if (it == history_records_.end() || node_id_it == node_ids_.end() ||
        node_to_read.nodeId.identifierType != node_id_it->second.get().identifierType) {
      result.statusCode = UA_STATUSCODE_BADNODEIDUNKNOWN;
      return std::nullopt;
    }
    return selectRecords(it->second, details.startTime, details.endTime, offset, history_page_size_, has_more_data);
  }

  static void readRawCallback(UA_Server* /*server*/, void* hdb_context, const UA_NodeId* /*sessionId*/, void* /*session_context*/,
      const UA_RequestHeader* /*request_header*/, const UA_ReadRawModifiedDetails* history_read_details,
      UA_TimestampsToReturn /*timestamps_to_return*/, UA_Boolean /*release_continuation_points*/, size_t nodes_to_read_size,
      const UA_HistoryReadValueId* nodes_to_read, UA_HistoryReadResponse* response, UA_HistoryData* const* const history_data) {
    auto* self = static_cast<OpcUaTestServer*>(hdb_context);
    std::lock_guard<std::mutex> lock(self->history_mutex_);

    for (size_t i = 0; i < nodes_to_read_size; ++i) {
      const size_t offset = readContinuationOffset(nodes_to_read[i].continuationPoint);
      bool has_more_data = false;
      auto records = self->readNodeHistory(nodes_to_read[i], *history_read_details, response->results[i], offset, has_more_data);
      if (!records) {
        continue;
      }

      history_data[i]->dataValues = makeDataValues(*records);
      history_data[i]->dataValuesSize = records->size();
      if (has_more_data) {
        writeContinuationPoint(response->results[i], offset + records->size());
      }
      response->results[i].statusCode = UA_STATUSCODE_GOOD;
    }

    response->responseHeader.serviceResult = UA_STATUSCODE_GOOD;
  }

  static void readModifiedCallback(UA_Server* /*server*/, void* hdb_context, const UA_NodeId* /*sessionId*/, void* /*session_context*/,
      const UA_RequestHeader* /*request_header*/, const UA_ReadRawModifiedDetails* history_read_details,
      UA_TimestampsToReturn /*timestamps_to_return*/, UA_Boolean /*release_continuation_points*/, size_t nodes_to_read_size,
      const UA_HistoryReadValueId* nodes_to_read, UA_HistoryReadResponse* response, UA_HistoryModifiedData* const* const history_data) {
    auto* self = static_cast<OpcUaTestServer*>(hdb_context);
    std::lock_guard<std::mutex> lock(self->history_mutex_);

    for (size_t i = 0; i < nodes_to_read_size; ++i) {
      const size_t offset = readContinuationOffset(nodes_to_read[i].continuationPoint);
      bool has_more_data = false;
      auto records = self->readNodeHistory(nodes_to_read[i], *history_read_details, response->results[i], offset, has_more_data);
      if (!records) {
        continue;
      }

      history_data[i]->dataValues = makeDataValues(*records);
      history_data[i]->dataValuesSize = records->size();
      history_data[i]->modificationInfos = makeModificationInfos(*records);
      history_data[i]->modificationInfosSize = records->size();
      if (has_more_data) {
        writeContinuationPoint(response->results[i], offset + records->size());
      }
      response->results[i].statusCode = UA_STATUSCODE_GOOD;
    }

    response->responseHeader.serviceResult = UA_STATUSCODE_GOOD;
  }

 public:
  UA_NodeId addObject(const char* name, UA_NodeId parent, UA_NodeId requested_node_id = UA_NODEID_NULL) {
    UA_NodeId object_id;
    UA_ObjectAttributes attr = UA_ObjectAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", name);

    auto status = UA_Server_addObjectNode(server_,
        requested_node_id,
        parent,
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(ns_index_, const_cast<char*>(name)),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
        attr,
        nullptr,
        &object_id);

    if (status != UA_STATUSCODE_GOOD) {
      UA_LocalizedText_clear(&attr.displayName);
      throw std::runtime_error("Failed to add object node");
    }

    UA_LocalizedText_clear(&attr.displayName);
    return object_id;
  }

  void addEventSource(const char* name, opc::OPCNodeIDType id_type, const std::string& node_id) {
    auto requested_node_id = opc::buildNodeId(id_type, ns_index_, node_id);
    if (!requested_node_id) {
      throw std::runtime_error("Failed to build the node id of the event source: " + requested_node_id.error());
    }
    event_source_nodes_.emplace(id_type,
        opc::NodeId{addEventSourceObject(name, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), *requested_node_id)});
  }

  UA_NodeId addEventSourceObject(const char* name, UA_NodeId parent, const UA_NodeId& requested_node_id) {
    UA_ObjectAttributes attr = UA_ObjectAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", name);
    attr.eventNotifier = UA_EVENTNOTIFIER_SUBSCRIBE_TO_EVENT;

    UA_NodeId object_id;
    auto status = UA_Server_addObjectNode(server_,
        requested_node_id,
        parent,
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(ns_index_, const_cast<char*>(name)),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
        attr,
        nullptr,
        &object_id);

    UA_LocalizedText_clear(&attr.displayName);

    if (status != UA_STATUSCODE_GOOD) {
      throw std::runtime_error("Failed to add event source object node");
    }

    return object_id;
  }

  UA_StatusCode addNode(UA_NodeId parent_node_id, UA_NodeId& target_node_id, opc::OPCNodeIDType type, const std::string& browse_name,
      const UA_VariableAttributes& attr, UA_UInt32 reference_type = UA_NS0ID_HASCOMPONENT) {
    UA_QualifiedName qname = UA_QUALIFIEDNAME(ns_index_, const_cast<char*>(browse_name.c_str()));
    opc::NodeId node_id;

    switch (type) {
      case opc::OPCNodeIDType::Int:
        node_id = opc::NodeId{UA_NODEID_NUMERIC(ns_index_, gsl::narrow<UA_UInt32>(std::stoul(browse_name)))};
        break;
      case opc::OPCNodeIDType::String:
        node_id = opc::NodeId{UA_NODEID_STRING_ALLOC(ns_index_, browse_name.c_str())};
        break;
      case opc::OPCNodeIDType::Guid:
        node_id = opc::NodeId{UA_NODEID_GUID(ns_index_, UA_GUID(browse_name.c_str()))};
        break;
      default:
        return UA_STATUSCODE_BADNOTSUPPORTED;
    }

    return UA_Server_addVariableNode(server_,
        node_id,
        parent_node_id,
        UA_NODEID_NUMERIC(0, reference_type),
        qname,
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        attr,
        nullptr,
        &target_node_id);
  }

  UA_NodeId addIntVariable(const std::string& name, opc::OPCNodeIDType type, UA_NodeId parent, UA_Int32 value, UA_UInt32 reference_type = UA_NS0ID_HASCOMPONENT) {
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", name.c_str());
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE | UA_ACCESSLEVELMASK_HISTORYREAD;

    UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_INT32]);

    UA_NodeId node_id;
    auto status = addNode(parent, node_id, type, name, attr, reference_type);

    if (status != UA_STATUSCODE_GOOD) {
      UA_LocalizedText_clear(&attr.displayName);
      throw std::runtime_error("Failed to add variable node");
    }

    UA_LocalizedText_clear(&attr.displayName);
    node_ids_[name] = opc::NodeId{node_id};
    return node_ids_[name].get();
  }

 private:
  void addStringVariable(const char* name, const char* node_id_str, UA_NodeId parent, UA_Int32 value) {
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", name);
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_INT32]);

    UA_NodeId requested_id = UA_NODEID_STRING_ALLOC(ns_index_, node_id_str);
    auto status = UA_Server_addVariableNode(server_,
        requested_id,
        parent,
        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
        UA_QUALIFIEDNAME(ns_index_, const_cast<char*>(name)),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        attr,
        nullptr,
        nullptr);

    UA_NodeId_clear(&requested_id);

    if (status != UA_STATUSCODE_GOOD) {
      UA_LocalizedText_clear(&attr.displayName);
      throw std::runtime_error("Failed to add string variable node");
    }

    UA_LocalizedText_clear(&attr.displayName);
  }

  void ensureConnection() {
    REQUIRE(utils::verifyEventHappenedInPollTime(
        5s,
        [&]() {
          auto logs = getLogs();
          return std::find_if(logs.begin(), logs.end(), [](const std::string& message) {
            return message.find("New DiscoveryUrl added") != std::string::npos;
          }) != logs.end();
        },
        100ms));
  }

  UA_Server* server_;
  UA_UInt16 ns_index_;
  UA_Boolean running_ = false;
  std::mutex mutex_;
  std::thread server_thread_;
  mutable std::mutex server_logs_mutex_;
  std::vector<std::string> server_logs_;
  std::unordered_map<std::string, opc::NodeId> node_ids_;
  std::unordered_map<opc::OPCNodeIDType, opc::NodeId> event_source_nodes_;
  std::mutex history_mutex_;
  std::unordered_map<std::string, std::vector<HistoryModificationRecord>> history_records_;
  size_t history_page_size_ = 0;
  std::mutex files_mutex_;
  std::map<std::string, TestFile> files_;  // node based container, the addresses are used as node contexts
  UA_UInt32 next_file_handle_ = 1;
  std::optional<UA_StatusCode> create_file_failure_;
  std::optional<MalformedResponse> create_file_malformed_response_;
};

}  // namespace org::apache::nifi::minifi::test
