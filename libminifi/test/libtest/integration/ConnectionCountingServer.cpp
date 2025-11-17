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
#include "ConnectionCountingServer.h"

#include <cinttypes>
#include <cassert>

#include "utils/Id.h"

namespace org::apache::nifi::minifi::test {

namespace details {

bool NumberedMethodResponder::handleGet(CivetServer*, struct mg_connection* conn) {
  sendNumberedMessage("GET", conn);
  return true;
}

bool NumberedMethodResponder::handlePost(CivetServer*, struct mg_connection* conn) {
  sendNumberedMessage("POST", conn);
  return true;
}

bool NumberedMethodResponder::handlePut(CivetServer*, struct mg_connection* conn) {
  sendNumberedMessage("PUT", conn);
  return true;
}

bool NumberedMethodResponder::handleHead(CivetServer*, struct mg_connection* conn) {
  sendNumberedMessage("HEAD", conn);
  return true;
}

void NumberedMethodResponder::sendNumberedMessage(std::string body, struct mg_connection* conn) {
  saveConnectionId(conn);
  body.append(std::to_string(response_id_));
  mg_printf(conn, "HTTP/1.1 200 OK\r\n");
  mg_printf(conn, "Content-length: %lu\r\n", body.length());
  mg_printf(conn, "Response-number: %" PRIu64 "\r\n", response_id_);
  mg_printf(conn, "\r\n");
  mg_printf(conn, body.data(), body.length());
  ++response_id_;
}

void NumberedMethodResponder::saveConnectionId(struct mg_connection* conn) {
  auto user_connection_data = reinterpret_cast<minifi::utils::SmallString<36>*>(mg_get_user_connection_data(conn));
  assert(user_connection_data);
  connections_.emplace(*user_connection_data);
}

bool ReverseBodyPostHandler::handlePost(CivetServer* /*server*/, struct mg_connection* conn) {
  saveConnectionId(conn);
  auto request_body = std::vector<char>(2048);
  const size_t read_size = mg_read(conn, request_body.data(), 2048);
  assert(read_size < 2048);
  std::string response_body{request_body.begin(), request_body.begin() + gsl::narrow<std::vector<char>::difference_type>(read_size)};
  std::ranges::reverse(response_body);
  mg_printf(conn, "HTTP/1.1 200 OK\r\n");
  mg_printf(conn, "Content-length: %zu\r\n", read_size);
  mg_printf(conn, "\r\n");
  mg_printf(conn, response_body.data(), read_size);

  return true;
}

void ReverseBodyPostHandler::saveConnectionId(struct mg_connection* conn) {
  auto user_connection_data = reinterpret_cast<minifi::utils::SmallString<36>*>(mg_get_user_connection_data(conn));
  connections_.emplace(*user_connection_data);
}

AddIdToUserConnectionData::AddIdToUserConnectionData() {
  init_connection = [](const struct mg_connection*, void** user_connection_data) -> int {
    auto id = new minifi::utils::SmallString<36>(minifi::utils::IdGenerator::getIdGenerator()->generate().to_string());
    *user_connection_data = reinterpret_cast<void*>(id);
    return 0;
  };

  connection_close = [](const struct mg_connection* conn) -> void {
    auto user_connection_data = reinterpret_cast<minifi::utils::SmallString<36>*>(mg_get_user_connection_data(conn));
    delete user_connection_data;  // NOLINT(cppcoreguidelines-owning-memory)
  };
}
}  // namespace details

ConnectionCountingServer::ConnectionCountingServer() {
  server_.addHandler("/method", numbered_method_responder_);
  server_.addHandler("/reverse", reverse_body_post_handler_);
}

std::string ConnectionCountingServer::getPort() {
  const auto& listening_ports = server_.getListeningPorts();
  assert(!listening_ports.empty());
  return std::to_string(listening_ports[0]);
}

}  // namespace org::apache::nifi::minifi::test
