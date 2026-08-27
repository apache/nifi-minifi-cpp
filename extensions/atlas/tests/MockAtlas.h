/**
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
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <CivetServer.h>

#include "core/logging/LoggerFactory.h"
#include "integration/CivetLibrary.h"
#include "minifi-cpp/core/logging/Logger.h"

// A CivetWeb-based fake Atlas server that records requests and returns configurable
// responses. Tests configure it up front, drive AtlasClient against it, and then
// inspect the captured request bodies.
namespace org::apache::nifi::minifi::extensions::atlas::test {

// Represents one HTTP interaction captured by the mock. Bodies are stored verbatim
// so tests can parse them however they want (rapidjson, substring assertions).
struct CapturedRequest {
  std::string method;
  std::string uri;
  std::string body;
  std::optional<std::string> authorization;
};

// Small helper: read the entire request body off a mg_connection.
inline std::string readAllBody(mg_connection* conn) {
  std::string body;
  char buffer[4096];
  int chars_read = 0;
  while ((chars_read = mg_read(conn, buffer, sizeof(buffer))) > 0) {
    body.append(buffer, static_cast<size_t>(chars_read));
  }
  return body;
}

// Encodes a plain HTTP response with the given status/body onto a connection.
inline void writeResponse(mg_connection* conn, int status_code, const std::string& status_text, const std::string& body) {
  mg_printf(conn, "HTTP/1.1 %d %s\r\n", status_code, status_text.c_str());
  mg_printf(conn, "Content-Type: application/json\r\n");
  mg_printf(conn, "Content-Length: %zu\r\n\r\n", body.size());
  if (!body.empty()) {
    mg_printf(conn, "%s", body.c_str());
  }
}

// Handler that records every request path/body it sees and returns a configurable
// status + body. Thread-safe (Civet may dispatch on any of its worker threads).
class RecordingHandler : public CivetHandler {
 public:
  void setResponse(int status_code, std::string body, std::string status_text = "OK") {
    std::lock_guard lock{mutex_};
    status_code_ = status_code;
    status_text_ = std::move(status_text);
    body_ = std::move(body);
  }

  std::vector<CapturedRequest> requests() const {
    std::lock_guard lock{mutex_};
    return requests_;
  }

 private:
  bool handleAny(CivetServer*, mg_connection* conn, const std::string& method) {
    const auto* req_info = mg_get_request_info(conn);
    CapturedRequest req;
    req.method = method;
    req.uri = std::string{req_info->request_uri ? req_info->request_uri : ""};
    if (req_info->query_string) {
      req.uri += "?";
      req.uri += req_info->query_string;
    }
    if (const auto* auth = mg_get_header(conn, "Authorization")) {
      req.authorization = auth;
    }
    if (method == "POST" || method == "PUT") {
      req.body = readAllBody(conn);
    }
    {
      std::lock_guard lock{mutex_};
      requests_.push_back(std::move(req));
      writeResponse(conn, status_code_, status_text_, body_);
    }
    return true;
  }

  bool handlePost(CivetServer* s, mg_connection* conn) override { return handleAny(s, conn, "POST"); }
  bool handlePut(CivetServer* s, mg_connection* conn) override { return handleAny(s, conn, "PUT"); }
  bool handleGet(CivetServer* s, mg_connection* conn) override { return handleAny(s, conn, "GET"); }

  mutable std::mutex mutex_;
  int status_code_ = 200;
  std::string status_text_ = "OK";
  std::string body_;
  std::vector<CapturedRequest> requests_;
};

class MockAtlas {
 public:
  explicit MockAtlas(std::string port) : port_(std::move(port)) {
    std::vector<std::string> options{"listening_ports", port_};
    server_ = std::make_unique<CivetServer>(options, &callbacks_, &logger_);
    server_->addHandler("/api/atlas/v2/types/typedefs", types_handler_);
    server_->addHandler("/api/atlas/v2/entity/bulk", entities_handler_);
    server_->addHandler("/api/atlas/v2/entity/uniqueAttribute", lookup_handler_);
  }

  const std::string& port() const { return port_; }
  RecordingHandler& types() { return types_handler_; }
  RecordingHandler& entities() { return entities_handler_; }
  RecordingHandler& lookup() { return lookup_handler_; }

 private:
  CivetLibrary lib_;
  std::string port_;
  CivetCallbacks callbacks_{};
  std::unique_ptr<CivetServer> server_;
  RecordingHandler types_handler_;
  RecordingHandler entities_handler_;
  RecordingHandler lookup_handler_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<MockAtlas>::getLogger();
};

}  // namespace org::apache::nifi::minifi::extensions::atlas::test
