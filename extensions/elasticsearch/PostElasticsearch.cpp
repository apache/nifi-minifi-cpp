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

#include "PostElasticsearch.h"

#include <utility>
#include <vector>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "rapidjson/document.h"
#include "rapidjson/stream.h"
#include "rapidjson/writer.h"
#include "utils/JsonCallback.h"
#include "utils/expected.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {

void PostElasticsearch::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

auto PostElasticsearch::getSSLContextService(core::ProcessContext& context) const {
  if (auto ssl_context = context.getProperty(PostElasticsearch::SSLContext))
    return std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(context.getControllerService(*ssl_context, getUUID()));
  return std::shared_ptr<minifi::controllers::SSLContextService>{};
}

auto PostElasticsearch::getCredentialsService(core::ProcessContext& context) const {
  if (auto credentials = context.getProperty(PostElasticsearch::ElasticCredentials))
    return std::dynamic_pointer_cast<ElasticsearchCredentialsControllerService>(context.getControllerService(*credentials, getUUID()));
  return std::shared_ptr<ElasticsearchCredentialsControllerService>{};
}

void PostElasticsearch::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  max_batch_size_ = utils::parseU64Property(context, MaxBatchSize);
  if (max_batch_size_ < 1)
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Max Batch Size property is invalid");

  if (auto hosts_str = context.getProperty(Hosts)) {
    auto hosts = utils::string::split(*hosts_str, ",");
    if (hosts.size() > 1)
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Multiple hosts not yet supported");
    host_url_ = hosts[0];
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid hosts");
  }

  credentials_service_ = getCredentialsService(context);
  if (!credentials_service_)
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing Elasticsearch credentials service");

  client_.initialize(http::HttpRequestMethod::POST, host_url_ + "/_bulk", getSSLContextService(context));
  client_.setContentType("application/json");
  credentials_service_->authenticateClient(client_);
}

namespace {

class ElasticPayload {
 public:
  [[nodiscard]] std::string toString() const {
    auto result = headerString();
    if (payload_) {
      rapidjson::StringBuffer payload_buffer;
      rapidjson::Writer<rapidjson::StringBuffer> payload_writer(payload_buffer);
      payload_->Accept(payload_writer);
      result = result + std::string("\n") + payload_buffer.GetString();
    }
    return result;
  }

  static nonstd::expected<ElasticPayload, std::string> parse(core::ProcessSession& session, core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) {
    auto action = context.getProperty(PostElasticsearch::Action, flow_file.get());
    if (!action || (action != "index" && action != "create" && action != "delete" && action != "update" && action != "upsert"))
      return nonstd::make_unexpected("Missing or invalid action");

    auto index = context.getProperty(PostElasticsearch::Index, flow_file.get());
    if (!index)
      return nonstd::make_unexpected("Missing index");

    auto id = context.getProperty(PostElasticsearch::Identifier, flow_file.get()) | utils::toOptional();
    if (!id && (action == "delete" || action == "update" || action == "upsert"))
      return nonstd::make_unexpected("Identifier is required for DELETE,UPDATE and UPSERT actions");

    std::optional<rapidjson::Document> payload;
    if (action == "index" || action == "create") {
      payload = rapidjson::Document(rapidjson::kObjectType);
      utils::JsonInputCallback callback(*payload);
      if (session.read(flow_file, std::ref(callback)) < 0) {
        return nonstd::make_unexpected("invalid flowfile content");
      }
    }
    if (action == "update" || action == "upsert") {
      payload = rapidjson::Document(rapidjson::kObjectType);
      rapidjson::Document doc_member(rapidjson::kObjectType, &payload->GetAllocator());
      utils::JsonInputCallback callback(doc_member);
      if (session.read(flow_file, std::ref(callback)) < 0) {
        return nonstd::make_unexpected("invalid flowfile content");
      }
      if (action == "upsert") {
        action = "update";
        doc_member.AddMember("doc_as_upsert", true, doc_member.GetAllocator());
      }
      payload->AddMember("doc", doc_member, payload->GetAllocator());
    }
    return ElasticPayload(std::move(*action), std::move(*index), std::move(id), std::move(payload));
  }

 private:
  ElasticPayload(std::string operation,
                 std::string index,
                 std::optional<std::string> id,
                 std::optional<rapidjson::Document> payload) :
      operation_(std::move(operation)),
      index_(std::move(index)),
      id_(std::move(id)),
      payload_(std::move(payload)) {
  }

