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

#include "TestBase.h"
#include "Catch.h"
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
    state_storage_ = std::make_unique<minifi::controllers::VolatileMapStateStorage>("KeyValueStateStorage");
    state_manager_ = std::make_unique<minifi::controllers::KeyValueStateManager>(utils::IdGenerator::getIdGenerator()->generate(), gsl::make_not_null(state_storage_.get()));
    upload_storage_ = std::make_unique<minifi::aws::s3::MultipartUploadStateStorage>(gsl::make_not_null(state_manager_.get()));
  }

 protected:
  std::unique_ptr<minifi::controllers::KeyValueStateStorage> state_storage_;
  std::unique_ptr<core::StateManager> state_manager_;
  std::unique_ptr<minifi::aws::s3::MultipartUploadStateStorage> upload_storage_;
};

TEST_CASE_METHOD(MultipartUploadStateStorageTestFixture, "Store and get current key state", "[s3StateStorage]") {
  REQUIRE(upload_storage_->getState("test_bucket", "key") == std::nullopt);
  minifi::aws::s3::MultipartUploadState state;
  state.upload_id = "id1";
  state.uploaded_parts = 2;
  state.uploaded_size = 100_MiB;
  state.part_size = 50_MiB;
  state.full_size = 200_MiB;
  state.upload_time = Aws::Utils::DateTime::CurrentTimeMillis();
  state.uploaded_etags = {"etag1", "etag2"};
  upload_storage_->storeState("test_bucket", "key", state);
  REQUIRE(*upload_storage_->getState("test_bucket", "key") == state);
}

TEST_CASE_METHOD(MultipartUploadStateStorageTestFixture, "Get key upload state from multiple keys and buckets", "[s3StateStorage]") {
  minifi::aws::s3::MultipartUploadState state1;
  state1.upload_id = "id1";
  state1.uploaded_parts = 3;
  state1.uploaded_size = 150_MiB;
  state1.part_size = 50_MiB;
  state1.full_size = 200_MiB;
  state1.upload_time = Aws::Utils::DateTime::CurrentTimeMillis();
  state1.uploaded_etags = {"etag1", "etag2", "etag3"};
  upload_storage_->storeState("old_bucket", "key1", state1);
  minifi::aws::s3::MultipartUploadState state2;
  state2.upload_id = "id2";
  state2.uploaded_parts = 2;
  state2.uploaded_size = 100_MiB;
  state2.part_size = 50_MiB;
  state2.full_size = 200_MiB;
  state2.upload_time = Aws::Utils::DateTime::CurrentTimeMillis();
  state2.uploaded_etags = {"etag4", "etag5"};
  upload_storage_->storeState("test_bucket", "key1", state2);
  minifi::aws::s3::MultipartUploadState state3;
  state3.upload_id = "id3";
  state3.uploaded_parts = 4;
  state3.uploaded_size = 200_MiB;
  state3.part_size = 50_MiB;
  state3.full_size = 400_MiB;
  state3.upload_time = Aws::Utils::DateTime::CurrentTimeMillis();
  state3.uploaded_etags = {"etag6", "etag7", "etag9", "etag8"};
  upload_storage_->storeState("test_bucket", "key2", state3);
  minifi::aws::s3::MultipartUploadState state4;
  state2.upload_id = "id2";
  state2.uploaded_parts = 3;
  state2.uploaded_size = 150_MiB;
  state2.part_size = 50_MiB;
  state2.full_size = 200_MiB;
  state2.upload_time = Aws::Utils::DateTime::CurrentTimeMillis();
  state2.uploaded_etags = {"etag4", "etag5", "etag10"};
  upload_storage_->storeState("test_bucket", "key1", state4);
  REQUIRE(*upload_storage_->getState("test_bucket", "key1") == state4);
  REQUIRE(*upload_storage_->getState("old_bucket", "key1") == state1);
  REQUIRE(*upload_storage_->getState("test_bucket", "key2") == state3);
}

TEST_CASE_METHOD(MultipartUploadStateStorageTestFixture, "Remove state", "[s3StateStorage]") {
  minifi::aws::s3::MultipartUploadState state1;
  state1.upload_id = "id1";
  state1.uploaded_parts = 3;
  state1.uploaded_size = 150_MiB;
  state1.part_size = 50_MiB;
  state1.full_size = 200_MiB;
  state1.upload_time = Aws::Utils::DateTime::CurrentTimeMillis();
  state1.uploaded_etags = {"etag1", "etag2", "etag3"};
  upload_storage_->storeState("old_bucket", "key1", state1);
  minifi::aws::s3::MultipartUploadState state2;
  state2.upload_id = "id2";
  state2.uploaded_parts = 2;
  state2.uploaded_size = 100_MiB;
  state2.part_size = 50_MiB;
  state2.full_size = 200_MiB;
  state2.upload_time = Aws::Utils::DateTime::CurrentTimeMillis();
  state2.uploaded_etags = {"etag4", "etag5"};
  upload_storage_->storeState("test_bucket", "key1", state2);
  minifi::aws::s3::MultipartUploadState state3;
  state3.upload_id = "id3";
  state3.uploaded_parts = 4;
  state3.uploaded_size = 200_MiB;
  state3.part_size = 50_MiB;
  state3.full_size = 400_MiB;
  state3.upload_time = Aws::Utils::DateTime::CurrentTimeMillis();
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
  minifi::aws::s3::MultipartUploadState state1;
  state1.upload_id = "id1";
  state1.uploaded_parts = 3;
  state1.uploaded_size = 150_MiB;
  state1.part_size = 50_MiB;
  state1.full_size = 200_MiB;
  state1.upload_time = Aws::Utils::DateTime(Aws::Utils::DateTime::CurrentTimeMillis()) - std::chrono::minutes(10);
  state1.uploaded_etags = {"etag1", "etag2", "etag3"};
  upload_storage_->storeState("test_bucket", "key1", state1);
  minifi::aws::s3::MultipartUploadState state2;
  state2.upload_id = "id2";
  state2.uploaded_parts = 2;
  state2.uploaded_size = 100_MiB;
  state2.part_size = 50_MiB;
  state2.full_size = 200_MiB;
  state2.upload_time = Aws::Utils::DateTime::CurrentTimeMillis();
  state2.uploaded_etags = {"etag4", "etag5"};
  upload_storage_->storeState("test_bucket", "key2", state2);
  upload_storage_->removeAgedStates(std::chrono::milliseconds(10));
  REQUIRE(upload_storage_->getState("test_bucket", "key1") == std::nullopt);
  REQUIRE(upload_storage_->getState("test_bucket", "key2") == state2);
}

}  // namespace org::apache::nifi::minifi::test
