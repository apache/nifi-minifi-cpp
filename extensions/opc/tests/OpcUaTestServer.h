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

#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include "unit/TestUtils.h"
#include "unit/Catch.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class OpcUaTestServer {
 public:
  explicit OpcUaTestServer(UA_UInt16 port = 4840) : server_(UA_Server_new()) {
    UA_ServerConfig_setDefault(UA_Server_getConfig(server_));

    auto config = UA_Server_getConfig(server_);
    UA_ServerConfig_setMinimal(config, port, nullptr);
    config->logging->log = [] (void *log_context, UA_LogLevel level, UA_LogCategory /*category*/, const char *msg, va_list args) {
      char buffer[1024];
      vsnprintf(buffer, sizeof(buffer), msg, args);

      std::string level_str;
      switch (level) {
          case UA_LOGLEVEL_TRACE: return;
          case UA_LOGLEVEL_DEBUG: level_str = "DEBUG"; break;
          case UA_LOGLEVEL_INFO: level_str = "INFO"; break;
          case UA_LOGLEVEL_WARNING: level_str = "WARNING"; break;
          case UA_LOGLEVEL_ERROR: level_str = "ERROR"; break;
          case UA_LOGLEVEL_FATAL: level_str = "FATAL"; break;
          default: level_str = "UNKNOWN"; break;
      }

      std::string log_message = "[" + level_str + "] " + buffer + "\n";
      auto server = static_cast<OpcUaTestServer*>(log_context);
      server->addLog(log_message);
    };

    config->logging->context = this;

    ns_index_ = UA_Server_addNamespace(server_, "custom.namespace");

    UA_NodeId simulator_node = addObject("Simulator", UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER));
    UA_NodeId default_node = addObject("Default", simulator_node);
    UA_NodeId device1_node = addObject("Device1", default_node);

    auto int1_node = addIntVariable("INT1", device1_node, 1);
    node_ids_["Simulator/Default/Device1/INT1"] = int1_node;
    auto int2_node = addIntVariable("INT2", device1_node, 2);
    node_ids_["Simulator/Default/Device1/INT2"] = int2_node;
    auto int3_node = addIntVariable("INT3", device1_node, 3);
    node_ids_["Simulator/Default/Device1/INT3"] = int3_node;
    auto int4_node = addIntVariable("INT4", int3_node, 4);
    node_ids_["Simulator/Default/Device1/INT4"] = int4_node;
  }

  void start() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
    server_thread_ = std::thread([this]() {
      UA_Server_run(server_, &running_);
    });
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
  UA_NodeId addObject(const char *name, UA_NodeId parent) {
    UA_NodeId object_id;
    UA_ObjectAttributes attr = UA_ObjectAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", name);

    auto status = UA_Server_addObjectNode(
      server_, UA_NODEID_NULL, parent,
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(ns_index_, const_cast<char*>(name)),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
      attr, nullptr, &object_id);

    if (status != UA_STATUSCODE_GOOD) {
      UA_LocalizedText_clear(&attr.displayName);
      throw std::runtime_error("Failed to add object node");
    }

    UA_LocalizedText_clear(&attr.displayName);
    return object_id;
  }

  UA_NodeId addIntVariable(const char *name, UA_NodeId parent, UA_Int32 value) {
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", name);
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_INT32]);

    UA_NodeId node_id;
    auto status = UA_Server_addVariableNode(
      server_, UA_NODEID_NULL, parent,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
      UA_QUALIFIEDNAME(ns_index_, const_cast<char*>(name)),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr, nullptr, &node_id);

    if (status != UA_STATUSCODE_GOOD) {
      UA_LocalizedText_clear(&attr.displayName);
      throw std::runtime_error("Failed to add variable node");
    }

    UA_LocalizedText_clear(&attr.displayName);
    return node_id;
  }

  void ensureConnection() {
    REQUIRE(utils::verifyEventHappenedInPollTime(
      5s,
      [&]() {
        auto logs = getLogs();
        return std::find_if(logs.begin(), logs.end(), [](const std::string& message) { return message.find("New DiscoveryUrl added") != std::string::npos;}) != logs.end();
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
  std::unordered_map<std::string, UA_NodeId> node_ids_;
};

}  // namespace org::apache::nifi::minifi::test
