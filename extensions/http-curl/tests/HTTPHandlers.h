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

#include <algorithm>
#include <cinttypes>
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "civetweb.h"
#include "CivetServer.h"
#include "concurrentqueue.h"
#include "CivetStream.h"
#include "io/CRCStream.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "HTTPUtils.h"
#include "ServerAwareHandler.h"
#include "utils/gsl.h"

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
  std::vector<uint8_t> data;
};

class SiteToSiteLocationResponder : public ServerAwareHandler {
 public:
  explicit SiteToSiteLocationResponder(bool isSecure)
      : isSecure(isSecure) {
  }
  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override {
    std::string site2site_rest_resp = "{"
        "\"revision\": {"
        "\"clientId\": \"483d53eb-53ec-4e93-b4d4-1fc3d23dae6f\""
        "},"
        "\"controller\": {"
        "\"id\": \"fe4a3a42-53b6-4af1-a80d-6fdfe60de97f\","
        "\"name\": \"NiFi Flow\","
        "\"siteToSiteSecure\": ";
    site2site_rest_resp += (isSecure ? "true" : "false");
    site2site_rest_resp += "}}";
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
              "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
              site2site_rest_resp.length());
    mg_printf(conn, "%s", site2site_rest_resp.c_str());
    return true;
  }

 protected:
  bool isSecure;
};

class PeerResponder : public ServerAwareHandler {
 public:
  explicit PeerResponder(std::string base_url) {
    (void)base_url;  // unused in release builds
    std::string scheme;
    assert(parse_http_components(base_url, port, scheme, path));
  }

  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override {
#ifdef WIN32
    std::string hostname = org::apache::nifi::minifi::io::Socket::getMyHostName();
#else
    std::string hostname = "localhost";
#endif
    std::string site2site_rest_resp = "{\"peers\" : [{ \"hostname\": \"" + hostname + "\", \"port\": " + port + ",  \"secure\": false, \"flowFileCount\" : 0 }] }";
    std::stringstream headers;
    headers << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " << site2site_rest_resp.length() << "\r\nConnection: close\r\n\r\n";
    mg_printf(conn, "%s", headers.str().c_str());
    mg_printf(conn, "%s", site2site_rest_resp.c_str());
    return true;
  }

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

  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override {
    std::string site2site_rest_resp =
        "{\"controller\":{\"id\":\"96dab149-0162-1000-7924-ed3122d6ea2b\",\"name\":\"NiFi Flow\",\"comments\":\"\",\"runningCount\":3,\"stoppedCount\":6,\"invalidCount\":1,\"disabledCount\":0,\"inputPortCount\":1,\"outputPortCount\":1,\"remoteSiteListeningPort\":10443,\"siteToSiteSecure\":false,\"instanceId\":\"13881505-0167-1000-be72-aa29341a3e9a\",\"inputPorts\":[{\"id\":\"471deef6-2a6e-4a7d-912a-81cc17e3a204\",\"name\":\"RPGIN\",\"comments\":\"\",\"state\":\"RUNNING\"}],\"outputPorts\":[{\"id\":\"9cf15a63-0166-1000-1b29-027406d96013\",\"name\":\"ddsga\",\"comments\":\"\",\"state\":\"STOPPED\"}]}}";  // NOLINT line length
    std::stringstream headers;
    headers << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " << site2site_rest_resp.length() << "\r\nConnection: close\r\n\r\n";
    mg_printf(conn, "%s", headers.str().c_str());
    mg_printf(conn, "%s", site2site_rest_resp.c_str());
    return true;
  }

 protected:
  std::string base_url;
};

class TransactionResponder : public ServerAwareHandler {
 public:
  explicit TransactionResponder(std::string base_url, std::string port_id, bool input_port, bool wrong_uri = false, bool empty_transaction_uri = false)
      : base_url(std::move(base_url)),
        wrong_uri(wrong_uri),
        empty_transaction_uri(empty_transaction_uri),
        input_port(input_port),
        port_id(std::move(port_id)),
        flow_files_feed_(nullptr) {
    if (input_port) {
      transaction_id_str = "fe4a3a42-53b6-4af1-a80d-6fdfe60de96";
      transaction_id_str += std::to_string(transaction_id.load());
      transaction_id++;
    } else {
      transaction_id_str = "fe4a3a42-53b6-4af1-a80d-6fdfe60de95";
      transaction_id_str += std::to_string(transaction_id_output.load());
      transaction_id_output++;
    }
  }

