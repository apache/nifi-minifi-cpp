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

#include <cinttypes>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <set>
#include <unordered_map>

#include "civetweb.h"
#include "CivetServer.h"
#include "concurrentqueue.h"
#include "rapidjson/document.h"
#include "utils/HTTPUtils.h"
#include "ServerAwareHandler.h"
#include "c2/C2Payload.h"
#include "properties/Configure.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

static std::atomic<int> transaction_id;
static std::atomic<int> transaction_id_output;

struct FlowObj {
  FlowObj() = default;

  FlowObj(FlowObj &&other) noexcept
      : total_size(other.total_size),
        attributes(std::move(other.attributes)),
        data(std::move(other.data))
  { }

  uint64_t total_size{0};
  std::map<std::string, std::string> attributes;
  std::vector<std::byte> data;
};

class SiteToSiteLocationResponder : public ServerAwareHandler {
 public:
  explicit SiteToSiteLocationResponder(bool isSecure)
      : isSecure(isSecure) {
  }
  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override;

 protected:
  bool isSecure;
};

class PeerResponder : public ServerAwareHandler {
 public:
  explicit PeerResponder(std::string base_url) {
    std::string scheme;
    REQUIRE(minifi::utils::parse_http_components(base_url, port, scheme, path));
  }

  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override;

 protected:
  std::string base_url;
  std::string port;
  std::string path;
};

class SiteToSiteBaseResponder : public ServerAwareHandler {
 public:
  explicit SiteToSiteBaseResponder(std::string base_url)
      : base_url(std::move(base_url)) {
  }

  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override;

 protected:
  std::string base_url;
};

class TransactionResponder : public ServerAwareHandler {
 public:
  explicit TransactionResponder(std::string base_url, std::string port_id, bool input_port, bool wrong_uri = false, bool empty_transaction_uri = false);

  bool handlePost(CivetServer* /*server*/, struct mg_connection *conn) override;

  void setFeed(moodycamel::ConcurrentQueue<std::shared_ptr<FlowObj>> *feed) {
    flow_files_feed_ = feed;
  }

  std::string getTransactionId() {
    return transaction_id_str;
  }

 protected:
  std::string base_url;
  std::string transaction_id_str;
  bool wrong_uri;
  bool empty_transaction_uri;
  bool input_port;
  std::string port_id;
  moodycamel::ConcurrentQueue<std::shared_ptr<FlowObj>> *flow_files_feed_;
};

class FlowFileResponder : public ServerAwareHandler {
 public:
  explicit FlowFileResponder(bool input_port, bool wrong_uri = false, bool invalid_checksum = false)
      : wrong_uri(wrong_uri),
        input_port(input_port),
        invalid_checksum(invalid_checksum),
        flow_files_feed_(nullptr) {
  }

  moodycamel::ConcurrentQueue<std::shared_ptr<FlowObj>> *getFlows() {
    return &flow_files_;
  }

  void setFeed(moodycamel::ConcurrentQueue<std::shared_ptr<FlowObj>> *feed) {
    flow_files_feed_ = feed;
  }

  bool handlePost(CivetServer* /*server*/, struct mg_connection *conn) override;
  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override;

  void setFlowUrl(std::string flowUrl) {
    base_url = std::move(flowUrl);
  }

 protected:
  // base url
  std::string base_url;
  // set the wrong url
  bool wrong_uri;
  // we are running an input port
  bool input_port;
  // invalid checksum is returned.
  bool invalid_checksum;
  moodycamel::ConcurrentQueue<std::shared_ptr<FlowObj>> flow_files_;
  moodycamel::ConcurrentQueue<std::shared_ptr<FlowObj>> *flow_files_feed_;
};

class DeleteTransactionResponder : public ServerAwareHandler {
 public:
  explicit DeleteTransactionResponder(std::string base_url, std::string response_code, int expected_resp_code)
      : flow_files_feed_(nullptr),
        base_url(std::move(base_url)),
        response_code(std::move(response_code)) {
    expected_resp_code_str = std::to_string(expected_resp_code);
  }

