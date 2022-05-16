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

#include <map>
#include <string>

#include "Connection.h"

namespace org::apache::nifi::minifi::state::response {

class ConnectionMonitor {
 public:
  void clearConnections() {
    connections_.clear();
  }

  void updateConnection(minifi::Connection* connection) {
    if (nullptr != connection) {
      connections_[connection->getUUIDStr()] = connection;
    }
  }

  virtual ~ConnectionMonitor() = default;

 protected:
  std::map<std::string, minifi::Connection*> connections_;
};

}  // namespace org::apache::nifi::minifi::state::response
