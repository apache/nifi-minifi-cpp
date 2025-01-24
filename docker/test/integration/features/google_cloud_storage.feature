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

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance can upload data to Google Cloud storage
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "hello_gcs" is present in "/tmp/input"
    And a Google Cloud storage server is set up
    And a PutGCSObject processor
    And the PutGCSObject processor is set up with a GCPCredentialsControllerService to communicate with the Google Cloud storage server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutGCSObject
    And the "success" relationship of the PutGCSObject processor is connected to the PutFile
    And the "failure" relationship of the PutGCSObject processor is connected to the PutGCSObject

    When all instances start up

    Then a flowfile with the content "hello_gcs" is placed in the monitored directory in less than 45 seconds
    And an object with the content "hello_gcs" is present in the Google Cloud storage

  Scenario: A MiNiFi instance can fetch the listed objects from Google Cloud storage bucket
    Given a Google Cloud storage server is set up and a single object with contents "preloaded data" is present
    And a ListGCSBucket processor
    And a FetchGCSObject processor
    And the ListGCSBucket and the FetchGCSObject processors are set up with a GCPCredentialsControllerService to communicate with the Google Cloud storage server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the ListGCSBucket processor is connected to the FetchGCSObject
    And the "success" relationship of the FetchGCSObject processor is connected to the PutFile

    When all instances start up

    Then a flowfile with the content "preloaded data" is placed in the monitored directory in less than 10 seconds


  Scenario: A MiNiFi instance can delete the listed objects from Google Cloud storage bucket
    Given a Google Cloud storage server is set up with some test data
    And a ListGCSBucket processor
    And a DeleteGCSObject processor
    And the ListGCSBucket and the DeleteGCSObject processors are set up with a GCPCredentialsControllerService to communicate with the Google Cloud storage server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the ListGCSBucket processor is connected to the DeleteGCSObject
    And the "success" relationship of the DeleteGCSObject processor is connected to the PutFile

    When all instances start up

    Then the test bucket of Google Cloud Storage is empty
    And at least one empty flowfile is placed in the monitored directory in less than 10 seconds