  bool handlePost(CivetServer* /*server*/, struct mg_connection *conn) override {
    std::string site2site_rest_resp;
    std::stringstream headers;
    headers << "HTTP/1.1 201 OK\r\nContent-Type: application/json\r\nContent-Length: " << site2site_rest_resp.length() << "\r\nX-Location-Uri-Intent: ";
    if (wrong_uri)
      headers << "ohstuff\r\n";
    else
      headers << "transaction-url\r\n";

    std::string port_type;

    if (input_port)
      port_type = "input-ports";
    else
      port_type = "output-ports";
    if (!empty_transaction_uri)
      headers << "locAtion: " << base_url << "/site-to-site/" << port_type << "/" << port_id << "/transactions/" << transaction_id_str << "\r\n";
    headers << "Connection: close\r\n\r\n";
    mg_printf(conn, "%s", headers.str().c_str());
    mg_printf(conn, "%s", site2site_rest_resp.c_str());
    return true;
  }

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

  bool handlePost(CivetServer* /*server*/, struct mg_connection *conn) override {
    std::string site2site_rest_resp;
    std::stringstream headers;

    if (!wrong_uri) {
      minifi::io::CivetStream civet_stream(conn);
      minifi::io::CRCStream < minifi::io::CivetStream > stream(gsl::make_not_null(&civet_stream));
      uint32_t num_attributes = 0;
      uint64_t total_size = 0;
      {
        const auto read = stream.read(num_attributes);
        if (!isServerRunning()) return false;
        assert(read > 0);
        total_size += read;
      }

      const auto flow = std::make_shared<FlowObj>();

      for (uint32_t i = 0; i < num_attributes; i++) {
        std::string name, value;
        {
          const auto read = stream.read(name, true);
          if (!isServerRunning()) return false;
          assert(read > 0);
          total_size += read;
        }
        {
          const auto read = stream.read(value, true);
          if (!isServerRunning()) return false;
          assert(read > 0);
          total_size += read;
        }
        flow->attributes[name] = value;
      }
      uint64_t length;
      {
        const auto read = stream.read(length);
        if (!isServerRunning()) return false;
        assert(read > 0);
        total_size += read;
      }

      total_size += length;
      flow->data.resize(gsl::narrow<size_t>(length));
      flow->total_size = total_size;

      {
        const auto read = stream.read(flow->data.data(), length);
        if (!isServerRunning()) return false;
        (void)read;
        assert(read == length);
      }

      if (!invalid_checksum) {
        site2site_rest_resp = std::to_string(stream.getCRC());
        flow_files_.enqueue(flow);
      } else {
        site2site_rest_resp = "Imawrongchecksumshortandstout";
      }

      headers << "HTTP/1.1 202 OK\r\nContent-Type: application/json\r\nContent-Length: " << site2site_rest_resp.length() << "\r\nConnection: close\r\n\r\n";
    } else {
      headers << "HTTP/1.1 404\r\nConnection: close\r\n\r\n";
    }

    mg_printf(conn, "%s", headers.str().c_str());
    mg_printf(conn, "%s", site2site_rest_resp.c_str());
    return true;
  }

  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override {
    if (flow_files_feed_->size_approx() > 0) {
      std::shared_ptr<FlowObj> flowobj;
      std::vector<std::shared_ptr<FlowObj>> flows;
      uint64_t total = 0;

      while (flow_files_feed_->try_dequeue(flowobj)) {
        flows.push_back(flowobj);
        total += flowobj->total_size;
      }
      mg_printf(conn,
          "HTTP/1.1 200 OK\r\n"
          "Content-Length: %" PRIu64 "\r\n"
          "Content-Type: application/octet-stream\r\n"
          "Connection: close\r\n\r\n",
          total);
      minifi::io::BufferStream serializer;
      minifi::io::CRCStream < minifi::io::BaseStream > stream(gsl::make_not_null(&serializer));
      for (const auto& flow : flows) {
        uint32_t num_attributes = gsl::narrow<uint32_t>(flow->attributes.size());
        stream.write(num_attributes);
        for (const auto& entry : flow->attributes) {
          stream.write(entry.first);
          stream.write(entry.second);
        }
        uint64_t length = flow->data.size();
        stream.write(length);
        stream.write(flow->data.data(), gsl::narrow<size_t>(length));
      }
    } else {
      mg_printf(conn, "HTTP/1.1 200 OK\r\nConnection: "
                "close\r\nContent-Length: 0\r\n");
      mg_printf(conn, "Content-Type: text/plain\r\n\r\n");
    }
    return true;
  }

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

