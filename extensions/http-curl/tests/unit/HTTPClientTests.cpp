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

#include <utility>
#include <vector>
#include <string>
#include "TestBase.h"
#include "Catch.h"
#include "client/HTTPClient.h"
#include "CivetServer.h"
#include "ConnectionCountingServer.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::curl::testing {

TEST_CASE("HTTPClientTestChunkedResponse", "[basic]") {
  LogTestController::getInstance().setDebug<HTTPClient>();

  class Responder : public CivetHandler {
   public:
    static void send_response(struct mg_connection *conn) {
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

    bool handleGet(CivetServer* /*server*/, struct mg_connection* conn) override {
      send_response(conn);
      return true;
    }

    bool handlePost(CivetServer* /*server*/, struct mg_connection* conn) override {
      mg_printf(conn, "HTTP/1.1 100 Continue\r\n\r\n");

      std::array<uint8_t, 16384U> buf;
      while (mg_read(conn, buf.data(), buf.size()) > 0) {}

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
  REQUIRE(1 == vec.size());
  const std::string port = std::to_string(vec.at(0));

  HTTPClient client;
  client.initialize("GET", "http://localhost:" + port + "/testytesttest", nullptr);

  REQUIRE(client.submit());

  const auto& headers = client.getResponseHeaderMap();
  REQUIRE("whatever" == headers.at("X-Custom-Test"));

  const std::vector<char>& response = client.getResponseBody();
  REQUIRE("foobar\r\nbuzz" == std::string(response.begin(), response.end()));

  LogTestController::getInstance().reset();
}

TEST_CASE("HTTPClient escape test") {
  HTTPClient client;
  CHECK(client.escape("Hello Günter") == "Hello%20G%C3%BCnter");
  CHECK(client.escape("шеллы") == "%D1%88%D0%B5%D0%BB%D0%BB%D1%8B");
}

TEST_CASE("HTTPClient isValidHttpHeaderField test") {
  CHECK_FALSE(HTTPClient::isValidHttpHeaderField(""));
  CHECK(HTTPClient::isValidHttpHeaderField("valid"));
  CHECK_FALSE(HTTPClient::isValidHttpHeaderField(" "));
  CHECK_FALSE(HTTPClient::isValidHttpHeaderField(std::string("invalid") + static_cast<char>(11) + "character"));
  CHECK_FALSE(HTTPClient::isValidHttpHeaderField(std::string("invalid") + static_cast<char>(128) + "character"));
  CHECK_FALSE(HTTPClient::isValidHttpHeaderField("contains:invalid"));
}

TEST_CASE("HTTPClient replaceInvalidCharactersInHttpHeaderFieldName test") {
  CHECK(HTTPClient::replaceInvalidCharactersInHttpHeaderFieldName("") == "X-MiNiFi-Empty-Attribute-Name");
  CHECK(HTTPClient::replaceInvalidCharactersInHttpHeaderFieldName("valid") == "valid");
  CHECK(HTTPClient::replaceInvalidCharactersInHttpHeaderFieldName(" ") == "-");
  CHECK(HTTPClient::replaceInvalidCharactersInHttpHeaderFieldName(std::string("invalid") + static_cast<char>(11) + "character") == "invalid-character");
  CHECK(HTTPClient::replaceInvalidCharactersInHttpHeaderFieldName(std::string("invalid") + static_cast<char>(128) + "character") == "invalid-character");
  CHECK(HTTPClient::replaceInvalidCharactersInHttpHeaderFieldName("contains:invalid") == "contains-invalid");
}

TEST_CASE("HTTPClient should be reusable", "[basic]") {
  LogTestController::getInstance().setDebug<HTTPClient>();

  ConnectionCountingServer keep_alive_server_;

  HTTPClient client;
  client.setKeepAliveProbe(KeepAliveProbeData{2s, 2s});
  client.initialize("GET", "http://localhost:" + keep_alive_server_.getPort() + "/method", nullptr);

  std::vector<std::string> methods = {"GET", "GET", "POST", "GET", "GET", "POST", "POST", "POST"};
  uint64_t request_number = 0;
  for (const auto& method: methods) {
    client.set_request_method(method);
    REQUIRE(client.submit());
    const auto& headers = client.getResponseHeaderMap();
    REQUIRE(headers.contains("Response-number"));
    CHECK(std::to_string(request_number) == headers.at("Response-number"));
    const auto& response = client.getResponseBody();
    CHECK(method + std::to_string(request_number) == std::string(response.begin(), response.end()));
    ++request_number;
  }

  CHECK(keep_alive_server_.getConnectionCounter() == 1);
  LogTestController::getInstance().reset();
}

#ifdef __linux__
TEST_CASE("SSL without SSLContextService", "[HTTPClient]") {
  HTTPClient client;
  client.initialize("GET", "https://apache.org", nullptr);
  REQUIRE(client.submit());
}
#endif

}  // namespace org::apache::nifi::minifi::extensions::curl::testing
