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
#include "processors/GetCouchbaseKey.h"
#include "MockCouchbaseClusterService.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::couchbase::test {

REGISTER_RESOURCE(MockCouchbaseClusterService, ControllerService);

struct ExpectedCallOptions {
  std::string bucket_name;
  std::string scope_name;
  std::string collection_name;
  std::string doc_id;
  couchbase::CouchbaseValueType document_type;
};

class GetCouchbaseKeyTestController : public TestController {
 public:
  GetCouchbaseKeyTestController()
      : controller_(minifi::test::utils::make_processor<processors::GetCouchbaseKey>("GetCouchbaseKey")),
        proc_(controller_.getProcessor()) {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<controllers::CouchbaseClusterService>();
    LogTestController::getInstance().setDebug<processors::GetCouchbaseKey>();
    auto controller_service_node = controller_.plan->addController("MockCouchbaseClusterService", "MockCouchbaseClusterService");
    mock_couchbase_cluster_service_ = controller_service_node->getControllerServiceImplementation<MockCouchbaseClusterService>();
    gsl_Assert(mock_couchbase_cluster_service_);
    REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::CouchbaseClusterControllerService.name, "MockCouchbaseClusterService"));
  }

  void verifyResults(const minifi::test::ProcessorTriggerResult& results, const minifi::core::Relationship& expected_result, const ExpectedCallOptions& expected_call_options,
      const std::string& input) const {
    std::shared_ptr<core::FlowFile> flow_file;
    if (expected_result == processors::GetCouchbaseKey::Success) {
      REQUIRE(results.at(processors::GetCouchbaseKey::Success).size() == 1);
      REQUIRE(results.at(processors::GetCouchbaseKey::Failure).empty());
      REQUIRE(results.at(processors::GetCouchbaseKey::Retry).empty());
      flow_file = results.at(processors::GetCouchbaseKey::Success)[0];
    } else if (expected_result == processors::GetCouchbaseKey::Failure) {
      REQUIRE(results.at(processors::GetCouchbaseKey::Success).empty());
      REQUIRE(results.at(processors::GetCouchbaseKey::Failure).size() == 1);
      REQUIRE(results.at(processors::GetCouchbaseKey::Retry).empty());
      flow_file = results.at(processors::GetCouchbaseKey::Failure)[0];
      REQUIRE(LogTestController::getInstance().contains("Failed to get document", 1s));
    } else {
      REQUIRE(results.at(processors::GetCouchbaseKey::Success).empty());
      REQUIRE(results.at(processors::GetCouchbaseKey::Failure).empty());
      REQUIRE(results.at(processors::GetCouchbaseKey::Retry).size() == 1);
      flow_file = results.at(processors::GetCouchbaseKey::Retry)[0];
    }

    auto get_collection_parameters = mock_couchbase_cluster_service_->getCollectionParameter();
    CHECK(get_collection_parameters.bucket_name == expected_call_options.bucket_name);
    CHECK(get_collection_parameters.collection_name == expected_call_options.collection_name);
    CHECK(get_collection_parameters.scope_name == expected_call_options.scope_name);

    auto get_parameters = mock_couchbase_cluster_service_->getGetParameters();
    CHECK(get_parameters.document_id == expected_call_options.doc_id);
    CHECK(get_parameters.document_type == expected_call_options.document_type);

    if (expected_result != processors::GetCouchbaseKey::Success) {
      return;
    }

    CHECK(flow_file->getAttribute("couchbase.bucket").value() == expected_call_options.bucket_name);
    CHECK(flow_file->getAttribute("couchbase.doc.id").value() == expected_call_options.doc_id);
    CHECK(flow_file->getAttribute("couchbase.doc.cas").value() == std::to_string(COUCHBASE_GET_RESULT_CAS));
    CHECK(flow_file->getAttribute("couchbase.doc.expiry").value() == COUCHBASE_GET_RESULT_EXPIRY);
    std::string value = proc_->getProperty(processors::GetCouchbaseKey::PutValueToAttribute.name).value_or("");
    if (!value.empty()) {
      CHECK(flow_file->getAttribute(value).value() == COUCHBASE_GET_RESULT_CONTENT);
      CHECK(controller_.plan->getContent(flow_file) == input);
    } else {
      CHECK(controller_.plan->getContent(flow_file) == COUCHBASE_GET_RESULT_CONTENT);
    }
  }

 protected:
  minifi::test::SingleProcessorTestController controller_;
  core::Processor* proc_ = nullptr;
  std::shared_ptr<MockCouchbaseClusterService> mock_couchbase_cluster_service_;
};

TEST_CASE_METHOD(GetCouchbaseKeyTestController, "Invalid Couchbase cluster controller service", "[getcouchbasekey]") {
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::CouchbaseClusterControllerService.name, "invalid"));
  REQUIRE_THROWS_AS(controller_.trigger({minifi::test::InputFlowFileData{"couchbase_id"}}), minifi::Exception);
}

