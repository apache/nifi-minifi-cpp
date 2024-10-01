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

namespace org::apache::nifi::minifi::couchbase {

nonstd::expected<CouchbaseUpsertResult, std::error_code> RemoteCouchbaseCollection::upsert(const std::string& document_id, const std::vector<std::byte>& buffer,
    const ::couchbase::upsert_options& options) {
  auto [err, resp] = collection_.upsert<::couchbase::codec::raw_binary_transcoder>(document_id, buffer, options).get();
  if (err.ec()) {
    client_.setConnectionError();
    return nonstd::make_unexpected(err.ec());
  } else {
    const uint64_t partition_uuid = (resp.mutation_token().has_value() ? resp.mutation_token()->partition_uuid() : 0);
    const uint64_t sequence_number = (resp.mutation_token().has_value() ? resp.mutation_token()->sequence_number() : 0);
    const uint16_t partition_id = (resp.mutation_token().has_value() ? resp.mutation_token()->partition_id() : 0);
    return CouchbaseUpsertResult {
      collection_.bucket_name(),
      resp.cas().value(),
      partition_uuid,
      sequence_number,
      partition_id
    };
  }
}

std::unique_ptr<CouchbaseCollection> CouchBaseClient::getCollection(std::string_view bucket_name, std::string_view scope_name, std::string_view collection_name) {
  if (!establishConnection()) {
    return nullptr;
  }
  return std::make_unique<RemoteCouchbaseCollection>(cluster_.bucket(bucket_name).scope(scope_name).collection(collection_name), *this);
}

void CouchBaseClient::setConnectionError() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  state_ = State::UNKNOWN;
}

void CouchBaseClient::close() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  if (state_ == State::CONNECTED || state_ == State::UNKNOWN) {
    cluster_.close().wait();
    state_ = State::DISCONNECTED;
  }
}

bool CouchBaseClient::establishConnection() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  if (state_ == State::CONNECTED) {
    return true;
  }

  if (state_ == State::UNKNOWN) {
    auto [err, resp] = cluster_.ping().get();
    if (err.ec()) {
      close();
    } else {
      state_ = State::CONNECTED;
      return true;
    }
  }

  auto options = ::couchbase::cluster_options(username_, password_);
  auto [connect_err, cluster] = ::couchbase::cluster::connect(connection_string_, options).get();
  if (connect_err.ec()) {
    logger_->log_error("Failed to connect to Couchbase cluster: {}", connect_err.message());
    return false;
  }
  cluster_ = std::move(cluster);
  state_ = State::CONNECTED;
  return true;
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
  client_ = std::make_unique<CouchBaseClient>(connection_string, username, password, logger_);
}

gsl::not_null<std::shared_ptr<CouchbaseClusterService>> CouchbaseClusterService::getFromProperty(const core::ProcessContext& context, const core::PropertyReference& property) {
  std::shared_ptr<CouchbaseClusterService> couchbase_cluster_service;
  if (auto connection_controller_name = context.getProperty(property)) {
    couchbase_cluster_service = std::dynamic_pointer_cast<CouchbaseClusterService>(context.getControllerService(*connection_controller_name));
  }
  if (!couchbase_cluster_service) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Missing Couchbase Cluster Service");
  }
  return gsl::make_not_null(couchbase_cluster_service);
}

REGISTER_RESOURCE(CouchbaseClusterService, ControllerService);

}  // namespace controllers
}  // namespace org::apache::nifi::minifi::couchbase
