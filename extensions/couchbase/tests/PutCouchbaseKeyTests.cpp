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

#include "unit/TestBase.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "processors/PutCouchbaseKey.h"
#include "MockCouchbaseClusterService.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::couchbase::test {

REGISTER_RESOURCE(MockCouchbaseClusterService, ControllerService);

const std::string TEST_UUID = "a53f0e78-b91a-4a82-939b-639174edb00b";

struct ExpectedCallOptions {
  std::string bucket_name;
  std::string scope_name;
  std::string collection_name;
  ::couchbase::persist_to persist_to;
  ::couchbase::replicate_to replicate_to;
  CouchbaseValueType document_type;
  std::string doc_id;
};

class PutCouchbaseKeyTestController : public TestController {
 public:
  PutCouchbaseKeyTestController()
      : controller_(minifi::test::utils::make_processor<processors::PutCouchbaseKey>("PutCouchbaseKey")),
        proc_(controller_.getProcessor()) {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<controllers::CouchbaseClusterService>();
    LogTestController::getInstance().setDebug<processors::PutCouchbaseKey>();
    auto controller_service_node = controller_.plan->addController("MockCouchbaseClusterService", "MockCouchbaseClusterService");
    mock_couchbase_cluster_service_ = controller_service_node->getControllerServiceImplementation<MockCouchbaseClusterService>();
    gsl_Assert(mock_couchbase_cluster_service_);
    REQUIRE(proc_->setProperty(processors::PutCouchbaseKey::CouchbaseClusterControllerService.name, "MockCouchbaseClusterService"));
  }

  [[nodiscard]] static std::vector<std::byte> stringToByteVector(const std::string& str) {
    std::vector<std::byte> byte_vector;
    byte_vector.reserve(str.size());
    for (char ch : str) {
      byte_vector.push_back(static_cast<std::byte>(ch));
    }
    return byte_vector;
  }

  void verifyResults(const minifi::test::ProcessorTriggerResult& results, const minifi::core::Relationship& expected_result, const ExpectedCallOptions& expected_call_options,
      const std::string& input) const {
    std::shared_ptr<core::FlowFile> flow_file;
    if (expected_result == processors::PutCouchbaseKey::Success) {
      REQUIRE(results.at(processors::PutCouchbaseKey::Success).size() == 1);
      REQUIRE(results.at(processors::PutCouchbaseKey::Failure).empty());
      REQUIRE(results.at(processors::PutCouchbaseKey::Retry).empty());
      flow_file = results.at(processors::PutCouchbaseKey::Success)[0];
    } else if (expected_result == processors::PutCouchbaseKey::Failure) {
      REQUIRE(results.at(processors::PutCouchbaseKey::Success).empty());
      REQUIRE(results.at(processors::PutCouchbaseKey::Failure).size() == 1);
      REQUIRE(results.at(processors::PutCouchbaseKey::Retry).empty());
      flow_file = results.at(processors::PutCouchbaseKey::Failure)[0];
      REQUIRE(LogTestController::getInstance().contains("Failed to upsert document", 1s));
    } else {
      REQUIRE(results.at(processors::PutCouchbaseKey::Success).empty());
      REQUIRE(results.at(processors::PutCouchbaseKey::Failure).empty());
      REQUIRE(results.at(processors::PutCouchbaseKey::Retry).size() == 1);
      flow_file = results.at(processors::PutCouchbaseKey::Retry)[0];
    }

    auto get_collection_parameters = mock_couchbase_cluster_service_->getCollectionParameter();
    CHECK(get_collection_parameters.bucket_name == expected_call_options.bucket_name);
    CHECK(get_collection_parameters.collection_name == expected_call_options.collection_name);
    CHECK(get_collection_parameters.scope_name == expected_call_options.scope_name);

    auto upsert_parameters = mock_couchbase_cluster_service_->getUpsertParameters();
    CHECK(upsert_parameters.document_type == expected_call_options.document_type);
    auto expected_doc_id = expected_call_options.doc_id.empty() ? TEST_UUID : expected_call_options.doc_id;
    CHECK(upsert_parameters.document_id == expected_doc_id);
    CHECK(upsert_parameters.buffer == stringToByteVector(input));

    auto upsert_options = upsert_parameters.options.build();
    CHECK(upsert_options.persist_to == expected_call_options.persist_to);
    CHECK(upsert_options.replicate_to == expected_call_options.replicate_to);

    if (expected_result != processors::PutCouchbaseKey::Success) {
      return;
    }

    CHECK(flow_file->getAttribute("couchbase.bucket").value() == expected_call_options.bucket_name);
    CHECK(flow_file->getAttribute("couchbase.doc.id").value() == expected_doc_id);
    CHECK(flow_file->getAttribute("couchbase.doc.cas").value() == std::to_string(COUCHBASE_PUT_RESULT_CAS));
    CHECK(flow_file->getAttribute("couchbase.doc.sequence.number").value() == std::to_string(COUCHBASE_PUT_RESULT_SEQUENCE_NUMBER));
    CHECK(flow_file->getAttribute("couchbase.partition.uuid").value() == std::to_string(COUCHBASE_PUT_RESULT_PARTITION_UUID));
    CHECK(flow_file->getAttribute("couchbase.partition.id").value() == std::to_string(COUCHBASE_PUT_RESULT_PARTITION_ID));
  }

