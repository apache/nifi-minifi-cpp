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
#include <string>
#include <utility>
#include <vector>
#include <CivetServer.h>
#include "integration/CivetLibrary.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch::test {

class MockElasticAuthHandler : public CivetAuthHandler {
 public:
  static constexpr const char* API_KEY = "VnVhQ2ZHY0JDZGJrUW0tZTVhT3g6dWkybHAyYXhUTm1zeWFrdzl0dk5udw";
  static constexpr const char* USERNAME = "elastic";
  static constexpr const char* PASSWORD = "elastic_password";

 private:
  bool authorize(CivetServer*, struct mg_connection* conn) override {
    const char* authHeader = mg_get_header(conn, "Authorization");
    if (authHeader == nullptr) {
      return false;
    }
    if (strcmp(authHeader, "Basic ZWxhc3RpYzplbGFzdGljX3Bhc3N3b3Jk") == 0)
      return true;
    if (strcmp(authHeader, "ApiKey VnVhQ2ZHY0JDZGJrUW0tZTVhT3g6dWkybHAyYXhUTm1zeWFrdzl0dk5udw") == 0)
      return true;
    return false;
  };
};

class BulkElasticHandler : public CivetHandler {
 public:
  void returnErrors(bool ret_errors) {
    ret_error_ = ret_errors;
  }

 private:
  rapidjson::Value addIndexSuccess(rapidjson::Document::AllocatorType& alloc) {
    rapidjson::Value item{rapidjson::kObjectType};
    rapidjson::Value operation{rapidjson::kObjectType};
    operation.AddMember("_index", "test", alloc);
    operation.AddMember("_id", "1", alloc);
    operation.AddMember("result", "created", alloc);
    item.AddMember("index", operation, alloc);
    return item;
  }

  rapidjson::Value addUpdateSuccess(rapidjson::Document::AllocatorType& alloc) {
    rapidjson::Value item{rapidjson::kObjectType};
    rapidjson::Value operation{rapidjson::kObjectType};
    operation.AddMember("_index", "test", alloc);
    operation.AddMember("_id", "1", alloc);
    operation.AddMember("result", "updated", alloc);
    item.AddMember("update", operation, alloc);
    return item;
  }

  rapidjson::Value addUpdateError(rapidjson::Document::AllocatorType& alloc) {
    rapidjson::Value item{rapidjson::kObjectType};
    rapidjson::Value operation{rapidjson::kObjectType};
    operation.AddMember("_index", "test", alloc);
    operation.AddMember("_id", "1", alloc);
    rapidjson::Value error{rapidjson::kObjectType};
    error.AddMember("type", "document_missing_exception", alloc);
    error.AddMember("reason", "[6]: document missing", alloc);
    error.AddMember("index_uuid", "aAsFqTI0Tc2W0LCWgPNrOA", alloc);
    error.AddMember("shard", "0", alloc);
    error.AddMember("index", "index", alloc);
    operation.AddMember("error", error, alloc);
    item.AddMember("update", operation, alloc);
    return item;
  }

  bool handlePost(CivetServer*, struct mg_connection* conn) override {
    char request[2048];
    size_t chars_read = mg_read(conn, request, 2048);

    std::vector<std::string> lines = utils::string::splitRemovingEmpty({request, chars_read}, "\n");
    rapidjson::Document response{rapidjson::kObjectType};
    response.AddMember("took", 30, response.GetAllocator());
    response.AddMember("errors", ret_error_, response.GetAllocator());
    response.AddMember("items", rapidjson::kArrayType, response.GetAllocator());
    auto& items = response["items"];
    for (const auto& line : lines) {
      rapidjson::Document line_json;
      line_json.Parse<rapidjson::kParseStopWhenDoneFlag>(line.data());
      if (!line_json.HasMember("index") && !line_json.HasMember("create") && !line_json.HasMember("update") && !line_json.HasMember("delete"))
        continue;


      rapidjson::Value item{rapidjson::kObjectType};
      rapidjson::Value operation{rapidjson::kObjectType};

      if (ret_error_) {
        items.PushBack(addUpdateError(response.GetAllocator()), response.GetAllocator());
      } else {
        if (line_json.HasMember("update"))
          items.PushBack(addUpdateSuccess(response.GetAllocator()), response.GetAllocator());
        else
          items.PushBack(addIndexSuccess(response.GetAllocator()), response.GetAllocator());
      }
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    response.Accept(writer);

    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
    mg_printf(conn, "Content-length: %lu", buffer.GetSize());
    mg_printf(conn, "\r\n\r\n");
    mg_printf(conn, "%s", buffer.GetString());
    return true;
  }

  bool ret_error_ = false;
};

class MockElastic {
 public:
  explicit MockElastic(std::string port) : port_(std::move(port)) {
    std::vector<std::string> options;
    options.emplace_back("listening_ports");
    options.emplace_back(port_);

    server_ = std::make_unique<CivetServer>(options, &callbacks_, &logger_);
    bulk_handler_ = std::make_unique<BulkElasticHandler>();
    server_->addHandler("/_bulk", *bulk_handler_);

    auth_handler_ = std::make_unique<MockElasticAuthHandler>();
    server_->addAuthHandler("/_bulk", *auth_handler_);
  }

  [[nodiscard]] const std::string& getPort() const {
    return port_;
  }

  void returnErrors(bool ret_errors) {
    bulk_handler_->returnErrors(ret_errors);
  }

 private:
  CivetLibrary lib_;
  std::string port_;
  std::unique_ptr<CivetServer> server_;
  std::unique_ptr<BulkElasticHandler> bulk_handler_;
  std::unique_ptr<MockElasticAuthHandler> auth_handler_;

  CivetCallbacks callbacks_;
  std::shared_ptr<org::apache::nifi::minifi::core::logging::Logger> logger_ = org::apache::nifi::minifi::core::logging::LoggerFactory<MockElastic>::getLogger();
};

}  // namespace org::apache::nifi::minifi::extensions::elasticsearch::test
