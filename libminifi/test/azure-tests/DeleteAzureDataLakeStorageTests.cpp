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

#include "AzureDataLakeStorageTestsFixture.h"
#include "processors/DeleteAzureDataLakeStorage.h"
#include "controllerservices/AzureStorageCredentialsService.h"

namespace {

using namespace std::chrono_literals;

using DeleteAzureDataLakeStorageTestsFixture = AzureDataLakeStorageTestsFixture<minifi::azure::processors::DeleteAzureDataLakeStorage>;

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Azure storage credentials service is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(azure_data_lake_storage_, minifi::azure::processors::DeleteAzureDataLakeStorage::AzureStorageCredentialsService.getName(), "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  REQUIRE(getFailedFlowFileContents().size() == 0);
}

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Filesystem name is not set", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(update_attribute_, "test.filesystemname", "", true);
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "Filesystem Name '' is invalid or empty!"));
  auto failed_flowfiles = getFailedFlowFileContents();
  REQUIRE(failed_flowfiles.size() == 1);
  REQUIRE(failed_flowfiles[0] == TEST_DATA);
}

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Connection String is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString.getName(), "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  REQUIRE(getFailedFlowFileContents().size() == 0);
}

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Delete file succeeds", "[azureDataLakeStorageDelete]") {
  test_controller_.runSession(plan_, true);
  REQUIRE(getFailedFlowFileContents().size() == 0);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "key:filename value:" + GETFILE_FILE_NAME));
}

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Delete file fails", "[azureDataLakeStorageDelete]") {
  mock_data_lake_storage_client_ptr_->setDeleteFailure(true);
  test_controller_.runSession(plan_, true);
  auto failed_flowfiles = getFailedFlowFileContents();
  REQUIRE(failed_flowfiles.size() == 1);
  REQUIRE(failed_flowfiles[0] == TEST_DATA);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(!verifyLogLinePresenceInPollTime(0s, "key:filename value:"));
}

}
