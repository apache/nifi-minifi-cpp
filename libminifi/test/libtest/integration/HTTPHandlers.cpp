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
#include "HTTPHandlers.h"

#include <algorithm>

#include "CivetStream.h"
#include "io/BufferStream.h"
#include "io/CRCStream.h"
#include "minifi-cpp/agent/agent_docs.h"
#include "minifi-cpp/agent/agent_version.h"
#include "minifi-cpp/utils/gsl.h"
#include "range/v3/algorithm/contains.hpp"
#include "range/v3/view/filter.hpp"
#include "range/v3/view/view.hpp"
#include "rapidjson/error/en.h"
#include "sitetosite/HttpSiteToSiteClient.h"
#include "utils/StringUtils.h"
#include "utils/net/DNS.h"

namespace org::apache::nifi::minifi::test {

bool SiteToSiteLocationResponder::handleGet(CivetServer* /*server*/, struct mg_connection *conn) {
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

bool PeerResponder::handleGet(CivetServer* /*server*/, struct mg_connection *conn) {
#ifdef WIN32
  std::string hostname = org::apache::nifi::minifi::utils::net::getMyHostName();
#else
  std::string hostname = "localhost";
#endif
  std::string site2site_rest_resp = R"({"peers" : [{ "hostname": ")" + hostname + R"(", "port": )" + port + R"(,  "secure": false, "flowFileCount" : 0 }] })";
  std::stringstream headers;
  headers << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " << site2site_rest_resp.length() << "\r\nConnection: close\r\n\r\n";
  mg_printf(conn, "%s", headers.str().c_str());
  mg_printf(conn, "%s", site2site_rest_resp.c_str());
  return true;
}

bool SiteToSiteBaseResponder::handleGet(CivetServer* /*server*/, struct mg_connection *conn) {
  std::string site2site_rest_resp =
      "{\"controller\":{\"id\":\"96dab149-0162-1000-7924-ed3122d6ea2b\",\"name\":\"NiFi Flow\",\"comments\":\"\",\"runningCount\":3,\"stoppedCount\":6,\"invalidCount\":1,\"disabledCount\":0,\"inputPortCount\":1,\"outputPortCount\":1,\"remoteSiteListeningPort\":10443,\"siteToSiteSecure\":false,\"instanceId\":\"13881505-0167-1000-be72-aa29341a3e9a\",\"inputPorts\":[{\"id\":\"471deef6-2a6e-4a7d-912a-81cc17e3a204\",\"name\":\"RPGIN\",\"comments\":\"\",\"state\":\"RUNNING\"}],\"outputPorts\":[{\"id\":\"9cf15a63-0166-1000-1b29-027406d96013\",\"name\":\"ddsga\",\"comments\":\"\",\"state\":\"STOPPED\"}]}}";  // NOLINT line length
  std::stringstream headers;
  headers << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " << site2site_rest_resp.length() << "\r\nConnection: close\r\n\r\n";
  mg_printf(conn, "%s", headers.str().c_str());
  mg_printf(conn, "%s", site2site_rest_resp.c_str());
  return true;
}

TransactionResponder::TransactionResponder(std::string base_url, std::string port_id, bool input_port, bool wrong_uri, bool empty_transaction_uri)
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

bool TransactionResponder::handlePost(CivetServer* /*server*/, struct mg_connection *conn) {
  auto req_info = mg_get_request_info(conn);
  static const std::unordered_map<std::string, std::string> expected_headers {
    {std::string{minifi::sitetosite::HttpSiteToSiteClient::PROTOCOL_VERSION_HEADER}, "1"},
    {std::string{minifi::sitetosite::HttpSiteToSiteClient::HANDSHAKE_PROPERTY_USE_COMPRESSION}, "false"},
    {std::string{minifi::sitetosite::HttpSiteToSiteClient::HANDSHAKE_PROPERTY_REQUEST_EXPIRATION}, "20000"},
    {std::string{minifi::sitetosite::HttpSiteToSiteClient::HANDSHAKE_PROPERTY_BATCH_COUNT}, "5"},
    {std::string{minifi::sitetosite::HttpSiteToSiteClient::HANDSHAKE_PROPERTY_BATCH_SIZE}, "100"},
    {std::string{minifi::sitetosite::HttpSiteToSiteClient::HANDSHAKE_PROPERTY_BATCH_DURATION}, "30000"}
  };
  std::unordered_map<std::string, std::string> received_headers;
  for (int i = 0; i < req_info->num_headers; ++i) {
    auto header = &req_info->http_headers[i];
    received_headers[std::string(header->name)] = std::string(header->value);
  }
  for (const auto& header : expected_headers) {
    CHECK(received_headers.contains(header.first));
    CHECK(received_headers[header.first] == header.second);
  }

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

bool FlowFileResponder::handlePost(CivetServer* /*server*/, struct mg_connection *conn) {
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
      REQUIRE(read > 0);
      total_size += read;
    }

    const auto flow = std::make_shared<FlowObj>();

    for (uint32_t i = 0; i < num_attributes; i++) {
      std::string name;
      std::string value;
      {
        const auto read = stream.read(name, true);
        if (!isServerRunning()) return false;
        REQUIRE(read > 0);
        total_size += read;
      }
      {
        const auto read = stream.read(value, true);
        if (!isServerRunning()) return false;
        REQUIRE(read > 0);
        total_size += read;
      }
      flow->attributes[name] = value;
    }
    uint64_t length{};
    {
      const auto read = stream.read(length);
      if (!isServerRunning()) return false;
      REQUIRE(read > 0);
      total_size += read;
    }

    total_size += length;
    flow->data.resize(gsl::narrow<size_t>(length));
    flow->total_size = total_size;

    {
      const auto read = stream.read(flow->data);
      if (!isServerRunning()) return false;
      (void)read;
      REQUIRE(read == length);
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

bool FlowFileResponder::handleGet(CivetServer* /*server*/, struct mg_connection *conn) {
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
    minifi::io::CRCStream <minifi::io::OutputStream> stream(gsl::make_not_null(&serializer));
    for (const auto& flow : flows) {
      auto num_attributes = gsl::narrow<uint32_t>(flow->attributes.size());
      stream.write(num_attributes);
      for (const auto& entry : flow->attributes) {
        stream.write(entry.first);
        stream.write(entry.second);
      }
      uint64_t length = flow->data.size();
      stream.write(length);
      stream.write(flow->data);
    }
  } else {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nConnection: "
              "close\r\nContent-Length: 0\r\n");
    mg_printf(conn, "Content-Type: text/plain\r\n\r\n");
  }
  return true;
}

bool DeleteTransactionResponder::handleDelete(CivetServer* /*server*/, struct mg_connection *conn) {
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

void HeartbeatHandler::sendHeartbeatResponse(const std::vector<C2Operation>& operations, struct mg_connection * conn) {
  rapidjson::Document hb_obj{rapidjson::kObjectType};
  hb_obj.AddMember("operation", "heartbeat", hb_obj.GetAllocator());
  hb_obj.AddMember("requested_operations", rapidjson::kArrayType, hb_obj.GetAllocator());
  for (const auto& c2_operation : operations) {
    rapidjson::Value op{rapidjson::kObjectType};
    op.AddMember("operation", c2_operation.operation, hb_obj.GetAllocator());
    op.AddMember("operationid", c2_operation.operation_id, hb_obj.GetAllocator());
    op.AddMember("operand", c2_operation.operand, hb_obj.GetAllocator());
    if (!c2_operation.args.empty()) {
      rapidjson::Value args{rapidjson::kObjectType};
      for (auto& [arg_name, arg_val] : c2_operation.args) {
        rapidjson::Value json_arg_val;
        if (auto* json_val = arg_val.json()) {
          json_arg_val.CopyFrom(*json_val, hb_obj.GetAllocator());
        } else {
          json_arg_val.SetString(arg_val.to_string(), hb_obj.GetAllocator());
        }
        args.AddMember(rapidjson::StringRef(arg_name), json_arg_val, hb_obj.GetAllocator());
      }
      op.AddMember("args", args, hb_obj.GetAllocator());
    }
    hb_obj["requested_operations"].PushBack(op, hb_obj.GetAllocator());
  }

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  hb_obj.Accept(writer);

  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
            "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
            buffer.GetLength());
  mg_printf(conn, "%s", buffer.GetString());
}

void HeartbeatHandler::verifyJsonHasAgentManifest(const rapidjson::Document& root, const std::vector<std::string>& verify_components, const std::vector<std::string>& disallowed_properties) {
  bool found = false;
  REQUIRE(root.HasMember("agentInfo"));
  REQUIRE(root["agentInfo"].HasMember("agentManifest"));
  REQUIRE(root["agentInfo"]["agentManifest"].HasMember("bundles"));
  REQUIRE(root["agentInfo"].HasMember("agentManifestHash"));
  const std::string manifestHash = root["agentInfo"]["agentManifestHash"].GetString();
  REQUIRE(manifestHash.length() == 128);

  // throws if not a valid hexadecimal hash
  const auto hashVec = minifi::utils::string::from_hex(manifestHash);
  REQUIRE(hashVec.size() == 64);

  for (auto &bundle : root["agentInfo"]["agentManifest"]["bundles"].GetArray()) {
    REQUIRE(bundle.HasMember("artifact"));
    std::string str = bundle["artifact"].GetString();
    if (str == "minifi-standard-processors") {
      std::vector<std::string> classes;
      for (auto &proc : bundle["componentManifest"]["processors"].GetArray()) {
        classes.push_back(proc["type"].GetString());
      }

      const auto bundle_details = BundleIdentifier{.name = str, .version = AgentBuild::VERSION};
      const auto group = minifi::ClassDescriptionRegistry::getClassDescriptions().at(bundle_details);
      for (const auto& proc : group.processors) {
        REQUIRE(std::find(classes.begin(), classes.end(), proc.full_name_) != std::end(classes));
        (void)proc;
        found = true;
      }
    }
  }
  REQUIRE(found);

  verifySupportedOperations(root, verify_components, disallowed_properties);
}

void HeartbeatHandler::verify(struct mg_connection *conn) {
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

std::set<std::string> HeartbeatHandler::getOperandsOfProperties(const rapidjson::Value& operation_node) {
  std::set<std::string> operands;
  REQUIRE(operation_node.HasMember("properties"));
  const auto& properties_node = operation_node["properties"];
  for (auto it = properties_node.MemberBegin(); it != properties_node.MemberEnd(); ++it) {
    operands.insert(it->name.GetString());
  }
  return operands;
}

void HeartbeatHandler::verifyMetadata(const rapidjson::Value& operation_node, const std::unordered_map<std::string, Metadata>& operand_with_metadata) {
  std::unordered_map<std::string, Metadata> operand_with_metadata_found;
  const auto& properties_node = operation_node["properties"];
  for (auto prop_it = properties_node.MemberBegin(); prop_it != properties_node.MemberEnd(); ++prop_it) {
    if (prop_it->value.ObjectEmpty()) {
      continue;
    }
    Metadata metadata_item;
    for (auto metadata_it = prop_it->value.MemberBegin(); metadata_it != prop_it->value.MemberEnd(); ++metadata_it) {
      std::vector<std::unordered_map<std::string, std::string>> values;
      for (const auto& value : metadata_it->value.GetArray()) {
        std::unordered_map<std::string, std::string> value_item;
        for (auto value_it = value.MemberBegin(); value_it != value.MemberEnd(); ++value_it) {
          value_item.emplace(value_it->name.GetString(), value_it->value.GetString());
        }
        values.push_back(value_item);
      }
      metadata_item.emplace(metadata_it->name.GetString(), values);
    }
    operand_with_metadata_found.emplace(prop_it->name.GetString(), metadata_item);
  }
  REQUIRE(operand_with_metadata_found == operand_with_metadata);
}

void HeartbeatHandler::verifyProperties(const rapidjson::Value& operation_node, minifi::c2::Operation operation,
    const std::vector<std::string>& verify_components, const std::vector<std::string>& disallowed_properties) {
  switch (operation) {
    case minifi::c2::Operation::describe: {
      verifyOperands<minifi::c2::DescribeOperand>(operation_node);
      break;
    }
    case minifi::c2::Operation::update: {
      std::vector<std::unordered_map<std::string, std::string>> config_properties;
      const auto prop_reader = [this](const std::string& sensitive_props) { return configuration_->getString(sensitive_props); };
      const auto sensitive_props = minifi::Configuration::getSensitiveProperties(prop_reader);

      auto allowed_not_sensitive_configuration_properties = minifi::Configuration::CONFIGURATION_PROPERTIES | ranges::views::filter([&](const auto& configuration_property) {
        const auto& configuration_property_name = configuration_property.first;
        return !ranges::contains(sensitive_props, configuration_property_name) && !ranges::contains(disallowed_properties, configuration_property_name);
      });
      for (const auto& [property_name, property_validator] : allowed_not_sensitive_configuration_properties) {
        std::unordered_map<std::string, std::string> config_property;
        config_property.emplace("propertyName", property_name);
        if (auto value = configuration_->getRawValue(std::string(property_name))) {
          config_property.emplace("propertyValue", *value);
        }
        if (const auto nifi_standard_validator = property_validator->getEquivalentNifiStandardValidatorName()) {
          config_property.emplace("validator", *nifi_standard_validator);
        }
        config_properties.push_back(config_property);
      }
      Metadata metadata;
      metadata.emplace("availableProperties", config_properties);
      std::unordered_map<std::string, Metadata> operand_with_metadata;
      operand_with_metadata.emplace("properties", metadata);
      verifyOperands<minifi::c2::UpdateOperand>(operation_node, operand_with_metadata);
      break;
    }
    case minifi::c2::Operation::transfer: {
      verifyOperands<minifi::c2::TransferOperand>(operation_node);
      break;
    }
    case minifi::c2::Operation::clear: {
      verifyOperands<minifi::c2::ClearOperand>(operation_node);
      break;
    }
    case minifi::c2::Operation::start:
    case minifi::c2::Operation::stop: {
      auto operands = getOperandsOfProperties(operation_node);
      REQUIRE(operands.contains("c2"));
      // FlowController is also present, but this handler has no way of knowing its UUID to test it
      for (const auto& component : verify_components) {
        REQUIRE(operands.contains(component));
      }
      break;
    }
    default:
      break;
  }
}

void HeartbeatHandler::verifySupportedOperations(const rapidjson::Document& root, const std::vector<std::string>& verify_components, const std::vector<std::string>& disallowed_properties) {
  auto& agent_manifest = root["agentInfo"]["agentManifest"];
  REQUIRE(agent_manifest.HasMember("supportedOperations"));

  std::set<std::string> operations;
  for (const auto& operation_node : agent_manifest["supportedOperations"].GetArray()) {
    REQUIRE(operation_node.HasMember("type"));
    operations.insert(operation_node["type"].GetString());
    verifyProperties(operation_node, minifi::utils::enumCast<minifi::c2::Operation>(operation_node["type"].GetString(), true), verify_components, disallowed_properties);
  }

  REQUIRE(operations == std::set<std::string>(magic_enum::enum_names<minifi::c2::Operation>().begin(), magic_enum::enum_names<minifi::c2::Operation>().end()));
}

void StoppingHeartbeatHandler::sendStartStopOperation(struct mg_connection *conn) {
  std::lock_guard<std::mutex> lock(start_stop_send_mutex_);
  std::string requested_operation;
  if (post_count_ == 0) {
    requested_operation = R"({ "operationid" : 41, "operation" : "stop", "operand" : "2438e3c8-015a-1000-79ca-83af40ec1991" }, )"
        R"({ "operationid" : 42, "operation" : "stop", "operand" : "FlowController" })";
  } else if (post_count_ == 1) {
    requested_operation = R"({ "operationid" : 43, "operation" : "start", "operand" : "2438e3c8-015a-1000-79ca-83af40ec1991" }, )"
        R"({ "operationid" : 44, "operation" : "start", "operand" : "FlowController" })";
  } else if (post_count_ == 2) {
    requested_operation = R"({ "identifier" : 45, "operation" : "STOP", "operand" : "PROCESSOR", "args" : { "processorId" : "2438e3c8-015a-1000-79ca-83af40ec1992" } }, )"
        R"({ "identifier" : 46, "operation" : "STOP", "operand" : "FLOW" })";
  } else if (post_count_ == 3) {
    requested_operation = R"({ "identifier" : 47, "operation" : "START", "operand" : "PROCESSOR", "args" : { "processorId" : "2438e3c8-015a-1000-79ca-83af40ec1992" } }, )"
        R"({ "identifier" : 48, "operation" : "START", "operand" : "FLOW" })";
  } else {
    requested_operation = R"({ "identifier" : 49, "operation" : "STOP", "operand" : "PROCESSOR", "args" : { "processorId" : "9998e3c8-015a-1000-79ca-83af40ec1999" } }, )"
        R"({ "identifier" : 50, "operation" : "STOP", "operand" : "PROCESSOR" })";
  }

  std::string resp = R"({"operation" : "heartbeat", "requested_operations" : [ )" + requested_operation + " ]}";
  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
            "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
            resp.length());
  mg_printf(conn, "%s", resp.c_str());
  ++post_count_;
}

bool C2FlowProvider::handleGet(CivetServer* /*server*/, struct mg_connection *conn) {
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

bool C2UpdateHandler::handlePost(CivetServer* /*server*/, struct mg_connection *conn) {
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

void C2UpdateHandler::setC2RestResponse(const std::string& url, const std::string& name, const std::optional<std::string>& persist) {
  std::string content = R"({"location": ")" + url + "\"";
  if (persist) {
    content += R"(, "persist": ")" + *persist + "\"";
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

bool C2FailedUpdateHandler::handlePost(CivetServer *server, struct mg_connection *conn) {
  calls_++;
  const auto data = readPayload(conn);

  if (data.find("operationState") != std::string::npos) {
    REQUIRE(data.find("state\": \"NOT_APPLIED") != std::string::npos);
  }

  return C2UpdateHandler::handlePost(server, conn);
}

void TimeoutingHTTPHandler::respond(struct mg_connection *conn) {
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
bool HttpGetResponder::handleGet(CivetServer* /*server*/, struct mg_connection *conn) {
  puts("handle get");
  static const std::string site2site_rest_resp = "hi this is a get test";
  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
            "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
            site2site_rest_resp.length());
  mg_printf(conn, "%s", site2site_rest_resp.c_str());
  return true;
}

bool C2AcknowledgeHandler::handlePost(CivetServer* /*server*/, struct mg_connection* conn) {
  std::string req = readPayload(conn);
  rapidjson::Document root;
  root.Parse(req.data(), req.size());

  std::string result_state;
  std::string details;

  if (root.IsObject() && root.HasMember("operationState")) {
    if (root["operationState"].IsObject()) {
      if (root["operationState"].HasMember("state")) {
        result_state = root["operationState"]["state"].GetString();
        std::lock_guard<std::mutex> guard(apply_count_mtx_);
        ++apply_count_[result_state];
      }
      if (root["operationState"].HasMember("details")) {
        details = root["operationState"]["details"].GetString();
      }
    }
  }
  if (root.IsObject() && root.HasMember("operationId")) {
    std::lock_guard<std::mutex> guard(ack_operations_mtx_);
    acknowledged_operations_.insert({root["operationId"].GetString(), OpResult{result_state, details}});
  }

  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                  "text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
  return true;
}

}  // namespace org::apache::nifi::minifi::test
