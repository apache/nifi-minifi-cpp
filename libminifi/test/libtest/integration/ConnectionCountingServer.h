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

#include <array>
#include <vector>
#include <string>
#include <set>
#include "CivetServer.h"
#include "utils/SmallString.h"

namespace org::apache::nifi::minifi::test {

namespace details {

class NumberedMethodResponder : public CivetHandler {
 public:
  explicit NumberedMethodResponder(std::set<minifi::utils::SmallString<36>>& connections) : connections_(connections) {}

  bool handleGet(CivetServer*, struct mg_connection* conn) override;
  bool handlePost(CivetServer*, struct mg_connection* conn) override;
  bool handlePut(CivetServer*, struct mg_connection* conn) override;
  bool handleHead(CivetServer*, struct mg_connection* conn) override;

 private:
  void sendNumberedMessage(std::string body, struct mg_connection* conn);
  void saveConnectionId(struct mg_connection* conn);

  uint64_t response_id_ = 0;
  std::set<minifi::utils::SmallString<36>>& connections_;
};

class ReverseBodyPostHandler : public CivetHandler {
 public:
  explicit ReverseBodyPostHandler(std::set<minifi::utils::SmallString<36>>& connections) : connections_(connections) {}

  bool handlePost(CivetServer* /*server*/, struct mg_connection* conn) override;

 private:
  void saveConnectionId(struct mg_connection* conn);

  std::set<minifi::utils::SmallString<36>>& connections_;
};

struct AddIdToUserConnectionData : public CivetCallbacks {
  AddIdToUserConnectionData();
};
}  // namespace details

class ConnectionCountingServer {
 public:
  ConnectionCountingServer();

  size_t getConnectionCounter() { return connections_.size(); }

  std::string getPort();

 private:
  static inline std::vector<std::string> options = {
      "enable_keep_alive", "yes",
      "keep_alive_timeout_ms", "15000",
      "num_threads", "1",
      "listening_ports", "0"};

  std::set<minifi::utils::SmallString<36>> connections_;
  details::AddIdToUserConnectionData add_id_to_user_connection_data_;
  CivetServer server_{options, &add_id_to_user_connection_data_};
  details::ReverseBodyPostHandler reverse_body_post_handler_{connections_};
  details::NumberedMethodResponder numbered_method_responder_{connections_};
};

}  // namespace org::apache::nifi::minifi::test