 protected:
  minifi::test::SingleProcessorTestController controller_;
  core::Processor* proc_ = nullptr;
  std::shared_ptr<MockCouchbaseClusterService> mock_couchbase_cluster_service_;
};

TEST_CASE_METHOD(PutCouchbaseKeyTestController, "Invalid Couchbase cluster controller service", "[putcouchbasekey]") {
  REQUIRE(proc_->setProperty(processors::PutCouchbaseKey::CouchbaseClusterControllerService.name, "invalid"));
  REQUIRE_THROWS_AS(controller_.trigger({minifi::test::InputFlowFileData{"{\"name\": \"John\"}\n{\"name\": \"Jill\"}", {{"uuid", TEST_UUID}}}}), minifi::Exception);
}

TEST_CASE_METHOD(PutCouchbaseKeyTestController, "Can't set empty bucket name", "[getcouchbasekey]") {
  CHECK_FALSE(proc_->setProperty(processors::PutCouchbaseKey::BucketName.name, ""));
}

TEST_CASE_METHOD(PutCouchbaseKeyTestController, "Empty evaluated bucket name", "[getcouchbasekey]") {
  CHECK(proc_->setProperty(processors::PutCouchbaseKey::BucketName.name, "${bucket_name}"));
  auto results = controller_.trigger({minifi::test::InputFlowFileData{"couchbase_id"}});
  REQUIRE(results[processors::PutCouchbaseKey::Failure].size() == 1);
  REQUIRE(LogTestController::getInstance().contains("Bucket '' is invalid or empty!", 1s));
}

TEST_CASE_METHOD(PutCouchbaseKeyTestController, "Put succeeeds with default properties", "[putcouchbasekey]") {
  REQUIRE(proc_->setProperty(processors::PutCouchbaseKey::BucketName.name, "mybucket"));
  const std::string input = "{\"name\": \"John\"}\n{\"name\": \"Jill\"}";
  auto results = controller_.trigger({minifi::test::InputFlowFileData{input, {{"uuid", TEST_UUID}}}});
  verifyResults(results, processors::PutCouchbaseKey::Success, ExpectedCallOptions{"mybucket", "_default", "_default",
    ::couchbase::persist_to::none, ::couchbase::replicate_to::none, CouchbaseValueType::Json, ""}, input);
}

TEST_CASE_METHOD(PutCouchbaseKeyTestController, "Put succeeeds with optional properties", "[putcouchbasekey]") {
  REQUIRE(proc_->setProperty(processors::PutCouchbaseKey::BucketName.name, "mybucket"));
  REQUIRE(proc_->setProperty(processors::PutCouchbaseKey::ScopeName.name, "scope1"));
  REQUIRE(proc_->setProperty(processors::PutCouchbaseKey::CollectionName.name, "collection1"));
  REQUIRE(proc_->setProperty(processors::PutCouchbaseKey::DocumentType.name, "Binary"));
  REQUIRE(proc_->setProperty(processors::PutCouchbaseKey::DocumentId.name, "important_doc"));
  REQUIRE(proc_->setProperty(processors::PutCouchbaseKey::PersistTo.name, "ACTIVE"));
  REQUIRE(proc_->setProperty(processors::PutCouchbaseKey::ReplicateTo.name, "TWO"));
  const std::string input = "{\"name\": \"John\"}\n{\"name\": \"Jill\"}";
  auto results = controller_.trigger({minifi::test::InputFlowFileData{input, {{"uuid", TEST_UUID}}}});
  verifyResults(results, processors::PutCouchbaseKey::Success, ExpectedCallOptions{"mybucket", "scope1", "collection1", ::couchbase::persist_to::active,
    ::couchbase::replicate_to::two, CouchbaseValueType::Binary, "important_doc"}, input);
}

TEST_CASE_METHOD(PutCouchbaseKeyTestController, "Put fails with default properties", "[putcouchbasekey]") {
  REQUIRE(proc_->setProperty(processors::PutCouchbaseKey::BucketName.name, "mybucket"));
  mock_couchbase_cluster_service_->setUpsertError(CouchbaseErrorType::FATAL);
  const std::string input = "{\"name\": \"John\"}\n{\"name\": \"Jill\"}";
  auto results = controller_.trigger({minifi::test::InputFlowFileData{input, {{"uuid", TEST_UUID}}}});
  verifyResults(results, processors::PutCouchbaseKey::Failure, ExpectedCallOptions{"mybucket", "_default", "_default", ::couchbase::persist_to::none, ::couchbase::replicate_to::none,
    CouchbaseValueType::Json, ""}, input);
}

TEST_CASE_METHOD(PutCouchbaseKeyTestController, "FlowFile is transferred to retry relationship when temporary error is returned", "[putcouchbasekey]") {
  REQUIRE(proc_->setProperty(processors::PutCouchbaseKey::BucketName.name, "mybucket"));
  mock_couchbase_cluster_service_->setUpsertError(CouchbaseErrorType::TEMPORARY);
  const std::string input = "{\"name\": \"John\"}\n{\"name\": \"Jill\"}";
  auto results = controller_.trigger({minifi::test::InputFlowFileData{input, {{"uuid", TEST_UUID}}}});
  verifyResults(results, processors::PutCouchbaseKey::Retry, ExpectedCallOptions{"mybucket", "_default", "_default", ::couchbase::persist_to::none, ::couchbase::replicate_to::none,
    CouchbaseValueType::Json, ""}, input);
}

}  // namespace org::apache::nifi::minifi::couchbase::test
