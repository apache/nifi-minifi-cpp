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
#include <memory>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "s3/MultipartUploadStateStorage.h"
#include "utils/Environment.h"
#include "MockS3RequestSender.h"
#include "controllers/VolatileMapStateStorage.h"
#include "controllers/keyvalue/KeyValueStateManager.h"

namespace org::apache::nifi::minifi::test {

class MultipartUploadStateStorageTestFixture {
 public:
  MultipartUploadStateStorageTestFixture() {
    LogTestController::getInstance().setDebug<minifi::aws::s3::MultipartUploadStateStorage>();
    const auto storage_uuid = minifi::utils::IdGenerator::getIdGenerator()->generate();
    state_storage_ = std::make_unique<minifi::controllers::VolatileMapStateStorage>(core::controller::ControllerServiceMetadata{
      .uuid = storage_uuid,
      .name = "KeyValueStateStorage",
      .logger = logging::LoggerFactory<minifi::controllers::VolatileMapStateStorage>::getLogger(storage_uuid)
    });
    state_manager_ = std::make_unique<minifi::controllers::KeyValueStateManager>(minifi::utils::IdGenerator::getIdGenerator()->generate(), gsl::make_not_null(state_storage_.get()));
    upload_storage_ = std::make_unique<minifi::aws::s3::MultipartUploadStateStorage>(gsl::make_not_null(state_manager_.get()));
  }

 protected:
  std::unique_ptr<minifi::controllers::KeyValueStateStorage> state_storage_;
  std::unique_ptr<core::StateManager> state_manager_;
  std::unique_ptr<minifi::aws::s3::MultipartUploadStateStorage> upload_storage_;
};

TEST_CASE_METHOD(MultipartUploadStateStorageTestFixture, "Store and get current key state", "[s3StateStorage]") {
  REQUIRE(upload_storage_->getState("test_bucket", "key") == std::nullopt);
  minifi::aws::s3::MultipartUploadState state("id1", 50_MiB, 200_MiB, Aws::Utils::DateTime::Now());
  state.uploaded_parts = 2;
  state.uploaded_size = 100_MiB;
  state.uploaded_etags = {"etag1", "etag2"};
  upload_storage_->storeState("test_bucket", "key", state);
  REQUIRE(*upload_storage_->getState("test_bucket", "key") == state);
}

TEST_CASE_METHOD(MultipartUploadStateStorageTestFixture, "Get key upload state from multiple keys and buckets", "[s3StateStorage]") {
  minifi::aws::s3::MultipartUploadState state1("id1", 50_MiB, 200_MiB, Aws::Utils::DateTime::Now());
  state1.uploaded_parts = 3;
  state1.uploaded_size = 150_MiB;
  state1.uploaded_etags = {"etag1", "etag2", "etag3"};
  upload_storage_->storeState("old_bucket", "key1", state1);
  minifi::aws::s3::MultipartUploadState state2("id2", 50_MiB, 200_MiB, Aws::Utils::DateTime::Now());
  state2.uploaded_parts = 2;
  state2.uploaded_size = 100_MiB;
  state2.uploaded_etags = {"etag4", "etag5"};
  upload_storage_->storeState("test_bucket", "key1", state2);
  minifi::aws::s3::MultipartUploadState state3("id3", 50_MiB, 400_MiB, Aws::Utils::DateTime::Now());
  state3.uploaded_parts = 4;
  state3.uploaded_size = 200_MiB;
  state3.uploaded_etags = {"etag6", "etag7", "etag9", "etag8"};
  upload_storage_->storeState("test_bucket", "key2", state3);
  minifi::aws::s3::MultipartUploadState state4("id2", 50_MiB, 200_MiB, Aws::Utils::DateTime::Now());
  state2.uploaded_parts = 3;
  state2.uploaded_size = 150_MiB;
  state2.uploaded_etags = {"etag4", "etag5", "etag10"};
  upload_storage_->storeState("test_bucket", "key1", state4);
  REQUIRE(*upload_storage_->getState("test_bucket", "key1") == state4);
  REQUIRE(*upload_storage_->getState("old_bucket", "key1") == state1);
  REQUIRE(*upload_storage_->getState("test_bucket", "key2") == state3);
}

TEST_CASE_METHOD(MultipartUploadStateStorageTestFixture, "Remove state", "[s3StateStorage]") {
  minifi::aws::s3::MultipartUploadState state1("id1", 50_MiB, 200_MiB, Aws::Utils::DateTime::Now());
  state1.uploaded_parts = 3;
  state1.uploaded_size = 150_MiB;
  state1.uploaded_etags = {"etag1", "etag2", "etag3"};
  upload_storage_->storeState("old_bucket", "key1", state1);
  minifi::aws::s3::MultipartUploadState state2("id2", 50_MiB, 200_MiB, Aws::Utils::DateTime::Now());
  state2.uploaded_parts = 2;
  state2.uploaded_size = 100_MiB;
  state2.uploaded_etags = {"etag4", "etag5"};
  upload_storage_->storeState("test_bucket", "key1", state2);
  minifi::aws::s3::MultipartUploadState state3("id3", 50_MiB, 400_MiB, Aws::Utils::DateTime::Now());
  state3.uploaded_parts = 4;
  state3.uploaded_size = 200_MiB;
  state3.uploaded_etags = {"etag6", "etag7", "etag9", "etag8"};
  upload_storage_->storeState("test_bucket", "key2", state3);
  REQUIRE(*upload_storage_->getState("old_bucket", "key1") == state1);
  REQUIRE(upload_storage_->getState("test_bucket", "key1") == state2);
  REQUIRE(*upload_storage_->getState("test_bucket", "key2") == state3);
  upload_storage_->removeState("test_bucket", "key1");
  REQUIRE(*upload_storage_->getState("old_bucket", "key1") == state1);
  REQUIRE(upload_storage_->getState("test_bucket", "key1") == std::nullopt);
  REQUIRE(*upload_storage_->getState("test_bucket", "key2") == state3);
}

TEST_CASE_METHOD(MultipartUploadStateStorageTestFixture, "Remove aged off state", "[s3StateStorage]") {
  using namespace std::literals::chrono_literals;
  minifi::aws::s3::MultipartUploadState state1("id1", 50_MiB, 200_MiB, Aws::Utils::DateTime::Now() - 10min);
  state1.uploaded_parts = 3;
  state1.uploaded_size = 150_MiB;
  state1.uploaded_etags = {"etag1", "etag2", "etag3"};
  upload_storage_->storeState("test_bucket", "key1", state1);
  minifi::aws::s3::MultipartUploadState state2("id2", 50_MiB, 200_MiB, Aws::Utils::DateTime::Now());
  state2.uploaded_parts = 2;
  state2.uploaded_size = 100_MiB;
  state2.uploaded_etags = {"etag4", "etag5"};
  upload_storage_->storeState("test_bucket", "key2", state2);
  upload_storage_->removeAgedStates(10min);
  CHECK(upload_storage_->getState("test_bucket", "key1") == std::nullopt);
  CHECK(upload_storage_->getState("test_bucket", "key2") == state2);
}

}  // namespace org::apache::nifi::minifi::test
