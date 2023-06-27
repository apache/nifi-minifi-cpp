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

#include "utils/net/AsioSocketUtils.h"

namespace org::apache::nifi::minifi::controller {

bool sendSingleCommand(const utils::net::SocketData& socket_data, uint8_t op, const std::string& value);
bool stopComponent(const utils::net::SocketData& socket_data, const std::string& component);
bool startComponent(const utils::net::SocketData& socket_data, const std::string& component);
bool clearConnection(const utils::net::SocketData& socket_data, const std::string& connection);
bool updateFlow(const utils::net::SocketData& socket_data, std::ostream &out, const std::string& file);
bool getFullConnections(const utils::net::SocketData& socket_data, std::ostream &out);
bool getConnectionSize(const utils::net::SocketData& socket_data, std::ostream &out, const std::string& connection);
bool listComponents(const utils::net::SocketData& socket_data, std::ostream &out, bool show_header = true);
bool listConnections(const utils::net::SocketData& socket_data, std::ostream &out, bool show_header = true);
bool printManifest(const utils::net::SocketData& socket_data, std::ostream &out);
bool getJstacks(const utils::net::SocketData& socket_data, std::ostream &out);

}  // namespace org::apache::nifi::minifi::controller
