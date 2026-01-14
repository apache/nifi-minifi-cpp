# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

@ENABLE_AZURE
Feature: Sending data from MiNiFi-C++ to an Azure storage server
  In order to transfer data to interact with Azure servers
  As a user of MiNiFi
  I need to have a PutAzureBlobStorage processor

  Scenario: A MiNiFi instance can upload data to Azure blob storage
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "#test_data$123$#"
    And a PutAzureBlobStorage processor set up to communicate with an Azure blob storage
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutAzureBlobStorage
    And the "success" relationship of the PutAzureBlobStorage processor is connected to the PutFile
    And the "failure" relationship of the PutAzureBlobStorage processor is connected to the PutAzureBlobStorage
    And PutFile's success relationship is auto-terminated

    And an Azure storage server is set up
    When the MiNiFi instance starts up

    Then a single file with the content "#test_data$123$#" is placed in the "/tmp/output" directory in less than 20 seconds
    And the object on the Azure storage server is "#test_data$123$#"

  Scenario: A MiNiFi instance can delete blob from Azure blob storage
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a DeleteAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Blob" property of the DeleteAzureBlobStorage processor is set to "test"
    And the "success" relationship of the GenerateFlowFile processor is connected to the DeleteAzureBlobStorage
    And an Azure storage server is set up
    And test blob "test" is created on Azure blob storage

    When the MiNiFi instance starts up

    Then the Azure blob storage becomes empty in 30 seconds

  Scenario: A MiNiFi instance can delete blob from Azure blob storage including snapshots
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a DeleteAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Blob" property of the DeleteAzureBlobStorage processor is set to "test"
    And the "Delete Snapshots Option" property of the DeleteAzureBlobStorage processor is set to "Include Snapshots"
    And the "success" relationship of the GenerateFlowFile processor is connected to the DeleteAzureBlobStorage
    And DeleteAzureBlobStorage's success relationship is auto-terminated
    And an Azure storage server is set up
    And test blob "test" is created on Azure blob storage with a snapshot

    When the MiNiFi instance starts up
    Then the Azure blob storage becomes empty in 30 seconds

  Scenario: A MiNiFi instance can delete blob snapshots from Azure blob storage
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a DeleteAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Blob" property of the DeleteAzureBlobStorage processor is set to "test"
    And the "Delete Snapshots Option" property of the DeleteAzureBlobStorage processor is set to "Delete Snapshots Only"
    And the "success" relationship of the GenerateFlowFile processor is connected to the DeleteAzureBlobStorage
    And DeleteAzureBlobStorage's success relationship is auto-terminated
    And an Azure storage server is set up
    And test blob "test" is created on Azure blob storage with a snapshot

    When the MiNiFi instance starts up
    Then the blob and snapshot count becomes 1 in 30 seconds

  Scenario: A MiNiFi instance can fetch a blob from Azure blob storage
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a directory at "/tmp/input" has a file with the content "dummy"
    And a FetchAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Blob" property of the FetchAzureBlobStorage processor is set to "test"
    And the "Range Start" property of the FetchAzureBlobStorage processor is set to "6"
    And the "Range Length" property of the FetchAzureBlobStorage processor is set to "5"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the FetchAzureBlobStorage
    And the "success" relationship of the FetchAzureBlobStorage processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    And an Azure storage server is set up

    When the MiNiFi instance starts up
    And test blob "test" with the content "#test_data_123$#" is created on Azure blob storage

    Then a single file with the content "data_" is placed in the "/tmp/output" directory in less than 20 seconds

  Scenario: A MiNiFi instance can list a container on Azure blob storage
    Given a ListAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Prefix" property of the ListAzureBlobStorage processor is set to "test"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And the "success" relationship of the ListAzureBlobStorage processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated
    And an Azure storage server is set up

    When the MiNiFi instance starts up
    And test blob "test_1" with the content "data_1" is created on Azure blob storage
    And test blob "test_2" with the content "data_2" is created on Azure blob storage
    And test blob "other_test" with the content "data_3" is created on Azure blob storage

    Then the Minifi logs contain the following message: "key:azure.blobname value:test_1" in less than 60 seconds
    Then the Minifi logs contain the following message: "key:azure.blobname value:test_2" in less than 60 seconds
    And the Minifi logs do not contain the following message: "key:azure.blobname value:other_test" after 0 seconds

  Scenario Outline: A MiNiFi instance can upload data to Azure blob storage through a proxy
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "#test_data$123$#" is present in "/tmp/input"
    And a PutAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Proxy Configuration Service" property of the PutAzureBlobStorage processor is set to "ProxyConfigurationService"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutAzureBlobStorage
    And the "success" relationship of the PutAzureBlobStorage processor is connected to the PutFile
    And the "failure" relationship of the PutAzureBlobStorage processor is connected to the PutAzureBlobStorage
    And a ProxyConfigurationService controller service is set up with <proxy type> proxy configuration

    And an Azure storage server is set up
    And the http proxy server is set up

    When all instances start up

    Then a single file with the content "#test_data$123$#" is placed in the "/tmp/output" directory in less than 60 seconds
    And the object on the Azure storage server is "#test_data$123$#"
    And no errors were generated on the http-proxy regarding "http://azure-storage-server-${scenario_id}:10000/devstoreaccount1/test-container/test-blob"

    Examples: Proxy Type
    | proxy type |
    | HTTP       |
    | HTTPS      |

  Scenario: A MiNiFi instance can delete blob from Azure blob storage through a proxy
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a DeleteAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Blob" property of the DeleteAzureBlobStorage processor is set to "test"
    And the "Proxy Configuration Service" property of the DeleteAzureBlobStorage processor is set to "ProxyConfigurationService"
    And the "success" relationship of the GenerateFlowFile processor is connected to the DeleteAzureBlobStorage
    And a ProxyConfigurationService controller service is set up with HTTP proxy configuration

    And an Azure storage server is set up
    And the http proxy server is set up

    When all instances start up
    And test blob "test" is created on Azure blob storage

    Then the Azure blob storage becomes empty in 30 seconds
    And no errors were generated on the http-proxy regarding "http://azure-storage-server-${scenario_id}:10000/devstoreaccount1/test-container/test"

  Scenario: A MiNiFi instance can fetch a blob from Azure blob storage through a proxy
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a file with the content "dummy" is present in "/tmp/input"
    And a FetchAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Blob" property of the FetchAzureBlobStorage processor is set to "test"
    And the "Range Start" property of the FetchAzureBlobStorage processor is set to "6"
    And the "Range Length" property of the FetchAzureBlobStorage processor is set to "5"
    And the "Proxy Configuration Service" property of the FetchAzureBlobStorage processor is set to "ProxyConfigurationService"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the FetchAzureBlobStorage
    And the "success" relationship of the FetchAzureBlobStorage processor is connected to the PutFile
    And a ProxyConfigurationService controller service is set up with HTTP proxy configuration

    And an Azure storage server is set up
    And the http proxy server is set up

    When all instances start up
    And test blob "test" with the content "#test_data$123$#" is created on Azure blob storage

    Then a single file with the content "data$" is placed in the "/tmp/output" directory in less than 60 seconds
    And no errors were generated on the http-proxy regarding "http://azure-storage-server-${scenario_id}:10000/devstoreaccount1/test-container/test"

  Scenario: A MiNiFi instance can list a container on Azure blob storage through a proxy
    Given a ListAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Prefix" property of the ListAzureBlobStorage processor is set to "test"
    And the "Proxy Configuration Service" property of the ListAzureBlobStorage processor is set to "ProxyConfigurationService"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And the "success" relationship of the ListAzureBlobStorage processor is connected to the LogAttribute
    And a ProxyConfigurationService controller service is set up with HTTP proxy configuration

    And an Azure storage server is set up
    And the http proxy server is set up

    When all instances start up
    And test blob "test_1" with the content "data_1" is created on Azure blob storage
    And test blob "test_2" with the content "data_2" is created on Azure blob storage
    And test blob "other_test" with the content "data_3" is created on Azure blob storage

    Then the Minifi logs contain the following message: "key:azure.blobname value:test_1" in less than 60 seconds
    And the Minifi logs contain the following message: "key:azure.blobname value:test_2" in less than 60 seconds
    And the Minifi logs do not contain the following message: "key:azure.blobname value:other_test" after 0 seconds
    And no errors were generated on the http-proxy regarding "http://azure-storage-server-${scenario_id}:10000/devstoreaccount1/test-container"
