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


class MockSplunkHandler : public CivetHandler {
 public:
  explicit MockSplunkHandler(std::string token, std::function<void(const struct mg_request_info *request_info)>& assertions) : token_(std::move(token)), assertions_(assertions) {
  }

  enum HeaderResult {
    MissingAuth,
    InvalidAuth,
    MissingReqChannel,
    HeadersOk
  };

  bool handlePost(CivetServer*, struct mg_connection *conn) override {
    switch (checkHeaders(conn)) {
      case MissingAuth:
        return send401(conn);
      case InvalidAuth:
        return send403(conn);
      case MissingReqChannel:
        return send400(conn);
      case HeadersOk:
        return handlePostImpl(conn);
    }
    return false;
  }

  HeaderResult checkHeaders(struct mg_connection *conn) const {
    const struct mg_request_info* req_info = mg_get_request_info(conn);
    assertions_(req_info);
    auto auth_header = std::find_if(std::begin(req_info->http_headers),
                                    std::end(req_info->http_headers),
                                    [](auto header) -> bool {return strcmp(header.name, "Authorization") == 0;});
    if (auth_header == std::end(req_info->http_headers))
      return MissingAuth;
    if (strcmp(auth_header->value, token_.c_str()) != 0)
      return InvalidAuth;

    auto request_channel_header = std::find_if(std::begin(req_info->http_headers),
                                               std::end(req_info->http_headers),
                                               [](auto header) -> bool {return strcmp(header.name, "X-Splunk-Request-Channel") == 0;});

    if (request_channel_header == std::end(req_info->http_headers))
      return MissingReqChannel;
    return HeadersOk;
  }

  bool send400(struct mg_connection *conn) const {
    constexpr const char * body = "{\"text\":\"Data channel is missing\",\"code\":10}";
    mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n");
    mg_printf(conn, "Content-length: %lu", strlen(body));
    mg_printf(conn, "\r\n\r\n");
    mg_printf(conn, body);
    return true;
  }

  bool send401(struct mg_connection *conn) const {
    constexpr const char * body = "{\"text\":\"Token is required\",\"code\":2}";
    mg_printf(conn, "HTTP/1.1 401 Unauthorized\r\n");
    mg_printf(conn, "Content-length: %lu", strlen(body));
    mg_printf(conn, "\r\n\r\n");
    mg_printf(conn, body);
    return true;
  }

  bool send403(struct mg_connection *conn) const {
    constexpr const char * body = "{\"text\":\"Invalid token\",\"code\":4}";
    mg_printf(conn, "HTTP/1.1 403 Forbidden\r\n");
    mg_printf(conn, "Content-length: %lu", strlen(body));
    mg_printf(conn, "\r\n\r\n");
    mg_printf(conn, body);
    return true;
  }

 protected:
  virtual bool handlePostImpl(struct mg_connection *conn) = 0;
  std::string token_;
  std::function<void(const struct mg_request_info *request_info)>& assertions_;
};

class RawCollectorHandler : public MockSplunkHandler {
 public:
  explicit RawCollectorHandler(std::string token, std::function<void(const struct mg_request_info *request_info)>& assertions) : MockSplunkHandler(std::move(token), assertions) {}
 protected:
  bool handlePostImpl(struct mg_connection* conn) override {
    constexpr const char * body = "{\"text\":\"Success\",\"code\":0,\"ackId\":808}";
    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
    mg_printf(conn, "Content-length: %lu", strlen(body));
    mg_printf(conn, "\r\n\r\n");
    mg_printf(conn, body);
    return true;
  }
};

class AckIndexerHandler : public MockSplunkHandler {
 public:
  explicit AckIndexerHandler(std::string token, std::vector<uint64_t> indexed_events, std::function<void(const struct mg_request_info *request_info)>& assertions)
      : MockSplunkHandler(std::move(token), assertions), indexed_events_(indexed_events) {}

 protected:
  bool handlePostImpl(struct mg_connection* conn) override {
    std::vector<char> data;
    data.reserve(2048);
    mg_read(conn, data.data(), 2048);
    rapidjson::Document post_data;

    rapidjson::ParseResult parse_result = post_data.Parse<rapidjson::kParseStopWhenDoneFlag>(data.data());
    if (parse_result.IsError())
      return sendInvalidFormat(conn);
    if (!post_data.HasMember("acks") || !post_data["acks"].IsArray())
      return sendInvalidFormat(conn);
    std::vector<uint64_t> ids;
    for (auto& id : post_data["acks"].GetArray()) {
      ids.push_back(id.GetUint64());
    }
    rapidjson::Document reply = rapidjson::Document(rapidjson::kObjectType);
    reply.AddMember("acks", rapidjson::kObjectType, reply.GetAllocator());
    for (auto& id : ids) {
      rapidjson::Value key(std::to_string(id).c_str(), reply.GetAllocator());
      reply["acks"].AddMember(key, std::find(indexed_events_.begin(), indexed_events_.end(), id) != indexed_events_.end() ? true : false, reply.GetAllocator());
    }
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    reply.Accept(writer);

    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
    mg_printf(conn, "Content-length: %lu", buffer.GetSize());
    mg_printf(conn, "\r\n\r\n");
    mg_printf(conn, "%s" , buffer.GetString());
    return true;
  }

  bool sendInvalidFormat(struct mg_connection* conn) {
    constexpr const char * body = "{\"text\":\"Invalid data format\",\"code\":6}";
    mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n");
    mg_printf(conn, "Content-length: %lu", strlen(body));
    mg_printf(conn, "\r\n\r\n");
    mg_printf(conn, body);
    return true;
  }

  std::vector<uint64_t> indexed_events_;
};

class MockSplunkHEC {
 public:
  static constexpr const char* TOKEN = "Splunk 822f7d13-2b70-4f8c-848b-86edfc251222";

  static inline std::vector<uint64_t> indexed_events = {0, 1};

  explicit MockSplunkHEC(std::string port) : port_(std::move(port)) {
    std::vector<std::string> options;
    options.emplace_back("listening_ports");
    options.emplace_back(port_);
    server_.reset(new CivetServer(options, &callbacks_, &logger_));
    {
      MockSplunkHandler* raw_collector_handler = new RawCollectorHandler(TOKEN, assertions_);
      server_->addHandler("/services/collector/raw", raw_collector_handler);
      handlers_.emplace_back(std::move(raw_collector_handler));
    }
    {
      MockSplunkHandler* ack_indexer_handler = new AckIndexerHandler(TOKEN, indexed_events, assertions_);
      server_->addHandler("/services/collector/ack", ack_indexer_handler);
      handlers_.emplace_back(std::move(ack_indexer_handler));
    }
  }

  const std::string& getPort() const {
    return port_;
  }

  void setAssertions(std::function<void(const struct mg_request_info *request_info)> assertions) {
    assertions_ = assertions;
  }


 private:
  CivetLibrary lib_;
  std::string port_;
  std::unique_ptr<CivetServer> server_;
  std::vector<std::unique_ptr<MockSplunkHandler>> handlers_;
  CivetCallbacks callbacks_;
  std::function<void(const struct mg_request_info *request_info)> assertions_ = [](const struct mg_request_info*) {};
  std::shared_ptr<org::apache::nifi::minifi::core::logging::Logger> logger_ = org::apache::nifi::minifi::core::logging::LoggerFactory<MockSplunkHEC>::getLogger();
};
