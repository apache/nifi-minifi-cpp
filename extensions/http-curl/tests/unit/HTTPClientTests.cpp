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

#include <fstream>
#include <map>
#include <memory>
#include <utility>
#include <string>
#include <set>
#include "FlowController.h"
#include "io/BaseStream.h"
#include "TestBase.h"
#include "processors/GetFile.h"
#include "core/Core.h"
#include "client/HTTPClient.h"
#include "CivetServer.h"

TEST_CASE("HTTPClientTestChunkedResponse", "[basic]") {
  LogTestController::getInstance().setDebug<utils::HTTPClient>();

  class Responder : public CivetHandler {
   public:
    void send_response(struct mg_connection *conn) {
      mg_printf(conn, "HTTP/1.1 200 OK\r\n");
      mg_printf(conn, "Content-Type: application/octet-stream\r\n");
      mg_printf(conn, "Transfer-Encoding: chunked\r\n");
      mg_printf(conn, "X-Custom-Test: whatever\r\n");
      mg_printf(conn, "\r\n");
      mg_send_chunk(conn, "foo", 3U);
      mg_send_chunk(conn, "bar\r\n", 5U);
      mg_send_chunk(conn, "buzz", 4U);
      mg_send_chunk(conn, nullptr, 0U);
    }

    bool handleGet(CivetServer *server, struct mg_connection *conn) {
      send_response(conn);
      return true;
    }

    bool handlePost(CivetServer *server, struct mg_connection *conn) {
      mg_printf(conn, "HTTP/1.1 100 Continue\r\n\r\n");

      std::array<uint8_t, 16384U> buf;
      while (mg_read(conn, buf.data(), buf.size()) > 0);

      send_response(conn);
      return true;
    }
  };

  std::vector<std::string> options;
  options.emplace_back("enable_keep_alive");
  options.emplace_back("yes");
  options.emplace_back("keep_alive_timeout_ms");
  options.emplace_back("15000");
  options.emplace_back("num_threads");
  options.emplace_back("1");
  options.emplace_back("listening_ports");
  options.emplace_back("0");

  CivetServer server(options);
  Responder responder;
  server.addHandler("**", responder);
  const auto& vec = server.getListeningPorts();
  REQUIRE(1U == vec.size());
  const std::string port = std::to_string(vec.at(0));

  utils::HTTPClient client;
  client.initialize("GET", "http://localhost:" + port + "/testytesttest");

  REQUIRE(client.submit());

  const auto& headers = client.getParsedHeaders();
  REQUIRE("whatever" == headers.at("X-Custom-Test"));

  const std::vector<char>& response = client.getResponseBody();
  REQUIRE("foobar\r\nbuzz" == std::string(response.begin(), response.end()));

  LogTestController::getInstance().reset();
}
