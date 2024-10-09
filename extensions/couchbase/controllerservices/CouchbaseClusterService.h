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

#include <memory>
#include <string>
#include <utility>

#include "core/controller/ControllerService.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "couchbase/cluster.hxx"
#include "core/ProcessContext.h"
#include "core/logging/LoggerConfiguration.h"
#include "couchbase/codec/raw_binary_transcoder.hxx"
#include "couchbase/error.hxx"

namespace org::apache::nifi::minifi::couchbase {

struct CouchbaseCollection {
  std::string bucket_name;
  std::string scope_name;
  std::string collection_name;
};

struct CouchbaseUpsertResult {
  std::string bucket_name;
  std::uint64_t cas{0};
  std::uint64_t sequence_number{0};
  std::uint64_t partition_uuid{0};
  std::uint16_t partition_id{0};
};

enum class CouchbaseErrorType {
  FATAL,
  TEMPORARY,
};

class CouchbaseClient {
 public:
  enum class State {
    DISCONNECTED,
    CONNECTED,
    UNKNOWN,
  };

  CouchbaseClient(std::string connection_string, std::string username, std::string password, const std::shared_ptr<core::logging::Logger>& logger)
    : connection_string_(std::move(connection_string)), username_(std::move(username)), password_(std::move(password)), logger_(logger) {
  }

  nonstd::expected<CouchbaseUpsertResult, CouchbaseErrorType> upsert(const CouchbaseCollection& collection, const std::string& document_id, const std::vector<std::byte>& buffer,
    const ::couchbase::upsert_options& options);
  std::optional<CouchbaseErrorType> establishConnection();
  void close();

 private:
  static constexpr std::array<::couchbase::errc::common, 9> temporary_connection_errors = {
    ::couchbase::errc::common::temporary_failure,
    ::couchbase::errc::common::request_canceled,
    ::couchbase::errc::common::service_not_available,
    ::couchbase::errc::common::internal_server_failure,
    ::couchbase::errc::common::cas_mismatch,
    ::couchbase::errc::common::ambiguous_timeout,
    ::couchbase::errc::common::unambiguous_timeout,
    ::couchbase::errc::common::rate_limited,
    ::couchbase::errc::common::quota_limited
  };

  static CouchbaseErrorType getErrorType(const std::error_code& error_code);
  nonstd::expected<::couchbase::collection, CouchbaseErrorType> getCollection(const CouchbaseCollection& collection);
  void setConnectionError();

  std::mutex state_mutex_;
  State state_ = State::DISCONNECTED;
  std::string connection_string_;
  std::string username_;
  std::string password_;
  ::couchbase::cluster cluster_;
  std::shared_ptr<core::logging::Logger> logger_;
};

namespace controllers {

class CouchbaseClusterService : public core::controller::ControllerService {
 public:
  explicit CouchbaseClusterService(std::string_view name, const minifi::utils::Identifier &uuid = {})
      : ControllerService(name, uuid) {
  }

  explicit CouchbaseClusterService(std::string_view name, const std::shared_ptr<Configure>& /*configuration*/)
      : ControllerService(name) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Provides a centralized Couchbase connection and bucket passwords management. Bucket passwords can be specified via dynamic properties.";

  EXTENSIONAPI static constexpr auto ConnectionString = core::PropertyDefinitionBuilder<>::createProperty("Connection String")
      .withDescription("The hostnames or ip addresses of the bootstraping nodes and optional parameters. Syntax) couchbase://node1,node2,nodeN?param1=value1&param2=value2&paramN=valueN")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto UserName = core::PropertyDefinitionBuilder<>::createProperty("User Name")
      .withDescription("The user name to authenticate MiNiFi as a Couchbase client.")
      .build();
  EXTENSIONAPI static constexpr auto UserPassword = core::PropertyDefinitionBuilder<>::createProperty("User Password")
      .withDescription("The user password to authenticate MiNiFi as a Couchbase client.")
      .isSensitive(true)
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
    ConnectionString,
    UserName,
    UserPassword
  });


  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() override;

  void yield() override {
  };

  bool isWorkAvailable() override {
    return false;
  };

  bool isRunning() const override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  void onEnable() override;
  void notifyStop() override {
    if (client_) {
      client_->close();
    }
  }

  virtual nonstd::expected<CouchbaseUpsertResult, CouchbaseErrorType> upsert(const CouchbaseCollection& collection,
      const std::string& document_id, const std::vector<std::byte>& buffer, const ::couchbase::upsert_options& options) {
    gsl_Expects(client_);
    return client_->upsert(collection, document_id, buffer, options);
  }

  static gsl::not_null<std::shared_ptr<CouchbaseClusterService>> getFromProperty(const core::ProcessContext& context, const core::PropertyReference& property);

 private:
  std::unique_ptr<CouchbaseClient> client_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<CouchbaseClusterService>::getLogger(uuid_);
};

}  // namespace controllers
}  // namespace org::apache::nifi::minifi::couchbase
