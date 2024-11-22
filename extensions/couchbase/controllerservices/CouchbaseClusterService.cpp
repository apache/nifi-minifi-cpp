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

#include "CouchbaseClusterService.h"
#include "couchbase/codec/raw_binary_transcoder.hxx"
#include "couchbase/codec/raw_string_transcoder.hxx"
#include "couchbase/codec/raw_json_transcoder.hxx"

#include "core/Resource.h"

namespace org::apache::nifi::minifi::couchbase {

namespace {

constexpr auto temporary_connection_errors = std::to_array<::couchbase::errc::common>({
  ::couchbase::errc::common::temporary_failure,
  ::couchbase::errc::common::request_canceled,
  ::couchbase::errc::common::internal_server_failure,
  ::couchbase::errc::common::cas_mismatch,
  ::couchbase::errc::common::ambiguous_timeout,
  ::couchbase::errc::common::unambiguous_timeout,
  ::couchbase::errc::common::rate_limited,
  ::couchbase::errc::common::quota_limited
});

CouchbaseErrorType getErrorType(const std::error_code& error_code) {
  for (const auto& temporary_error : temporary_connection_errors) {
    if (static_cast<int>(temporary_error) == error_code.value()) {
      return CouchbaseErrorType::TEMPORARY;
    }
  }
  return CouchbaseErrorType::FATAL;
}

}  // namespace

nonstd::expected<::couchbase::collection, CouchbaseErrorType> CouchbaseClient::getCollection(const CouchbaseCollection& collection) {
  auto connection_result = establishConnection();
  if (!connection_result) {
    return nonstd::make_unexpected(connection_result.error());
  }
  std::lock_guard<std::mutex> lock(cluster_mutex_);
  return cluster_->bucket(collection.bucket_name).scope(collection.scope_name).collection(collection.collection_name);
}

nonstd::expected<CouchbaseUpsertResult, CouchbaseErrorType> CouchbaseClient::upsert(const CouchbaseCollection& collection,
    CouchbaseValueType document_type, const std::string& document_id, const std::vector<std::byte>& buffer, const ::couchbase::upsert_options& options) {
  auto collection_result = getCollection(collection);
  if (!collection_result.has_value()) {
    return nonstd::make_unexpected(collection_result.error());
  }

  std::pair<::couchbase::error, ::couchbase::mutation_result> result;
  if (document_type == CouchbaseValueType::Json) {
    result = collection_result->upsert<::couchbase::codec::raw_json_transcoder>(document_id, buffer, options).get();
  } else if (document_type == CouchbaseValueType::String) {
    std::string data_str(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    result = collection_result->upsert<::couchbase::codec::raw_string_transcoder>(document_id, data_str, options).get();
  } else {
    result = collection_result->upsert<::couchbase::codec::raw_binary_transcoder>(document_id, buffer, options).get();
  }
  auto& [upsert_err, upsert_resp] = result;
  if (upsert_err.ec()) {
    // ambiguous_timeout should not be retried as we do not know if the insert was successful or not
    if (getErrorType(upsert_err.ec()) == CouchbaseErrorType::TEMPORARY && upsert_err.ec().value() != static_cast<int>(::couchbase::errc::common::ambiguous_timeout)) {
      logger_->log_error("Failed to upsert document '{}' to collection '{}.{}.{}' due to temporary issue, error code: '{}', message: '{}'",
        document_id, collection.bucket_name, collection.scope_name, collection.collection_name, upsert_err.ec(), upsert_err.message());
      return nonstd::make_unexpected(CouchbaseErrorType::TEMPORARY);
    }
    logger_->log_error("Failed to upsert document '{}' to collection '{}.{}.{}' with error code: '{}', message: '{}'",
      document_id, collection.bucket_name, collection.scope_name, collection.collection_name, upsert_err.ec(), upsert_err.message());
    return nonstd::make_unexpected(CouchbaseErrorType::FATAL);
  } else {
    const uint64_t partition_uuid = (upsert_resp.mutation_token().has_value() ? upsert_resp.mutation_token()->partition_uuid() : 0);
    const uint64_t sequence_number = (upsert_resp.mutation_token().has_value() ? upsert_resp.mutation_token()->sequence_number() : 0);
    const uint16_t partition_id = (upsert_resp.mutation_token().has_value() ? upsert_resp.mutation_token()->partition_id() : 0);
    return CouchbaseUpsertResult {
      collection.bucket_name,
      upsert_resp.cas().value(),
      partition_uuid,
      sequence_number,
      partition_id
    };
  }
}

void CouchbaseClient::close() {
  std::lock_guard<std::mutex> lock(cluster_mutex_);
  if (cluster_) {
    cluster_->close().wait();
  }
  cluster_ = std::nullopt;
}

nonstd::expected<void, CouchbaseErrorType> CouchbaseClient::establishConnection() {
  std::lock_guard<std::mutex> lock(cluster_mutex_);
  if (cluster_) {
    return {};
  }

  auto options = ::couchbase::cluster_options(username_, password_);
  auto [connect_err, cluster] = ::couchbase::cluster::connect(connection_string_, options).get();
  if (connect_err.ec()) {
    logger_->log_error("Failed to connect to Couchbase cluster with error code: '{}' and message: '{}'", connect_err.ec(), connect_err.message());
    return nonstd::make_unexpected(getErrorType(connect_err.ec()));
  }

  cluster_ = std::move(cluster);
  return {};
}

namespace controllers {

void CouchbaseClusterService::initialize() {
  setSupportedProperties(Properties);
}

void CouchbaseClusterService::onEnable() {
  std::string connection_string;
  getProperty(ConnectionString, connection_string);
  std::string username;
  getProperty(UserName, username);
  std::string password;
  getProperty(UserPassword, password);
  if (connection_string.empty() || username.empty() || password.empty()) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Missing connection string, username or password");
  }

  client_ = std::make_unique<CouchbaseClient>(connection_string, username, password, logger_);
  auto result = client_->establishConnection();
  if (!result) {
    if (result.error() == CouchbaseErrorType::FATAL) {
      throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Failed to connect to Couchbase cluster with fatal error");
    }
    logger_->log_warn("Failed to connect to Couchbase cluster with temporary error, will retry connection when a Couchbase processor is triggered");
  }
}

gsl::not_null<std::shared_ptr<CouchbaseClusterService>> CouchbaseClusterService::getFromProperty(const core::ProcessContext& context, const core::PropertyReference& property) {
  std::shared_ptr<CouchbaseClusterService> couchbase_cluster_service;
  if (auto connection_controller_name = context.getProperty(property)) {
    couchbase_cluster_service = std::dynamic_pointer_cast<CouchbaseClusterService>(context.getControllerService(*connection_controller_name, context.getProcessorNode()->getUUID()));
  }
  if (!couchbase_cluster_service) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Missing Couchbase Cluster Service");
  }
  return gsl::make_not_null(couchbase_cluster_service);
}

REGISTER_RESOURCE(CouchbaseClusterService, ControllerService);

}  // namespace controllers
}  // namespace org::apache::nifi::minifi::couchbase