  bool handleDelete(CivetServer* /*server*/, struct mg_connection *conn) override {
    std::string site2site_rest_resp;
    std::stringstream headers;
    std::string resp;
    CivetServer::getParam(conn, "responseCode", resp);
    headers << "HTTP/1.1 " << response_code << "\r\nContent-Type: application/json\r\nContent-Length: " << site2site_rest_resp.length() << "\r\n";
    headers << "Connection: close\r\n\r\n";
    mg_printf(conn, "%s", headers.str().c_str());
    mg_printf(conn, "%s", site2site_rest_resp.c_str());
    return true;
  }

  void setFeed(moodycamel::ConcurrentQueue<std::shared_ptr<FlowObj>> *feed) {
    flow_files_feed_ = feed;
  }

 protected:
  moodycamel::ConcurrentQueue<std::shared_ptr<FlowObj>> *flow_files_feed_;
  std::string base_url;
  std::string expected_resp_code_str;
  std::string response_code;
};

std::string readPayload(struct mg_connection *conn) {
  std::string response;
  int readBytes;

  std::array<char, 1024> buffer;
  while ((readBytes = mg_read(conn, buffer.data(), buffer.size())) > 0) {
    response.append(buffer.data(), readBytes);
  }
  return response;
}

class HeartbeatHandler : public ServerAwareHandler {
 public:
  void sendStopOperation(struct mg_connection *conn) {
    std::string resp = "{\"operation\" : \"heartbeat\", \"requested_operations\" : [{ \"operationid\" : 41, \"operation\" : \"stop\", \"operand\" : \"invoke\"  }, "
        "{ \"operationid\" : 42, \"operation\" : \"stop\", \"operand\" : \"FlowController\"  } ]}";
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
              "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
              resp.length());
    mg_printf(conn, "%s", resp.c_str());
  }

  void sendHeartbeatResponse(const std::string& operation, const std::string& operand, const std::string& operationId, struct mg_connection * conn) {
    std::string heartbeat_response = "{\"operation\" : \"heartbeat\",\"requested_operations\": [  {"
          "\"operation\" : \"" + operation + "\","
          "\"operationid\" : \"" + operationId + "\","
          "\"operand\": \"" + operand + "\"}]}";

      mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
                heartbeat_response.length());
      mg_printf(conn, "%s", heartbeat_response.c_str());
  }

  void verifyJsonHasAgentManifest(const rapidjson::Document& root) {
    bool found = false;
    assert(root.HasMember("agentInfo"));
    assert(root["agentInfo"].HasMember("agentManifest"));
    assert(root["agentInfo"]["agentManifest"].HasMember("bundles"));

    for (auto &bundle : root["agentInfo"]["agentManifest"]["bundles"].GetArray()) {
      assert(bundle.HasMember("artifact"));
      std::string str = bundle["artifact"].GetString();
      if (str == "minifi-standard-processors") {
        std::vector<std::string> classes;
        for (auto &proc : bundle["componentManifest"]["processors"].GetArray()) {
          classes.push_back(proc["type"].GetString());
        }

        auto group = minifi::BuildDescription::getClassDescriptions(str);
        for (const auto& proc : group.processors_) {
          assert(std::find(classes.begin(), classes.end(), proc.class_name_) != std::end(classes));
          found = true;
        }
      }
    }
    assert(found);
    (void)found;  // unused in release builds
  }

  virtual void handleHeartbeat(const rapidjson::Document& root, struct mg_connection *) {
    verifyJsonHasAgentManifest(root);
  }

  virtual void handleAcknowledge(const rapidjson::Document&) {
  }

  void verify(struct mg_connection *conn) {
    auto post_data = readPayload(conn);
    if (!isServerRunning()) {
      return;
    }
    if (!IsNullOrEmpty(post_data)) {
      rapidjson::Document root;
      rapidjson::ParseResult result = root.Parse(post_data.data(), post_data.size());
      if (!result) {
        throw std::runtime_error(fmt::format("JSON parse error: {0}\n JSON data: {1}", std::string(rapidjson::GetParseError_En(result.Code())), post_data));
      }
      std::string operation = root["operation"].GetString();
      if (operation == "heartbeat") {
        handleHeartbeat(root, conn);
      } else if (operation == "acknowledge") {
        handleAcknowledge(root);
      } else {
        throw std::runtime_error("operation not supported " + operation);
      }
    }
  }

  bool handlePost(CivetServer *, struct mg_connection *conn) override {
    verify(conn);
    sendStopOperation(conn);
    return true;
  }
};

