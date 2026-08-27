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
#include <cinttypes>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "include/OPCCommon.h"
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

 private:
  static std::string nodeIdToString(const UA_NodeId& id) {
    if (id.identifierType == UA_NODEIDTYPE_STRING) {
      return std::string(reinterpret_cast<const char*>(id.identifier.string.data), id.identifier.string.length);
    } else if (id.identifierType == UA_NODEIDTYPE_NUMERIC) {
      return std::to_string(id.identifier.numeric);
    } else if (id.identifierType == UA_NODEIDTYPE_GUID) {
      char guid_str[37];
      snprintf(guid_str,
          sizeof(guid_str),
          "%08x-%04x-%04x-%04x-%012" PRIx64,
          id.identifier.guid.data1,
          id.identifier.guid.data2,
          id.identifier.guid.data3,
          (id.identifier.guid.data4[0] << 8) | id.identifier.guid.data4[1],
          (gsl::narrow<uint64_t>(id.identifier.guid.data4[2]) << 40) | (gsl::narrow<uint64_t>(id.identifier.guid.data4[3]) << 32) |
              (gsl::narrow<uint64_t>(id.identifier.guid.data4[4]) << 24) | (gsl::narrow<uint64_t>(id.identifier.guid.data4[5]) << 16) |
              (gsl::narrow<uint64_t>(id.identifier.guid.data4[6]) << 8) | gsl::narrow<uint64_t>(id.identifier.guid.data4[7]));
      return std::string(guid_str);
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
      UA_Int32 value = records[i]->value;
      UA_Variant_setScalarCopy(&values[i].value, &value, &UA_TYPES[UA_TYPES_INT32]);
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
    if (it == history_records_.end() || node_id_it == node_ids_.end() || node_to_read.nodeId.identifierType != node_id_it->second.get().identifierType) {
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

  UA_NodeId addObject(const char* name, UA_NodeId parent) {
    UA_NodeId object_id;
    UA_ObjectAttributes attr = UA_ObjectAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", name);

    auto status = UA_Server_addObjectNode(server_,
        UA_NODEID_NULL,
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

  UA_StatusCode addNode(UA_NodeId parent_node_id, UA_NodeId& target_node_id, opc::OPCNodeIDType type, const std::string& browse_name,
      const UA_VariableAttributes& attr) {
    UA_QualifiedName qname = UA_QUALIFIEDNAME(ns_index_, const_cast<char*>(browse_name.c_str()));
    opc::NodeId node_id;

    switch (type) {
      case opc::OPCNodeIDType::Int:
        node_id = opc::NodeId{UA_NODEID_NUMERIC(ns_index_, std::stoi(browse_name))};
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
        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
        qname,
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        attr,
        nullptr,
        &target_node_id);
  }

  UA_NodeId addIntVariable(const std::string& name, opc::OPCNodeIDType type, UA_NodeId parent, UA_Int32 value) {
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", name.c_str());
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE | UA_ACCESSLEVELMASK_HISTORYREAD;

    UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_INT32]);

    UA_NodeId node_id;
    auto status = addNode(parent, node_id, type, name, attr);

    if (status != UA_STATUSCODE_GOOD) {
      UA_LocalizedText_clear(&attr.displayName);
      throw std::runtime_error("Failed to add variable node");
    }

    UA_LocalizedText_clear(&attr.displayName);
    node_ids_[name] = opc::NodeId{node_id};
    return node_ids_[name].get();
  }

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
  std::mutex history_mutex_;
  std::unordered_map<std::string, std::vector<HistoryModificationRecord>> history_records_;
  size_t history_page_size_ = 0;
};

}  // namespace org::apache::nifi::minifi::test
