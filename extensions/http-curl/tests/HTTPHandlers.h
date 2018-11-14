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
#include "civetweb.h"
#include "CivetServer.h"
#include "concurrentqueue.h"
#include "CivetStream.h"
#include "io/CRCStream.h"
#ifndef LIBMINIFI_TEST_CURL_TESTS_SITETOSITEHTTP_HTTPHANDLERS_H_
#define LIBMINIFI_TEST_CURL_TESTS_SITETOSITEHTTP_HTTPHANDLERS_H_
static std::atomic<int> transaction_id;
static std::atomic<int> transaction_id_output;

class FlowObj {
 public:
  FlowObj()
      : total_size(0) {

  }
  explicit FlowObj(const FlowObj &&other)
      : total_size(std::move(other.total_size)),
        attributes(std::move(other.attributes)),
        data(std::move(other.data)) {

  }
  uint64_t total_size;
  std::map<std::string, std::string> attributes;
  std::vector<uint8_t> data;

};

class SiteToSiteLocationResponder : public CivetHandler {
 public:
  explicit SiteToSiteLocationResponder(bool isSecure)
      : isSecure(isSecure) {
  }
  bool handleGet(CivetServer *server, struct mg_connection *conn) {
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

class PeerResponder : public CivetHandler {
 public:

  explicit PeerResponder(const std::string base_url)
      : base_url(base_url) {
  }

  bool handleGet(CivetServer *server, struct mg_connection *conn) {
    std::string site2site_rest_resp = "{\"peers\" : [{ \"hostname\": \"localhost\", \"port\": 8099,  \"secure\": false, \"flowFileCount\" : 0 }] }";
    std::stringstream headers;
    headers << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " << site2site_rest_resp.length() << "\r\nConnection: close\r\n\r\n";
    mg_printf(conn, "%s", headers.str().c_str());
    mg_printf(conn, "%s", site2site_rest_resp.c_str());
    return true;
  }

 protected:
  std::string base_url;
};

class SiteToSiteBaseResponder : public CivetHandler {
 public:

  explicit SiteToSiteBaseResponder(const std::string base_url)
      : base_url(base_url) {
  }

  bool handleGet(CivetServer *server, struct mg_connection *conn) {
    std::string site2site_rest_resp =
        "{\"controller\":{\"id\":\"96dab149-0162-1000-7924-ed3122d6ea2b\",\"name\":\"NiFi Flow\",\"comments\":\"\",\"runningCount\":3,\"stoppedCount\":6,\"invalidCount\":1,\"disabledCount\":0,\"inputPortCount\":1,\"outputPortCount\":1,\"remoteSiteListeningPort\":10443,\"siteToSiteSecure\":false,\"instanceId\":\"13881505-0167-1000-be72-aa29341a3e9a\",\"inputPorts\":[{\"id\":\"471deef6-2a6e-4a7d-912a-81cc17e3a204\",\"name\":\"RPGIN\",\"comments\":\"\",\"state\":\"RUNNING\"}],\"outputPorts\":[{\"id\":\"9cf15a63-0166-1000-1b29-027406d96013\",\"name\":\"ddsga\",\"comments\":\"\",\"state\":\"STOPPED\"}]}}";
    std::stringstream headers;
    headers << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " << site2site_rest_resp.length() << "\r\nConnection: close\r\n\r\n";
    mg_printf(conn, "%s", headers.str().c_str());
    mg_printf(conn, "%s", site2site_rest_resp.c_str());
    return true;
  }

 protected:
  std::string base_url;
};

class TransactionResponder : public CivetHandler {
 public:

  explicit TransactionResponder(const std::string base_url, std::string port_id, bool input_port, bool wrong_uri, bool empty_transaction_uri)
      : base_url(base_url),
        wrong_uri(wrong_uri),
        empty_transaction_uri(empty_transaction_uri),
        input_port(input_port),
        port_id(port_id),
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

  bool handlePost(CivetServer *server, struct mg_connection *conn) {
    std::string site2site_rest_resp = "";
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

class FlowFileResponder : public CivetHandler {
 public:

  explicit FlowFileResponder(bool input_port, bool wrong_uri, bool invalid_checksum)
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

  bool handlePost(CivetServer *server, struct mg_connection *conn) {
    std::string site2site_rest_resp = "";
    std::stringstream headers;

    if (!wrong_uri) {
      minifi::io::CivetStream civet_stream(conn);
      minifi::io::CRCStream < minifi::io::CivetStream > stream(&civet_stream);
      uint32_t num_attributes;
      uint64_t total_size = 0;
      total_size += stream.read(num_attributes);

      auto flow = std::make_shared<FlowObj>();

      for (int i = 0; i < num_attributes; i++) {
        std::string name, value;
        total_size += stream.readUTF(name, true);
        total_size += stream.readUTF(value, true);
        flow->attributes[name] = value;
      }
      uint64_t length;
      total_size += stream.read(length);

      total_size += length;
      flow->data.resize(length);
      flow->total_size = total_size;

      assert(stream.readData(flow->data.data(), length) == length);

      assert(flow->attributes["path"] == ".");
      assert(!flow->attributes["uuid"].empty());
      assert(!flow->attributes["filename"].empty());

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

  bool handleGet(CivetServer *server, struct mg_connection *conn) {

    if (flow_files_feed_->size_approx() > 0) {
      std::shared_ptr<FlowObj> flow;
      uint8_t buf[1];
      std::vector<std::shared_ptr<FlowObj>> flows;
      uint64_t total = 0;

      while (flow_files_feed_->try_dequeue(flow)) {
        flows.push_back(flow);
        total += flow->total_size;
      }
      mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                "Content-Length: %llu\r\n"
                "Content-Type: application/octet-stream\r\n"
                "Connection: close\r\n\r\n",
                total);
      minifi::io::BaseStream serializer;
      minifi::io::CRCStream < minifi::io::BaseStream > stream(&serializer);
      for (auto flow : flows) {
        uint32_t num_attributes = flow->attributes.size();
        stream.write(num_attributes);
        for (auto entry : flow->attributes) {
          stream.writeUTF(entry.first);
          stream.writeUTF(entry.second);
        }
        uint64_t length = flow->data.size();
        stream.write(length);
        stream.writeData(flow->data.data(), length);
      }
      auto ret = mg_write(conn, serializer.getBuffer(), total);
    } else {
      mg_printf(conn, "HTTP/1.1 200 OK\r\nConnection: "
                "close\r\nContent-Length: 0\r\n");
      mg_printf(conn, "Content-Type: text/plain\r\n\r\n");

    }
    return true;
  }

  void setFlowUrl(std::string flowUrl) {
    base_url = flowUrl;
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

class DeleteTransactionResponder : public CivetHandler {
 public:

  explicit DeleteTransactionResponder(const std::string base_url, std::string response_code, int expected_resp_code)
      : flow_files_feed_(nullptr),
        base_url(base_url),
        response_code(response_code) {
    expected_resp_code_str = std::to_string(expected_resp_code);
  }

  explicit DeleteTransactionResponder(const std::string base_url, std::string response_code, moodycamel::ConcurrentQueue<std::shared_ptr<FlowObj>> *feed)
      : flow_files_feed_(feed),
        base_url(base_url),
        response_code(response_code) {
  }

  bool handleDelete(CivetServer *server, struct mg_connection *conn) {

    std::string site2site_rest_resp = "";
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

#endif /* LIBMINIFI_TEST_CURL_TESTS_SITETOSITEHTTP_HTTPHANDLERS_H_ */