  [[nodiscard]] std::string headerString() const {
    rapidjson::Document first_line = rapidjson::Document(rapidjson::kObjectType);

    auto operation_index_key = rapidjson::Value(operation_.data(), gsl::narrow<rapidjson::SizeType>(operation_.size()));
    first_line.AddMember(operation_index_key, rapidjson::Value{rapidjson::kObjectType}, first_line.GetAllocator());
    auto& operation_request = first_line[operation_.c_str()];

    auto index_json = rapidjson::Value(index_.data(), gsl::narrow<rapidjson::SizeType>(index_.size()));
    operation_request.AddMember("_index", index_json, first_line.GetAllocator());

    if (id_) {
      auto id_json = rapidjson::Value(id_->data(), gsl::narrow<rapidjson::SizeType>(id_->size()));
      operation_request.AddMember("_id", id_json, first_line.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    first_line.Accept(writer);

    return buffer.GetString();
  }

  std::string operation_;
  std::string index_;
  std::optional<std::string> id_;
  std::optional<rapidjson::Document> payload_;
};

nonstd::expected<rapidjson::Document, std::string> submitRequest(http::HTTPClient& client, const std::string& payload, const size_t expected_items) {
  client.setPostFields(payload);
  if (!client.submit())
    return nonstd::make_unexpected("Submit failed");
  auto response_code = client.getResponseCode();
  if (response_code != 200)
    return nonstd::make_unexpected("Error occurred: " + std::to_string(response_code) + ", " + client.getResponseBody().data());
  rapidjson::Document response;
  rapidjson::ParseResult parse_result = response.Parse<rapidjson::kParseStopWhenDoneFlag>(client.getResponseBody().data());
  if (parse_result.IsError())
    return nonstd::make_unexpected("Response is not valid json");
  if (!response.HasMember("items"))
    return nonstd::make_unexpected("Response is invalid");
  if (response["items"].Size() != expected_items)
    return nonstd::make_unexpected("The number of responses dont match the number of requests");

  return response;
}

void addAttributesFromResponse(std::string name, rapidjson::Value::ConstMemberIterator object, core::FlowFile& flow_file) {
  name = name + "." + object->name.GetString();

  if (object->value.IsObject()) {
    for (auto it = object->value.MemberBegin(); it != object->value.MemberEnd(); ++it) {
      addAttributesFromResponse(name, it, flow_file);
    }
  } else if (object->value.IsInt64()) {
    flow_file.addAttribute(name, std::to_string(object->value.GetInt64()));
  } else if (object->value.IsString()) {
    flow_file.addAttribute(name, object->value.GetString());
  } else if (object->value.IsBool()) {
    flow_file.addAttribute(name, std::to_string(object->value.GetBool()));
  } else if (object->value.IsDouble()) {
    flow_file.addAttribute(name, std::to_string(object->value.GetDouble()));
  } else {
    core::logging::LoggerFactory<PostElasticsearch>::getLogger()->log_error("Unexpected {} in response json", magic_enum::enum_underlying(object->value.GetType()));
  }
}

void processResponseFromElastic(const rapidjson::Document& response, core::ProcessSession& session, const std::vector<std::shared_ptr<core::FlowFile>>& flowfiles_sent) {
  gsl_Expects(response.HasMember("items"));
  auto& items = response["items"];
  gsl_Expects(items.IsArray());
  gsl_Expects(items.Size() == flowfiles_sent.size());
  for (rapidjson::SizeType i = 0; i < items.Size(); ++i) {
    gsl_Expects(items[i].IsObject());
    for (auto it = items[i].MemberBegin(); it != items[i].MemberEnd(); ++it) {
      addAttributesFromResponse("elasticsearch", it, *flowfiles_sent[i]);
    }
    if (items[i].MemberBegin()->value.HasMember("error"))
      session.transfer(flowfiles_sent[i], PostElasticsearch::Error);
    else
      session.transfer(flowfiles_sent[i], PostElasticsearch::Success);
  }
}
}  // namespace

std::string PostElasticsearch::collectPayload(core::ProcessContext& context,
                                              core::ProcessSession& session,
                                              std::vector<std::shared_ptr<core::FlowFile>>& flowfiles_with_payload) const {
  std::stringstream payload;
  for (size_t flow_files_processed = 0; flow_files_processed < max_batch_size_; ++flow_files_processed) {
    auto flow_file = session.get();
    if (!flow_file)
      break;
    auto elastic_payload = ElasticPayload::parse(session, context, flow_file);
    if (!elastic_payload) {
      logger_->log_error("{}", elastic_payload.error());
      session.transfer(flow_file, PostElasticsearch::Failure);
      continue;
    }

    payload << elastic_payload->toString() << "\n";
    flowfiles_with_payload.push_back(flow_file);
  }
  return payload.str();
}

void PostElasticsearch::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(max_batch_size_ > 0);

  std::vector<std::shared_ptr<core::FlowFile>> flowfiles_with_payload;
  auto payload = collectPayload(context, session, flowfiles_with_payload);

  if (flowfiles_with_payload.empty()) {
    return;
  }

  auto result = submitRequest(client_, payload, flowfiles_with_payload.size());
  if (!result) {
    logger_->log_error("{}", result.error());
    for (const auto& flow_file_in_payload: flowfiles_with_payload)
      session.transfer(flow_file_in_payload, Failure);
    return;
  }

  processResponseFromElastic(*result, session, flowfiles_with_payload);
}

REGISTER_RESOURCE(PostElasticsearch, Processor);

}  // namespace org::apache::nifi::minifi::extensions::elasticsearch
