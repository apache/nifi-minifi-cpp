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

@ENABLE_GCP
Feature: Sending data to Google Cloud Storage using PutGCSObject

  Scenario: A MiNiFi instance can upload data to Google Cloud storage
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "hello_gcs"
    And a Google Cloud storage server is set up
    And a PutGCSObject processor
    And PutGCSObject is EVENT_DRIVEN
    And a GCPCredentialsControllerService controller service is set up
    And the "Credentials Location" property of the GCPCredentialsControllerService controller service is set to "Use Anonymous credentials"
    And the "GCP Credentials Provider Service" property of the PutGCSObject processor is set to "GCPCredentialsControllerService"
    And the "Bucket" property of the PutGCSObject processor is set to "test-bucket"
    And the "Number of retries" property of the PutGCSObject processor is set to "2"
    And the "Endpoint Override URL" property of the PutGCSObject processor is set to "fake-gcs-server-${scenario_id}:4443"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the PutGCSObject
    And the "success" relationship of the PutGCSObject processor is connected to the PutFile
    And the "failure" relationship of the PutGCSObject processor is connected to the PutGCSObject
    And PutFile's success relationship is auto-terminated

    When all instances start up

    Then a single file with the content "hello_gcs" is placed in the "/tmp/output" directory in less than 45 seconds
    And an object with the content "hello_gcs" is present in the Google Cloud storage

  Scenario Outline: A MiNiFi instance can upload data to Google Cloud storage through a http proxy
    Given the http proxy server is set up
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And the scheduling period of the GetFile processor is set to "60 sec"
    And a file with the content "hello_gcs" is present in "/tmp/input"
    And a Google Cloud storage server is set up
    And a PutGCSObject processor
    And the "Proxy Configuration Service" property of the PutGCSObject processor is set to "ProxyConfigurationService"
    And PutGCSObject is EVENT_DRIVEN
    And a GCPCredentialsControllerService controller service is set up
    And the "Credentials Location" property of the GCPCredentialsControllerService controller service is set to "Use Anonymous credentials"
    And the "GCP Credentials Provider Service" property of the PutGCSObject processor is set to "GCPCredentialsControllerService"
    And the "Bucket" property of the PutGCSObject processor is set to "test-bucket"
    And the "Number of retries" property of the PutGCSObject processor is set to "2"
    And the "Endpoint Override URL" property of the PutGCSObject processor is set to "fake-gcs-server-${scenario_id}:4443"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And a ProxyConfigurationService controller service is set up with <proxy type> proxy configuration
    And the "success" relationship of the GetFile processor is connected to the PutGCSObject
    And the "success" relationship of the PutGCSObject processor is connected to the PutFile
    And the "failure" relationship of the PutGCSObject processor is connected to the PutGCSObject
    And PutFile's success relationship is auto-terminated
    And PutFile's failure relationship is auto-terminated

    When all instances start up

    Then a single file with the content "hello_gcs" is placed in the "/tmp/output" directory in less than 60 seconds
    And an object with the content "hello_gcs" is present in the Google Cloud storage
    And no errors were generated on the http-proxy regarding "http://fake-gcs-server-${scenario_id}:4443/"

    Examples: Proxy Type
    | proxy type |
    | HTTP       |
    | HTTPS      |

  Scenario: A MiNiFi instance can fetch the listed objects from Google Cloud storage bucket
    Given a Google Cloud storage server is set up and a single object with contents "preloaded data" is present
    And a GCPCredentialsControllerService controller service is set up
    And the "Credentials Location" property of the GCPCredentialsControllerService controller service is set to "Use Anonymous credentials"
    And a ListGCSBucket processor
    And the "Bucket" property of the ListGCSBucket processor is set to "test-bucket"
    And the "Number of retries" property of the ListGCSBucket processor is set to "2"
    And the "Endpoint Override URL" property of the ListGCSBucket processor is set to "fake-gcs-server-${scenario_id}:4443"
    And the "GCP Credentials Provider Service" property of the ListGCSBucket processor is set to "GCPCredentialsControllerService"
    And a FetchGCSObject processor
    And FetchGCSObject is EVENT_DRIVEN
    And the "Bucket" property of the FetchGCSObject processor is set to "test-bucket"
    And the "Number of retries" property of the FetchGCSObject processor is set to "2"
    And the "Endpoint Override URL" property of the FetchGCSObject processor is set to "fake-gcs-server-${scenario_id}:4443"
    And the "GCP Credentials Provider Service" property of the FetchGCSObject processor is set to "GCPCredentialsControllerService"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the ListGCSBucket processor is connected to the FetchGCSObject
    And the "success" relationship of the FetchGCSObject processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When the MiNiFi instance starts up

    Then a single file with the content "preloaded data" is placed in the "/tmp/output" directory in less than 10 seconds

  Scenario Outline: A MiNiFi instance can fetch the listed objects from Google Cloud storage bucket through a http proxy
    Given the http proxy server is set up
    And a Google Cloud storage server is set up and a single object with contents "preloaded data" is present
    And a GCPCredentialsControllerService controller service is set up
    And the "Credentials Location" property of the GCPCredentialsControllerService controller service is set to "Use Anonymous credentials"
    And a ListGCSBucket processor
    And the "Bucket" property of the ListGCSBucket processor is set to "test-bucket"
    And the "Number of retries" property of the ListGCSBucket processor is set to "2"
    And the "Endpoint Override URL" property of the ListGCSBucket processor is set to "fake-gcs-server-${scenario_id}:4443"
    And the "GCP Credentials Provider Service" property of the ListGCSBucket processor is set to "GCPCredentialsControllerService"
    And the "Proxy Configuration Service" property of the ListGCSBucket processor is set to "ProxyConfigurationService"
    And a FetchGCSObject processor
    And FetchGCSObject is EVENT_DRIVEN
    And the "Bucket" property of the FetchGCSObject processor is set to "test-bucket"
    And the "Number of retries" property of the FetchGCSObject processor is set to "2"
    And the "Endpoint Override URL" property of the FetchGCSObject processor is set to "fake-gcs-server-${scenario_id}:4443"
    And the "GCP Credentials Provider Service" property of the FetchGCSObject processor is set to "GCPCredentialsControllerService"
    And the "Proxy Configuration Service" property of the FetchGCSObject processor is set to "ProxyConfigurationService"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And a ProxyConfigurationService controller service is set up with <proxy type> proxy configuration
    And the "success" relationship of the ListGCSBucket processor is connected to the FetchGCSObject
    And the "success" relationship of the FetchGCSObject processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    And PutFile's failure relationship is auto-terminated

    When all instances start up

    Then a single file with the content "preloaded data" is placed in the "/tmp/output" directory in less than 20 seconds
    And no errors were generated on the http-proxy regarding "http://fake-gcs-server-${scenario_id}:4443/"

    Examples: Proxy Type
    | proxy type |
    | HTTP       |
    | HTTPS      |

  Scenario: A MiNiFi instance can delete the listed objects from Google Cloud storage bucket
    Given a Google Cloud storage server is set up with some test data
    And a GCPCredentialsControllerService controller service is set up
    And the "Credentials Location" property of the GCPCredentialsControllerService controller service is set to "Use Anonymous credentials"
    And a ListGCSBucket processor
    And the "Bucket" property of the ListGCSBucket processor is set to "test-bucket"
    And the "Number of retries" property of the ListGCSBucket processor is set to "2"
    And the "Endpoint Override URL" property of the ListGCSBucket processor is set to "fake-gcs-server-${scenario_id}:4443"
    And the "GCP Credentials Provider Service" property of the ListGCSBucket processor is set to "GCPCredentialsControllerService"
    And a DeleteGCSObject processor
    And DeleteGCSObject is EVENT_DRIVEN
    And the "Bucket" property of the DeleteGCSObject processor is set to "test-bucket"
    And the "Number of retries" property of the DeleteGCSObject processor is set to "2"
    And the "Endpoint Override URL" property of the DeleteGCSObject processor is set to "fake-gcs-server-${scenario_id}:4443"
    And the "GCP Credentials Provider Service" property of the DeleteGCSObject processor is set to "GCPCredentialsControllerService"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the ListGCSBucket processor is connected to the DeleteGCSObject
    And the "success" relationship of the DeleteGCSObject processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When the MiNiFi instance starts up

    Then the test bucket of Google Cloud Storage is empty
    And at least one empty file is placed in the "/tmp/output" directory in less than 10 seconds

  Scenario Outline: A MiNiFi instance can delete the listed objects from Google Cloud storage bucket through a http proxy
    Given the http proxy server is set up
    Given a Google Cloud storage server is set up with some test data
    And a GCPCredentialsControllerService controller service is set up
    And the "Credentials Location" property of the GCPCredentialsControllerService controller service is set to "Use Anonymous credentials"
    And a ListGCSBucket processor
    And the "Bucket" property of the ListGCSBucket processor is set to "test-bucket"
    And the "Number of retries" property of the ListGCSBucket processor is set to "2"
    And the "Endpoint Override URL" property of the ListGCSBucket processor is set to "fake-gcs-server-${scenario_id}:4443"
    And the "GCP Credentials Provider Service" property of the ListGCSBucket processor is set to "GCPCredentialsControllerService"
    And the "Proxy Configuration Service" property of the ListGCSBucket processor is set to "ProxyConfigurationService"
    And a DeleteGCSObject processor
    And DeleteGCSObject is EVENT_DRIVEN
    And the "Bucket" property of the DeleteGCSObject processor is set to "test-bucket"
    And the "Number of retries" property of the DeleteGCSObject processor is set to "2"
    And the "Endpoint Override URL" property of the DeleteGCSObject processor is set to "fake-gcs-server-${scenario_id}:4443"
    And the "GCP Credentials Provider Service" property of the DeleteGCSObject processor is set to "GCPCredentialsControllerService"
    And the "Proxy Configuration Service" property of the DeleteGCSObject processor is set to "ProxyConfigurationService"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And a ProxyConfigurationService controller service is set up with <proxy type> proxy configuration
    And the "success" relationship of the ListGCSBucket processor is connected to the DeleteGCSObject
    And the "success" relationship of the DeleteGCSObject processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    And PutFile's failure relationship is auto-terminated

    When all instances start up

    Then the test bucket of Google Cloud Storage is empty
    And at least one empty file is placed in the "/tmp/output" directory in less than 10 seconds
    And no errors were generated on the http-proxy regarding "http://fake-gcs-server-${scenario_id}:4443/"

    Examples: Proxy Type
    | proxy type |
    | HTTP       |
    | HTTPS      |

  Scenario: A MiNiFi instance can upload data to Google Cloud storage when http proxy is configured but the proxy configuration service indicates DIRECT connection
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And the scheduling period of the GetFile processor is set to "60 sec"
    And a file with the content "hello_gcs" is present in "/tmp/input"
    And a Google Cloud storage server is set up
    And a PutGCSObject processor
    And the "Proxy Configuration Service" property of the PutGCSObject processor is set to "ProxyConfigurationService"
    And PutGCSObject is EVENT_DRIVEN
    And a GCPCredentialsControllerService controller service is set up
    And the "Credentials Location" property of the GCPCredentialsControllerService controller service is set to "Use Anonymous credentials"
    And the "GCP Credentials Provider Service" property of the PutGCSObject processor is set to "GCPCredentialsControllerService"
    And the "Bucket" property of the PutGCSObject processor is set to "test-bucket"
    And the "Number of retries" property of the PutGCSObject processor is set to "2"
    And the "Endpoint Override URL" property of the PutGCSObject processor is set to "fake-gcs-server-${scenario_id}:4443"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And a ProxyConfigurationService controller service is set up with DIRECT proxy configuration
    And the "success" relationship of the GetFile processor is connected to the PutGCSObject
    And the "success" relationship of the PutGCSObject processor is connected to the PutFile
    And the "failure" relationship of the PutGCSObject processor is connected to the PutGCSObject
    And PutFile's success relationship is auto-terminated
    And PutFile's failure relationship is auto-terminated

    When all instances start up

    Then a single file with the content "hello_gcs" is placed in the "/tmp/output" directory in less than 60 seconds
    And an object with the content "hello_gcs" is present in the Google Cloud storage
