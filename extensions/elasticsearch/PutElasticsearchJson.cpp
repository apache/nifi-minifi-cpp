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


#include "PutElasticsearchJson.h"
#include <vector>
#include <utility>

#include "ElasticsearchCredentialsControllerService.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "rapidjson/document.h"
#include "rapidjson/stream.h"
#include "rapidjson/writer.h"
#include "utils/expected.h"
#include "utils/JsonCallback.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {

const core::Relationship PutElasticsearchJson::Success("success", "All flowfiles that succeed in being transferred into Elasticsearch go here.");
const core::Relationship PutElasticsearchJson::Failure("failure", "All flowfiles that fail for reasons unrelated to server availability go to this relationship.");
const core::Relationship PutElasticsearchJson::Errors("errors", "All flowfiles that Elasticsearch responded to with an error go to this relationship.");

const core::Property PutElasticsearchJson::IndexOperation = core::PropertyBuilder::createProperty("Index operation")
    ->withDescription("The type of the operation used to index (create, delete, index, update)"
                      "Supports Expression Language: true (will be evaluated using flow file attributes and variable registry)")
    ->supportsExpressionLanguage(true)
    ->build();

const core::Property PutElasticsearchJson::MaxBatchSize = core::PropertyBuilder::createProperty("Max Batch Size")
    ->withDescription("The maximum number of Syslog events to process at a time.")
    ->withDefaultValue<uint64_t>(100)
    ->build();

const core::Property PutElasticsearchJson::ElasticCredentials = core::PropertyBuilder::createProperty("Elasticsearch Credentials Provider Service")
    ->withDescription("The Controller Service used to obtain Elasticsearch credentials.")
    ->isRequired(true)
    ->asType<ElasticsearchCredentialsControllerService>()
    ->build();

const core::Property PutElasticsearchJson::SSLContext = core::PropertyBuilder::createProperty("SSL Context Service")
    ->withDescription("The SSL Context Service used to provide client certificate "
                      "information for TLS/SSL (https) connections.")
    ->isRequired(false)
    ->asType<minifi::controllers::SSLContextService>()->build();

const core::Property PutElasticsearchJson::Hosts = core::PropertyBuilder::createProperty("Hosts")
    ->withDescription("A comma-separated list of HTTP hosts that host Elasticsearch query nodes. Currently only supports a single host.")
    ->supportsExpressionLanguage(true)
    ->build();

const core::Property PutElasticsearchJson::Index = core::PropertyBuilder::createProperty("Index")
    ->withDescription("The name of the index to use.")
    ->supportsExpressionLanguage(true)
    ->build();

const core::Property PutElasticsearchJson::Id = core::PropertyBuilder::createProperty("Id")
    ->withDescription("If the Index Operation is \"index\" or \"create\", this property may be left empty or evaluate to an empty value, "
                      "in which case the document's identifier will be auto-generated by Elasticsearch. "
                      "For all other Index Operations, the attribute must evaluate to a non-empty value.")
    ->supportsExpressionLanguage(true)
    ->build();


void PutElasticsearchJson::initialize() {
  setSupportedRelationships({Success, Failure, Errors});
  setSupportedProperties({ElasticCredentials, IndexOperation, MaxBatchSize, Hosts, Index, SSLContext, Id});
}

namespace {
auto getSSLContextService(core::ProcessContext& context) {
  if (auto ssl_context = context.getProperty(PutElasticsearchJson::SSLContext))
    return std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(context.getControllerService(*ssl_context));
  return std::shared_ptr<minifi::controllers::SSLContextService>{};
}

auto getCredentialsService(core::ProcessContext& context) {
  if (auto credentials = context.getProperty(PutElasticsearchJson::ElasticCredentials))
    return std::dynamic_pointer_cast<ElasticsearchCredentialsControllerService>(context.getControllerService(*credentials));
  return std::shared_ptr<ElasticsearchCredentialsControllerService>{};
}
}  // namespace

void PutElasticsearchJson::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  gsl_Expects(context);

  context->getProperty(MaxBatchSize.getName(), max_batch_size_);
  if (max_batch_size_ < 1)
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Max Batch Size property is invalid");

  std::string host_url{};
  if (auto hosts_str = context->getProperty(Hosts)) {
    auto hosts = utils::StringUtils::split(*hosts_str, ",");
    if (hosts.size() > 1)
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Multiple hosts not yet supported");
    host_url = hosts[0];
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid hosts");
  }

  auto credentials_service = getCredentialsService(*context);
  if (!credentials_service)
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing Elasticsearch credentials service");

  client_.initialize("POST", host_url + "/_bulk", getSSLContextService(*context));
  client_.setContentType("application/json");
  credentials_service->authenticateClient(client_);
}

namespace {

class ElasticPayload {
 public:
  std::string toString() const {
    auto result = headerString();
    if (payload_) {
      rapidjson::StringBuffer payload_buffer;
      rapidjson::Writer<rapidjson::StringBuffer> payload_writer(payload_buffer);
      payload_->Accept(payload_writer);
      result = result + std::string("\n") + payload_buffer.GetString();
    }
    if (doc_payload_) {
      rapidjson::StringBuffer doc_buffer;
      rapidjson::Writer<rapidjson::StringBuffer> doc_writer(doc_buffer);
      doc_payload_->Accept(doc_writer);
      result = result + std::string("\n{ \"doc\" : ") + doc_buffer.GetString() + "}";
    }
    return result;
  }

