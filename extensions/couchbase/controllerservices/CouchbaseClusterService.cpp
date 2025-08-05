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

#include "core/Resource.h"
#include "couchbase/codec/raw_binary_transcoder.hxx"
#include "couchbase/codec/raw_json_transcoder.hxx"
#include "couchbase/codec/raw_string_transcoder.hxx"
#include "utils/TimeUtil.h"

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

CouchbaseClient::CouchbaseClient(std::string connection_string, std::string username, std::string password, minifi::controllers::SSLContextServiceInterface* ssl_context_service,
  const std::shared_ptr<core::logging::Logger>& logger)
    : connection_string_(std::move(connection_string)), logger_(logger), cluster_options_(buildClusterOptions(std::move(username), std::move(password), ssl_context_service)) {
}

::couchbase::cluster_options CouchbaseClient::buildClusterOptions(std::string username, std::string password, minifi::controllers::SSLContextServiceInterface* ssl_context_service) {
  if (username.empty() && (!ssl_context_service || (ssl_context_service && ssl_context_service->getCertificateFile().empty()))) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Neither username and password nor SSLContextService is provided for Couchbase authentication");
  }

  if (!username.empty() && ssl_context_service && !ssl_context_service->getCertificateFile().empty()) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Either username and password or mTLS certificate authentication should be used in the SSLContextService for Couchbase, "
      "but not both");
  }

  if (!username.empty()) {
    logger_->log_debug("Using username and password authentication for Couchbase server");
    if (password.empty()) {
      throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Password missing for Couchbase server authentication");
    }
    ::couchbase::cluster_options cluster_options(std::move(username), std::move(password));
    if (ssl_context_service && !ssl_context_service->getCACertificate().empty()) {
      logger_->log_debug("Setting Couchbase client CA certificate path to '{}'", ssl_context_service->getCACertificate().string());
      cluster_options.security().trust_certificate(ssl_context_service->getCACertificate().string());
    }
    return cluster_options;
  }

  logger_->log_debug("Using mTLS authentication for Couchbase server");
  logger_->log_debug("Setting Couchbase client SSL key file path to '{}'", ssl_context_service->getPrivateKeyFile().string());
  logger_->log_debug("Setting Couchbase client certificate file path to '{}'", ssl_context_service->getCertificateFile().string());
  if (ssl_context_service->getPrivateKeyFile().empty() || ssl_context_service->getCertificateFile().empty()) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Couchbase client private key path or client certificate path is empty");
  }

  ::couchbase::cluster_options cluster_options(::couchbase::certificate_authenticator(ssl_context_service->getCertificateFile().string(), ssl_context_service->getPrivateKeyFile().string()));
  if (!ssl_context_service->getCACertificate().empty()) {
    logger_->log_debug("Setting Couchbase client CA certificate path to '{}'", ssl_context_service->getCACertificate().string());
    cluster_options.security().trust_certificate(ssl_context_service->getCACertificate().string());
  }
  cluster_options.security().tls_verify(::couchbase::tls_verify_mode::peer);
  return cluster_options;
}

nonstd::expected<::couchbase::collection, CouchbaseErrorType> CouchbaseClient::getCollection(const CouchbaseCollection& collection) {
  auto connection_result = establishConnection();
  if (!connection_result) {
    return nonstd::make_unexpected(connection_result.error());
  }
  std::lock_guard<std::mutex> lock(cluster_mutex_);
  return cluster_->bucket(collection.bucket_name).scope(collection.scope_name).collection(collection.collection_name);
}

nonstd::expected<CouchbaseUpsertResult, CouchbaseErrorType> CouchbaseClient::upsert(
    const CouchbaseCollection& collection, CouchbaseValueType document_type, const std::string& document_id, const std::vector<std::byte>& buffer, const ::couchbase::upsert_options& options) {
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
      logger_->log_error("Failed to upsert document '{}' to collection '{}.{}.{}' due to temporary issue, error code: '{}', message: '{}'", document_id, collection.bucket_name, collection.scope_name,
          collection.collection_name, upsert_err.ec(), upsert_err.message());
      return nonstd::make_unexpected(CouchbaseErrorType::TEMPORARY);
    }
    logger_->log_error("Failed to upsert document '{}' to collection '{}.{}.{}' with error code: '{}', message: '{}'", document_id, collection.bucket_name, collection.scope_name,
        collection.collection_name, upsert_err.ec(), upsert_err.message());
    return nonstd::make_unexpected(CouchbaseErrorType::FATAL);
  } else {
    return CouchbaseUpsertResult {
      {
        collection.bucket_name,
        upsert_resp.cas().value(),
      },
      (upsert_resp.mutation_token().has_value() ? upsert_resp.mutation_token()->sequence_number() : 0),
      (upsert_resp.mutation_token().has_value() ? upsert_resp.mutation_token()->partition_uuid() : 0),
      gsl::narrow<uint16_t>(upsert_resp.mutation_token().has_value() ? upsert_resp.mutation_token()->partition_id() : 0)
    };
  }
}