  explicit DeleteTransactionResponder(std::string base_url, std::string response_code, moodycamel::ConcurrentQueue<std::shared_ptr<FlowObj>> *feed)
      : flow_files_feed_(feed),
        base_url(std::move(base_url)),
        response_code(std::move(response_code)) {
  }

  bool handleDelete(CivetServer* /*server*/, struct mg_connection *conn) override;

  void setFeed(moodycamel::ConcurrentQueue<std::shared_ptr<FlowObj>> *feed) {
    flow_files_feed_ = feed;
  }

 protected:
  moodycamel::ConcurrentQueue<std::shared_ptr<FlowObj>> *flow_files_feed_;
  std::string base_url;
  std::string expected_resp_code_str;
  std::string response_code;
};

class HeartbeatHandler : public ServerAwareHandler {
 public:
  explicit HeartbeatHandler(std::shared_ptr<minifi::Configure> configuration) : configuration_(std::move(configuration)) {}

  virtual void handleHeartbeat(const rapidjson::Document& root, struct mg_connection *) {
    verifyJsonHasAgentManifest(root);
  }

  virtual void handleAcknowledge(const rapidjson::Document&) {
  }

  bool handlePost(CivetServer *, struct mg_connection *conn) override {
    verify(conn);
    return true;
  }

 protected:
  struct C2Operation {
    std::string operation;
    std::string operand;
    std::string operation_id;
    std::unordered_map<std::string, c2::C2Value> args;
  };

  static void sendHeartbeatResponse(const std::string& operation, const std::string& operand, const std::string& operation_id, struct mg_connection* conn,
      const std::unordered_map<std::string, c2::C2Value>& args = {}) {
    sendHeartbeatResponse({{operation, operand, operation_id, args}}, conn);
  }

  static void sendHeartbeatResponse(const std::vector<C2Operation>& operations, struct mg_connection * conn);
  void verifyJsonHasAgentManifest(const rapidjson::Document& root, const std::vector<std::string>& verify_components = {}, const std::vector<std::string>& disallowed_properties = {});
  void verify(struct mg_connection *conn);

 private:
  using Metadata = std::unordered_map<std::string, std::vector<std::unordered_map<std::string, std::string>>>;

  static std::set<std::string> getOperandsOfProperties(const rapidjson::Value& operation_node);
  static void verifyMetadata(const rapidjson::Value& operation_node, const std::unordered_map<std::string, Metadata>& operand_with_metadata);

  template<typename T>
  void verifyOperands(const rapidjson::Value& operation_node, const std::unordered_map<std::string, Metadata>& operand_with_metadata = {}) {
    auto operands = getOperandsOfProperties(operation_node);
    REQUIRE(operands == std::set<std::string>(magic_enum::enum_names<T>().begin(), magic_enum::enum_names<T>().end()));
    verifyMetadata(operation_node, operand_with_metadata);
  }

  void verifyProperties(const rapidjson::Value& operation_node, minifi::c2::Operation operation,
      const std::vector<std::string>& verify_components, const std::vector<std::string>& disallowed_properties);
  void verifySupportedOperations(const rapidjson::Document& root, const std::vector<std::string>& verify_components, const std::vector<std::string>& disallowed_properties);

  std::shared_ptr<minifi::Configure> configuration_;
};

class StoppingHeartbeatHandler : public HeartbeatHandler {
 public:
  explicit StoppingHeartbeatHandler(std::shared_ptr<minifi::Configure> configuration) : HeartbeatHandler(std::move(configuration)) {}

  void handleHeartbeat(const rapidjson::Document& root, struct mg_connection *conn) override {
    HeartbeatHandler::handleHeartbeat(root, conn);
    sendStartStopOperation(conn);
  }

 protected:
  void sendStartStopOperation(struct mg_connection *conn);

 private:
  std::mutex start_stop_send_mutex_;
  uint32_t post_count_{0};
};

class C2FlowProvider : public ServerAwareHandler {
 public:
  explicit C2FlowProvider(std::string test_file_location)
      : test_file_location_(std::move(test_file_location)) {
  }

  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override;

 private:
  const std::string test_file_location_;
};