  static auto parse(core::ProcessSession& session, core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) -> nonstd::expected<ElasticPayload, std::string> {
    auto index_op = context.getProperty(PutElasticsearchJson::IndexOperation, flow_file);
    if (!index_op || (index_op != "index" && index_op != "create" && index_op != "delete" && index_op != "update"))
      return nonstd::make_unexpected("Missing or invalid index operation");

    auto index = context.getProperty(PutElasticsearchJson::Index, flow_file);
    if (!index)
      return nonstd::make_unexpected("Missing index");

    auto id = context.getProperty(PutElasticsearchJson::Id, flow_file);
    if (!id && (index_op == "delete" || index_op == "update"))
      return nonstd::make_unexpected("Id is required for DELETE or UPDATE operations");

    std::optional<rapidjson::Document> payload;
    std::optional<rapidjson::Document> doc_payload;
    if (index_op == "index" || index_op == "create") {
      payload = rapidjson::Document(rapidjson::kObjectType);;
      utils::JsonInputCallback callback(*payload);
      if (session.read(flow_file, std::ref(callback)) < 0) {
        return nonstd::make_unexpected("invalid flowfile content");
      }
    }
    if (index_op == "update") {
      doc_payload = rapidjson::Document(rapidjson::kObjectType);;
      utils::JsonInputCallback callback(*doc_payload);
      if (session.read(flow_file, std::ref(callback)) < 0) {
        return nonstd::make_unexpected("invalid flowfile content");
      }
    }
    return ElasticPayload(std::move(*index_op), std::move(*index), std::move(id), std::move(payload), std::move(doc_payload));
  }

 private:
  ElasticPayload(std::string operation, std::string index, std::optional<std::string> id, std::optional<rapidjson::Document> payload, std::optional<rapidjson::Document> doc_payload) :
      operation_(std::move(operation)),
      index_(std::move(index)),
      id_(std::move(id)),
      payload_(std::move(payload)),
      doc_payload_(std::move(doc_payload)) {
  }

  std::string headerString() const {
    rapidjson::Document first_line = rapidjson::Document(rapidjson::kObjectType);

    auto operation_index_key = rapidjson::Value(operation_.data(), operation_.size(), first_line.GetAllocator());
    first_line.AddMember(operation_index_key, rapidjson::Value{rapidjson::kObjectType}, first_line.GetAllocator());
    auto& operation_request = first_line[operation_.c_str()];

    auto index_json = rapidjson::Value(index_.data(), index_.size(), first_line.GetAllocator());
    operation_request.AddMember("_index", index_json, first_line.GetAllocator());

    if (id_) {
      auto id_json = rapidjson::Value(id_->data(), id_->size(), first_line.GetAllocator());
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
  std::optional<rapidjson::Document> doc_payload_;
};

auto submitRequest(utils::HTTPClient& client, const size_t expected_items) -> nonstd::expected<rapidjson::Document, std::string> {
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
  }
}
}  // namespace

void PutElasticsearchJson::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session && max_batch_size_ > 0);
  std::stringstream payload;
  std::vector<std::shared_ptr<core::FlowFile>> flowfiles_in_payload;
  for (size_t flow_files_processed = 0; flow_files_processed < max_batch_size_; ++flow_files_processed) {
    auto flow_file = session->get();
    if (!flow_file)
      break;
    auto elastic_payload = ElasticPayload::parse(*session, *context, flow_file);
    if (!elastic_payload) {
      logger_->log_error(elastic_payload.error().c_str());
      session->transfer(flow_file, Failure);
      continue;
    }

    payload << elastic_payload->toString() << "\n";
    flowfiles_in_payload.push_back(flow_file);
  }

  if (flowfiles_in_payload.empty()) {
    yield();
    return;
  }


  client_.setPostFields(payload.str());
  auto result = submitRequest(client_, flowfiles_in_payload.size());
  if (!result) {
    logger_->log_error(result.error().c_str());
    for (const auto& flow_file_in_payload: flowfiles_in_payload)
      session->transfer(flow_file_in_payload, Failure);
    return;
  }

  auto& items = result->operator[]("items");
  gsl_Expects(items.Size() == flowfiles_in_payload.size());
  for (size_t i = 0; i < items.Size(); ++i) {
    for (auto it = items[i].MemberBegin(); it != items[i].MemberEnd(); ++it) {
      addAttributesFromResponse("elasticsearch", it, *flowfiles_in_payload[i]);
    }
    if (items[i].MemberBegin()->value.HasMember("error"))
      session->transfer(flowfiles_in_payload[i], Errors);
    else
      session->transfer(flowfiles_in_payload[i], Success);
  }
}

REGISTER_RESOURCE(PutElasticsearchJson, "An Elasticsearch put processor that uses the Elastic _bulk REST API.");

}  // namespace org::apache::nifi::minifi::extensions::elasticsearch
