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

#pragma once
#include <utility>
#include <string>
#include "CouchbaseClusterService.h"
#include "unit/Catch.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::couchbase::test {

struct GetCollectionParameters {
  std::string bucket_name;
  std::string scope_name;
  std::string collection_name;
};

struct UpsertParameters {
  std::string document_id;
  std::vector<std::byte> buffer;
  ::couchbase::upsert_options options;
};

class MockCouchbaseCollection : public CouchbaseCollection {
 public:
  MockCouchbaseCollection(UpsertParameters& parameters, std::error_code& upsert_error_code, std::string_view bucket_name)
      : parameters_(parameters),
        upsert_error_code_(upsert_error_code),
        bucket_name_(bucket_name) {}

  nonstd::expected<CouchbaseUpsertResult, std::error_code> upsert(const std::string& document_id, const std::vector<std::byte>& buffer, const ::couchbase::upsert_options& options) override {
    parameters_.document_id = document_id;
    parameters_.buffer = buffer;
    parameters_.options = options;

    if (upsert_error_code_) {
      return nonstd::make_unexpected(upsert_error_code_);
    } else {
      return CouchbaseUpsertResult{bucket_name_, 1, 2, 3, 4};
    }
  }

 private:
  UpsertParameters& parameters_;
  std::error_code& upsert_error_code_;
  std::string bucket_name_;
};

class MockCouchbaseClusterService : public controllers::CouchbaseClusterService {
 public:
  using CouchbaseClusterService::CouchbaseClusterService;
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void onEnable() override {}
  void notifyStop() override {}

  std::unique_ptr<CouchbaseCollection> getCollection(std::string_view bucket_name, std::string_view scope_name, std::string_view collection_name) override {
    get_collection_parameters_.bucket_name = bucket_name;
    get_collection_parameters_.scope_name = scope_name;
    get_collection_parameters_.collection_name = collection_name;
    if (!get_collection_succeeds_) {
      return nullptr;
    } else {
      return std::make_unique<MockCouchbaseCollection>(upsert_parameters_, upsert_error_code_, bucket_name);
    }
  }

  GetCollectionParameters getGetCollectionParameters() const {
    return get_collection_parameters_;
  }

  UpsertParameters getUpsertParameters() const {
    return upsert_parameters_;
  }

  void setUpsertErrorCode(const std::error_code& error) {
    upsert_error_code_ = error;
  }

  void setGetCollectionSucceeds(bool succeeds) {
    get_collection_succeeds_ = succeeds;
  }

  bool getGetCollectionSucceeds() const {
    return get_collection_succeeds_;
  }

 private:
  GetCollectionParameters get_collection_parameters_;
  UpsertParameters upsert_parameters_;
  std::error_code upsert_error_code_;
  bool get_collection_succeeds_{true};
};
}  // namespace org::apache::nifi::minifi::couchbase::test