nonstd::expected<CouchbaseGetResult, CouchbaseErrorType> CouchbaseClient::get(const CouchbaseCollection& collection, const std::string& document_id, CouchbaseValueType return_type) {
  auto collection_result = getCollection(collection);
  if (!collection_result.has_value()) {
    return nonstd::make_unexpected(collection_result.error());
  }

  ::couchbase::get_options options;
  options.with_expiry(true);
  auto [get_err, resp] = collection_result->get(document_id, options).get();
  if (get_err.ec()) {
    if (getErrorType(get_err.ec()) == CouchbaseErrorType::TEMPORARY) {
      logger_->log_error("Failed to get document '{}' from collection '{}.{}.{}' due to timeout", document_id, collection.bucket_name, collection.scope_name, collection.collection_name);
      return nonstd::make_unexpected(CouchbaseErrorType::TEMPORARY);
    }
    std::string cause = get_err.cause() ? get_err.cause()->message() : "";
    logger_->log_error("Failed to get document '{}' from collection '{}.{}.{}' with error code: '{}', message: '{}'", document_id, collection.bucket_name, collection.scope_name,
        collection.collection_name, get_err.ec(), get_err.message());
    return nonstd::make_unexpected(CouchbaseErrorType::FATAL);
  } else {
    try {
      CouchbaseGetResult result;
      result.bucket_name = collection.bucket_name;
      result.cas = resp.cas().value();
      if (return_type == CouchbaseValueType::Json) {
        result.value = resp.content_as<::couchbase::codec::binary, ::couchbase::codec::raw_json_transcoder>();
      } else if (return_type == CouchbaseValueType::String) {
        result.value = resp.content_as<::couchbase::codec::raw_string_transcoder>();
      } else {
        result.value = resp.content_as<::couchbase::codec::raw_binary_transcoder>();
      }
      if (resp.expiry_time().has_value()) {
        result.expiry = utils::timeutils::getTimeStr(*resp.expiry_time());
      }
      return result;
    } catch (const std::exception& ex) {
      logger_->log_error("Failed to get content for document '{}' from collection '{}.{}.{}' with the following exception: '{}'", document_id, collection.bucket_name, collection.scope_name,
          collection.collection_name, ex.what());
      return nonstd::make_unexpected(CouchbaseErrorType::FATAL);
    }
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

  auto [connect_err, cluster] = ::couchbase::cluster::connect(connection_string_, cluster_options_).get();
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
  std::string connection_string = getProperty(ConnectionString.name) | utils::orThrow("required property");
  std::string username = getProperty(UserName.name).value_or("");
  std::string password = getProperty(UserPassword.name).value_or("");

  if (connection_string.empty()) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Missing connection string");
  }

  if ((username.empty() || password.empty()) && linked_services_.empty()) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Missing username and password or SSLContextService as a linked service");
  }

  minifi::controllers::SSLContextServiceInterface* ssl_context_service_ptr = nullptr;
  if (!linked_services_.empty()) {
    auto ssl_context_service = std::dynamic_pointer_cast<minifi::controllers::SSLContextServiceInterface>(linked_services_[0]);
    if (!ssl_context_service) {
      throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Linked service is not an SSLContextService");
    }
    ssl_context_service_ptr = ssl_context_service.get();
  }
  client_ = std::make_unique<CouchbaseClient>(connection_string, username, password, ssl_context_service_ptr, logger_);

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
    couchbase_cluster_service = std::dynamic_pointer_cast<CouchbaseClusterService>(context.getControllerService(*connection_controller_name, context.getProcessor().getUUID()));
  }
  if (!couchbase_cluster_service) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Missing Couchbase Cluster Service");
  }
  return gsl::make_not_null(couchbase_cluster_service);
}

REGISTER_RESOURCE(CouchbaseClusterService, ControllerService);

}  // namespace controllers
}  // namespace org::apache::nifi::minifi::couchbase
