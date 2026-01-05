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
#include <mutex>
#include <variant>

#include "core/controller/ControllerServiceBase.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "couchbase/cluster.hxx"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/controllers/SSLContextServiceInterface.h"

namespace org::apache::nifi::minifi::couchbase {

struct CouchbaseCollection {
  std::string bucket_name;
  std::string scope_name;
  std::string collection_name;
};

struct CouchbaseCallResult {
  std::string bucket_name;
  std::uint64_t cas{0};
};

struct CouchbaseGetResult : public CouchbaseCallResult {
  std::string expiry;
  std::variant<std::vector<std::byte>, std::string> value;
};

struct CouchbaseUpsertResult : public CouchbaseCallResult {
  std::uint64_t sequence_number{0};
  std::uint64_t partition_uuid{0};
  std::uint16_t partition_id{0};
};

enum class CouchbaseValueType {
  Json,
  Binary,
  String
};

enum class CouchbaseErrorType {
  FATAL,
  TEMPORARY,
};

class CouchbaseClient {
 public:
  CouchbaseClient(std::string connection_string, std::string username, std::string password, controllers::SSLContextServiceInterface* ssl_context_service,
    const std::shared_ptr<core::logging::Logger>& logger);

  ~CouchbaseClient() {
    close();
  }

  CouchbaseClient(const CouchbaseClient&) = delete;
  CouchbaseClient(CouchbaseClient&&) = delete;
  CouchbaseClient& operator=(CouchbaseClient&&) = delete;
  CouchbaseClient& operator=(const CouchbaseClient&) = delete;

  nonstd::expected<CouchbaseUpsertResult, CouchbaseErrorType> upsert(const CouchbaseCollection& collection, CouchbaseValueType document_type, const std::string& document_id,
    const std::vector<std::byte>& buffer, const ::couchbase::upsert_options& options);
  nonstd::expected<CouchbaseGetResult, CouchbaseErrorType> get(const CouchbaseCollection& collection, const std::string& document_id, CouchbaseValueType return_type);
  nonstd::expected<void, CouchbaseErrorType> establishConnection();
  void close();

 private:
  ::couchbase::cluster_options buildClusterOptions(std::string username, std::string password, minifi::controllers::SSLContextServiceInterface* ssl_context_service);
  nonstd::expected<::couchbase::collection, CouchbaseErrorType> getCollection(const CouchbaseCollection& collection);

  std::string connection_string_;
  std::shared_ptr<core::logging::Logger> logger_;
  ::couchbase::cluster_options cluster_options_;
  std::mutex cluster_mutex_;
  std::optional<::couchbase::cluster> cluster_;
};

namespace controllers {

class CouchbaseClusterService : public core::controller::ControllerServiceBase, public core::controller::ControllerServiceInterface {
 public:
  using ControllerServiceBase::ControllerServiceBase;

  EXTENSIONAPI static constexpr const char* Description = "Provides a centralized Couchbase connection and bucket passwords management. Bucket passwords can be specified via dynamic properties.";

  EXTENSIONAPI static constexpr auto ConnectionString = core::PropertyDefinitionBuilder<>::createProperty("Connection String")
      .withDescription("The hostnames or ip addresses of the bootstraping nodes and optional parameters. Syntax: couchbase://node1,node2,nodeN?param1=value1&param2=value2&paramN=valueN")
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

  void initialize() override;

  void onEnable() override;
  void notifyStop() override {
    if (client_) {
      client_->close();
    }
  }

  ControllerServiceInterface* getControllerServiceInterface() override {return this;}

  virtual nonstd::expected<CouchbaseUpsertResult, CouchbaseErrorType> upsert(const CouchbaseCollection& collection, CouchbaseValueType document_type,
      const std::string& document_id, const std::vector<std::byte>& buffer, const ::couchbase::upsert_options& options) {
    gsl_Expects(client_);
    return client_->upsert(collection, document_type, document_id, buffer, options);
  }

  virtual nonstd::expected<CouchbaseGetResult, CouchbaseErrorType> get(const CouchbaseCollection& collection, const std::string& document_id, CouchbaseValueType return_type) {
    gsl_Expects(client_);
    return client_->get(collection, document_id, return_type);
  }

 private:
  std::unique_ptr<CouchbaseClient> client_;
};

}  // namespace controllers
}  // namespace org::apache::nifi::minifi::couchbase