class C2FlowProvider : public ServerAwareHandler {
 public:
  explicit C2FlowProvider(std::string test_file_location)
      : test_file_location_(std::move(test_file_location)) {
  }

  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override {
    std::ifstream myfile(test_file_location_.c_str(), std::ios::in | std::ios::binary);
    if (myfile.good()) {
      std::string str((std::istreambuf_iterator<char>(myfile)), (std::istreambuf_iterator<char>()));
      mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                      "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
                str.length());
      mg_printf(conn, "%s", str.c_str());
    } else {
      mg_printf(conn, "HTTP/1.1 500 Internal Server Error\r\n");
    }

    return true;
  }

 private:
  const std::string test_file_location_;
};

class C2UpdateHandler : public C2FlowProvider {
 public:
  using C2FlowProvider::C2FlowProvider;

  bool handlePost(CivetServer* /*server*/, struct mg_connection *conn) override {
    calls_++;
    if (!response_.empty()) {
      mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
                response_.length());
      mg_printf(conn, "%s", response_.c_str());
      response_.clear();
    } else {
      mg_printf(conn, "HTTP/1.1 500 Internal Server Error\r\n");
    }

    return true;
  }

  void setC2RestResponse(const std::string& url, const std::string& name, const utils::optional<std::string>& persist = {}) {
    std::string content = "{\"location\": \"" + url + "\"";
    if (persist) {
      content += ", \"persist\": \"" + *persist + "\"";
    }
    content += "}";
    response_ =
        "{\"operation\" : \"heartbeat\", "
          "\"requested_operations\": [  {"
            "\"operation\" : \"update\", "
            "\"operationid\" : \"8675309\", "
            "\"name\": \"" + name + "\", "
            "\"content\": " + content + "}]}";
  }

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

  bool handlePost(CivetServer *server, struct mg_connection *conn) override {
    calls_++;
    const auto data = readPayload(conn);

    if (data.find("operationState") != std::string::npos) {
      assert(data.find("state\": \"NOT_APPLIED") != std::string::npos);
    }

    return C2UpdateHandler::handlePost(server, conn);
  }
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
  void respond(struct mg_connection *conn) {
    if (!wait_times_.empty() && wait_times_[0] > std::chrono::seconds(0)) {
      sleep_for(wait_times_[0]);
    }
    int chunk_count = std::max(static_cast<int>(wait_times_.size()) - 1, 0);
    mg_printf(conn, "HTTP/1.1 201 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", chunk_count);
    for (int chunkIdx = 0; chunkIdx < chunk_count; ++chunkIdx) {
      mg_printf(conn, "a");
      if (wait_times_[chunkIdx + 1].count() > 0) {
        sleep_for(wait_times_[chunkIdx + 1]);
      }
    }
  }
  std::vector<std::chrono::milliseconds> wait_times_;
};
