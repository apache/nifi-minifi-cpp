// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>

#include "utils/net/AsioSocketUtils.h"
#include "utils/expected.h"

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
bool getFlowStatus(const utils::net::SocketData& socket_data, const std::string& status_query, std::ostream &out);
std::expected<void, std::string> getDebugBundle(const utils::net::SocketData& socket_data, const std::filesystem::path& target_dir);

}  // namespace org::apache::nifi::minifi::controller
