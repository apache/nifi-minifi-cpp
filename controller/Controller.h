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

#include "io/ClientSocket.h"

namespace org::apache::nifi::minifi::controller {

/**
 * Sends a single argument comment
 * @param socket socket unique ptr.
 * @param op operation to perform
 * @param value value to send
 */
bool sendSingleCommand(std::unique_ptr<io::Socket> socket, uint8_t op, const std::string& value);

/**
 * Stops a stopped component
 * @param socket socket unique ptr.
 * @param op operation to perform
 */
bool stopComponent(std::unique_ptr<io::Socket> socket, const std::string& component);

/**
 * Starts a previously stopped component.
 * @param socket socket unique ptr.
 * @param op operation to perform
 */
bool startComponent(std::unique_ptr<io::Socket> socket, const std::string& component);

/**
 * Clears a connection queue.
 * @param socket socket unique ptr.
 * @param op operation to perform
 */
bool clearConnection(std::unique_ptr<io::Socket> socket, const std::string& connection);

/**
 * Updates the flow to the provided file
 */
int updateFlow(std::unique_ptr<io::Socket> socket, std::ostream &out, const std::string& file);

/**
 * Lists connections which are full
 * @param socket socket ptr
 */
int getFullConnections(std::unique_ptr<io::Socket> socket, std::ostream &out);

/**
 * Prints the connection size for the provided connection.
 * @param socket socket ptr
 * @param connection connection whose size will be returned.
 */
int getConnectionSize(std::unique_ptr<io::Socket> socket, std::ostream &out, const std::string& connection);

int listComponents(std::unique_ptr<io::Socket> socket, std::ostream &out, bool show_header = true);
int listConnections(std::unique_ptr<io::Socket> socket, std::ostream &out, bool show_header = true);
int printManifest(std::unique_ptr<io::Socket> socket, std::ostream &out);

int getJstacks(std::unique_ptr<io::Socket> socket, std::ostream &out);

}  // namespace org::apache::nifi::minifi::controller
