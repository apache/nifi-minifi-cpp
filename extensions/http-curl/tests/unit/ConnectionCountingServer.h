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

namespace org::apache::nifi::minifi::extensions::curl::testing {

namespace details {

class NumberedMethodResponder : public CivetHandler {
 public:
  explicit NumberedMethodResponder(std::set<utils::SmallString<36>>& connections) : connections_(connections) {}

  bool handleGet(CivetServer*, struct mg_connection* conn) override {
    sendNumberedMessage("GET", conn);
    return true;
  }

  bool handlePost(CivetServer*, struct mg_connection* conn) override {
    sendNumberedMessage("POST", conn);
    return true;
  }

  bool handlePut(CivetServer*, struct mg_connection* conn) override {
    sendNumberedMessage("PUT", conn);
    return true;
  }

  bool handleHead(CivetServer*, struct mg_connection* conn) override {
    sendNumberedMessage("HEAD", conn);
    return true;
  }

 private:
  void sendNumberedMessage(std::string body, struct mg_connection* conn) {
    saveConnectionId(conn);
    body.append(std::to_string(response_id_));
    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
    mg_printf(conn, "Content-length: %lu\r\n", body.length());
    mg_printf(conn, "Response-number: %" PRIu64 "\r\n", response_id_);
    mg_printf(conn, "\r\n");
    mg_printf(conn, body.data(), body.length());
    ++response_id_;
  }

  void saveConnectionId(struct mg_connection* conn) {
    auto user_connection_data = reinterpret_cast<utils::SmallString<36>*>(mg_get_user_connection_data(conn));
    assert(user_connection_data);
    connections_.emplace(*user_connection_data);
  }

  uint64_t response_id_ = 0;
  std::set<utils::SmallString<36>>& connections_;
};

class ReverseBodyPostHandler : public CivetHandler {
 public:
  explicit ReverseBodyPostHandler(std::set<utils::SmallString<36>>& connections) : connections_(connections) {}

  bool handlePost(CivetServer* /*server*/, struct mg_connection* conn) override {
    saveConnectionId(conn);
    std::vector<char> request_body;
    request_body.reserve(2048);
    size_t read_size = mg_read(conn, request_body.data(), 2048);
    assert(read_size < 2048);
    std::string response_body{request_body.begin(), request_body.begin() + read_size};
    std::reverse(std::begin(response_body), std::end(response_body));
    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
    mg_printf(conn, "Content-length: %zu\r\n", read_size);
    mg_printf(conn, "\r\n");
    mg_printf(conn, response_body.data(), read_size);

    return true;
  }

 private:
  void saveConnectionId(struct mg_connection* conn) {
    auto user_connection_data = reinterpret_cast<utils::SmallString<36>*>(mg_get_user_connection_data(conn));
    connections_.emplace(*user_connection_data);
  }

  std::set<utils::SmallString<36>>& connections_;
};

struct AddIdToUserConnectionData : public CivetCallbacks {
  AddIdToUserConnectionData() {
    init_connection = [](const struct mg_connection*, void** user_connection_data) -> int {
      utils::SmallString<36>* id = new utils::SmallString<36>(utils::IdGenerator::getIdGenerator()->generate().to_string());
      *user_connection_data = reinterpret_cast<void*>(id);
      return 0;
    };

    connection_close = [](const struct mg_connection* conn) -> void {
      auto user_connection_data = reinterpret_cast<utils::SmallString<36>*>(mg_get_user_connection_data(conn));
      delete user_connection_data;
    };
  }
};
}  // namespace details

class ConnectionCountingServer {
 public:
  ConnectionCountingServer() {
    server_.addHandler("/method", numbered_method_responder_);
    server_.addHandler("/reverse", reverse_body_post_handler_);
  }

  size_t getConnectionCounter() { return connections_.size(); }

  std::string getPort() {
    const auto& listening_ports = server_.getListeningPorts();
    assert(!listening_ports.empty());
    return std::to_string(listening_ports[0]);
  }

 private:
  static inline std::vector<std::string> options = {
      "enable_keep_alive", "yes",
      "keep_alive_timeout_ms", "15000",
      "num_threads", "1",
      "listening_ports", "0"};

  std::set<utils::SmallString<36>> connections_;
  details::AddIdToUserConnectionData add_id_to_user_connection_data_;
  CivetServer server_{options, &add_id_to_user_connection_data_};
  details::ReverseBodyPostHandler reverse_body_post_handler_{connections_};
  details::NumberedMethodResponder numbered_method_responder_{connections_};
};

}  // namespace org::apache::nifi::minifi::extensions::curl::testing