class C2UpdateHandler : public C2FlowProvider {
 public:
  using C2FlowProvider::C2FlowProvider;

  bool handlePost(CivetServer* /*server*/, struct mg_connection *conn) override;
  void setC2RestResponse(const std::string& url, const std::string& name, const std::optional<std::string>& persist = {});

  size_t getCallCount() const {
    return calls_;
  }

 protected:
  std::atomic<size_t> calls_{0};

 private:
  std::string response_;
};

class C2FailedUpdateHandler : public C2UpdateHandler {
 public:
  explicit C2FailedUpdateHandler(const std::string& test_file_location) : C2UpdateHandler(test_file_location) {
  }

  bool handlePost(CivetServer *server, struct mg_connection *conn) override;
};

class InvokeHTTPCouldNotConnectHandler : public ServerAwareHandler {
};

class InvokeHTTPResponseOKHandler : public ServerAwareHandler {
 public:
  bool handlePost(CivetServer *, struct mg_connection *conn) override {
    mg_printf(conn, "HTTP/1.1 201 OK\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    return true;
  }
};

class InvokeHTTPRedirectHandler : public ServerAwareHandler {
 public:
  bool handlePost(CivetServer *, struct mg_connection *conn) override {
    mg_printf(conn, "HTTP/1.1 301 OK\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nLocation: /\r\n\r\n");
    return true;
  }
};

class InvokeHTTPResponse404Handler : public ServerAwareHandler {
 public:
  bool handlePost(CivetServer *, struct mg_connection *conn) override {
    mg_printf(conn, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    return true;
  }
};

class InvokeHTTPResponse501Handler : public ServerAwareHandler {
 public:
  bool handlePost(CivetServer *, struct mg_connection *conn) override {
    mg_printf(conn, "HTTP/1.1 501 Not Implemented\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    return true;
  }
};

class TimeoutingHTTPHandler : public ServerAwareHandler {
 public:
  explicit TimeoutingHTTPHandler(std::vector<std::chrono::milliseconds> wait_times)
      : wait_times_(wait_times) {
  }
  bool handlePost(CivetServer *, struct mg_connection *conn) override {
    respond(conn);
    return true;
  }
  bool handleGet(CivetServer *, struct mg_connection *conn) override {
    respond(conn);
    return true;
  }
  bool handleDelete(CivetServer *, struct mg_connection *conn) override {
    respond(conn);
    return true;
  }
  bool handlePut(CivetServer *, struct mg_connection *conn) override {
    respond(conn);
    return true;
  }

 private:
  void respond(struct mg_connection *conn);

  std::vector<std::chrono::milliseconds> wait_times_;
};

class HttpGetResponder : public ServerAwareHandler {
 public:
  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override;
};

class RetryHttpGetResponder : public ServerAwareHandler {
 public:
  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override {
    puts("handle get with retry");
    mg_printf(conn, "HTTP/1.1 501 Not Implemented\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    return true;
  }
};

class C2AcknowledgeHandler : public ServerAwareHandler {
  struct OpResult {
    std::string state;
    std::string details;
  };

 public:
  bool handlePost(CivetServer* /*server*/, struct mg_connection* conn) override;

  bool isAcknowledged(const std::string& operation_id) const {
    std::lock_guard<std::mutex> guard(ack_operations_mtx_);
    return acknowledged_operations_.count(operation_id) > 0;
  }

  std::optional<OpResult> getState(const std::string& operation_id) const {
    std::lock_guard<std::mutex> guard(ack_operations_mtx_);
    if (auto it = acknowledged_operations_.find(operation_id); it != acknowledged_operations_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  uint32_t getApplyCount(const std::string& result_state) const {
    std::lock_guard<std::mutex> guard(apply_count_mtx_);
    return apply_count_.find(result_state) != apply_count_.end() ? apply_count_.at(result_state) : 0;
  }

 private:
  mutable std::mutex ack_operations_mtx_;
  mutable std::mutex apply_count_mtx_;
  std::unordered_map<std::string, OpResult> acknowledged_operations_;
  std::unordered_map<std::string, uint32_t> apply_count_;
};

}  // namespace org::apache::nifi::minifi::test
