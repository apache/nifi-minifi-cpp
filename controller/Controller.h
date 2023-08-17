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
#pragma once

#include <memory>
#include <string>

#include "controllers/SSLContextService.h"

namespace org::apache::nifi::minifi::controller {

struct ControllerSocketData {
  std::string host = "localhost";
  int port = -1;
  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service;
};

bool sendSingleCommand(const ControllerSocketData& socket_data, uint8_t op, const std::string& value);
bool stopComponent(const ControllerSocketData& socket_data, const std::string& component);
bool startComponent(const ControllerSocketData& socket_data, const std::string& component);
bool clearConnection(const ControllerSocketData& socket_data, const std::string& connection);
bool updateFlow(const ControllerSocketData& socket_data, std::ostream &out, const std::string& file);
bool getFullConnections(const ControllerSocketData& socket_data, std::ostream &out);
bool getConnectionSize(const ControllerSocketData& socket_data, std::ostream &out, const std::string& connection);
bool listComponents(const ControllerSocketData& socket_data, std::ostream &out, bool show_header = true);
bool listConnections(const ControllerSocketData& socket_data, std::ostream &out, bool show_header = true);
bool printManifest(const ControllerSocketData& socket_data, std::ostream &out);
bool getJstacks(const ControllerSocketData& socket_data, std::ostream &out);

}  // namespace org::apache::nifi::minifi::controller
