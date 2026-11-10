// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once
#include <utility>
#include <string>
#include "CouchbaseClusterService.h"
#include "unit/Catch.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::couchbase::test {

const std::uint64_t COUCHBASE_PUT_RESULT_CAS = 9876;
const std::uint64_t COUCHBASE_PUT_RESULT_SEQUENCE_NUMBER = 345;
const std::uint64_t COUCHBASE_PUT_RESULT_PARTITION_UUID = 7890123456;
const std::uint16_t COUCHBASE_PUT_RESULT_PARTITION_ID = 1234;
const std::string COUCHBASE_GET_RESULT_EXPIRY = "2024/10/14 09:37:43.000Z";
const std::string COUCHBASE_GET_RESULT_CONTENT = "abc";
const uint64_t COUCHBASE_GET_RESULT_CAS = 1234567;

struct UpsertParameters {
  CouchbaseValueType document_type;
  std::string document_id;
  std::vector<std::byte> buffer;
  ::couchbase::upsert_options options;
};
struct GetParameters {
  std::string document_id;
  CouchbaseValueType document_type;
};

class MockCouchbaseClusterService : public controllers::CouchbaseClusterService {
 public:
  using CouchbaseClusterService::CouchbaseClusterService;
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void onEnable() override {}
  void notifyStop() override {}

  std::expected<CouchbaseUpsertResult, CouchbaseErrorType> upsert(const CouchbaseCollection& collection, CouchbaseValueType document_type, const std::string& document_id,
      const std::vector<std::byte>& buffer, const ::couchbase::upsert_options& options) override {
    collection_ = collection;
    upsert_parameters_.document_type = document_type;
    upsert_parameters_.document_id = document_id;
    upsert_parameters_.buffer = buffer;
    upsert_parameters_.options = options;

    if (upsert_error_) {
      return std::unexpected{*upsert_error_};
    } else {
      return CouchbaseUpsertResult{{collection_.bucket_name, COUCHBASE_PUT_RESULT_CAS}, COUCHBASE_PUT_RESULT_SEQUENCE_NUMBER, COUCHBASE_PUT_RESULT_PARTITION_UUID, COUCHBASE_PUT_RESULT_PARTITION_ID};
    }
  }

  std::expected<CouchbaseGetResult, CouchbaseErrorType> get(const CouchbaseCollection& collection, const std::string& document_id, CouchbaseValueType document_type) override {
    collection_ = collection;
    get_parameters_.document_id = document_id;
    get_parameters_.document_type = document_type;

    if (get_error_) {
      return std::unexpected{*get_error_};
    } else {
      if (document_type == CouchbaseValueType::String) {
        return CouchbaseGetResult{{collection_.bucket_name, COUCHBASE_GET_RESULT_CAS}, COUCHBASE_GET_RESULT_EXPIRY, COUCHBASE_GET_RESULT_CONTENT};
      }
      return CouchbaseGetResult{{collection_.bucket_name, COUCHBASE_GET_RESULT_CAS}, COUCHBASE_GET_RESULT_EXPIRY,
        std::vector<std::byte>{static_cast<std::byte>('a'), static_cast<std::byte>('b'), static_cast<std::byte>('c')}};
    }
  }

  UpsertParameters getUpsertParameters() const {
    return upsert_parameters_;
  }

  GetParameters getGetParameters() const {
    return get_parameters_;
  }

  CouchbaseCollection getCollectionParameter() const {
    return collection_;
  }

  void setUpsertError(const CouchbaseErrorType upsert_error) {
    upsert_error_ = upsert_error;
  }

  void setGetError(const CouchbaseErrorType get_error) {
    get_error_ = get_error;
  }

 private:
  CouchbaseCollection collection_;
  UpsertParameters upsert_parameters_;
  GetParameters get_parameters_;
  std::optional<CouchbaseErrorType> upsert_error_;
  std::optional<CouchbaseErrorType> get_error_;
};
}  // namespace org::apache::nifi::minifi::couchbase::test