TEST_CASE_METHOD(GetCouchbaseKeyTestController, "Can't set empty bucket name", "[getcouchbasekey]") {
  CHECK_FALSE(proc_->setProperty(processors::GetCouchbaseKey::BucketName.name, ""));
}

TEST_CASE_METHOD(GetCouchbaseKeyTestController, "Empty evaluated bucket name", "[getcouchbasekey]") {
  CHECK(proc_->setProperty(processors::GetCouchbaseKey::BucketName.name, "${missing_attr}"));
  auto results = controller_.trigger({minifi::test::InputFlowFileData{"couchbase_id"}});
  REQUIRE(results[processors::GetCouchbaseKey::Failure].size() == 1);
  REQUIRE(LogTestController::getInstance().contains("Bucket '' is invalid or empty!", 1s));
}

TEST_CASE_METHOD(GetCouchbaseKeyTestController, "Document ID is empty and no content is present to use", "[getcouchbasekey]") {
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::BucketName.name, "mybucket"));
  auto results = controller_.trigger({minifi::test::InputFlowFileData{""}});
  REQUIRE(results.at(processors::GetCouchbaseKey::Success).empty());
  REQUIRE(results.at(processors::GetCouchbaseKey::Failure).size() == 1);
  REQUIRE(results.at(processors::GetCouchbaseKey::Retry).empty());
  REQUIRE(LogTestController::getInstance().contains("Document ID is empty, transferring FlowFile to failure relationship", 1s));
}

TEST_CASE_METHOD(GetCouchbaseKeyTestController, "Document ID evaluates to be empty", "[getcouchbasekey]") {
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::BucketName.name, "mybucket"));
  CHECK(proc_->setProperty(processors::GetCouchbaseKey::DocumentId.name, "${missing_attr}"));
  const std::string input = "couchbase_id";
  auto results = controller_.trigger({minifi::test::InputFlowFileData{input}});
  verifyResults(results, processors::GetCouchbaseKey::Success, ExpectedCallOptions{"mybucket", "_default", "_default", input, couchbase::CouchbaseValueType::Json}, input);
}

TEST_CASE_METHOD(GetCouchbaseKeyTestController, "Get succeeeds with default properties", "[getcouchbasekey]") {
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::BucketName.name, "mybucket"));
  const std::string input = "couchbase_id";
  auto results = controller_.trigger({minifi::test::InputFlowFileData{input}});
  verifyResults(results, processors::GetCouchbaseKey::Success, ExpectedCallOptions{"mybucket", "_default", "_default", input, couchbase::CouchbaseValueType::Json}, input);
}

TEST_CASE_METHOD(GetCouchbaseKeyTestController, "Get succeeeds with optional properties", "[getcouchbasekey]") {
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::BucketName.name, "mybucket"));
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::ScopeName.name, "scope1"));
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::CollectionName.name, "collection1"));
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::DocumentId.name, "important_doc"));
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::DocumentType.name, "Binary"));
  auto results = controller_.trigger({minifi::test::InputFlowFileData{""}});
  verifyResults(results, processors::GetCouchbaseKey::Success, ExpectedCallOptions{"mybucket", "scope1", "collection1", "important_doc", couchbase::CouchbaseValueType::Binary}, "");
}

TEST_CASE_METHOD(GetCouchbaseKeyTestController, "Get fails with default properties", "[getcouchbasekey]") {
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::BucketName.name, "mybucket"));
  mock_couchbase_cluster_service_->setGetError(CouchbaseErrorType::FATAL);
  const std::string input = "couchbase_id";
  auto results = controller_.trigger({minifi::test::InputFlowFileData{input}});
  verifyResults(results, processors::GetCouchbaseKey::Failure, ExpectedCallOptions{"mybucket", "_default", "_default", input, couchbase::CouchbaseValueType::Json}, input);
}

TEST_CASE_METHOD(GetCouchbaseKeyTestController, "FlowFile is transferred to retry relationship when temporary error is returned", "[getcouchbasekey]") {
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::BucketName.name, "mybucket"));
  mock_couchbase_cluster_service_->setGetError(CouchbaseErrorType::TEMPORARY);
  const std::string input = "couchbase_id";
  auto results = controller_.trigger({minifi::test::InputFlowFileData{input}});
  verifyResults(results, processors::GetCouchbaseKey::Retry, ExpectedCallOptions{"mybucket", "_default", "_default", input, couchbase::CouchbaseValueType::Json}, input);
}

TEST_CASE_METHOD(GetCouchbaseKeyTestController, "Get result is written to attribute", "[getcouchbasekey]") {
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::BucketName.name, "mybucket"));
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::DocumentType.name, "String"));
  REQUIRE(proc_->setProperty(processors::GetCouchbaseKey::PutValueToAttribute.name, "myattribute"));
  const std::string input = "couchbase_id";
  auto results = controller_.trigger({minifi::test::InputFlowFileData{input}});
  verifyResults(results, processors::GetCouchbaseKey::Success, ExpectedCallOptions{"mybucket", "_default", "_default", input, couchbase::CouchbaseValueType::String}, input);
}

}  // namespace org::apache::nifi::minifi::couchbase::test
