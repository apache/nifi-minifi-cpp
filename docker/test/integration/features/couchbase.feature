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

@ENABLE_COUCHBASE
Feature: Executing Couchbase operations from MiNiFi-C++
  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance can insert json data to test bucket with PutCouchbaseKey processor
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content '{"field1": "value1", "field2": "value2"}' is present in '/tmp/input'
    And a PutCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And the "Document Id" property of the PutCouchbaseKey processor is set to "test_doc_id"
    And the "Document Type" property of the PutCouchbaseKey processor is set to "Json"
    And the "Couchbase Cluster Controller Service" property of the PutCouchbaseKey processor is set to "CouchbaseClusterService"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And a CouchbaseClusterService is setup up with the name "CouchbaseClusterService"

    And the "success" relationship of the GetFile processor is connected to the PutCouchbaseKey
    And the "success" relationship of the PutCouchbaseKey processor is connected to the LogAttribute

    When a Couchbase server is started
    And all instances start up

    Then the Minifi logs contain the following message: "key:couchbase.bucket value:test_bucket" in less than 60 seconds
    And the Minifi logs contain the following message: "key:couchbase.doc.id value:test_doc_id" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.cas value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.sequence.number value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.partition.uuid value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.partition.id value:[1-9][0-9]*" in less than 1 seconds
    And a document with id "test_doc_id" in bucket "test_bucket" is present with data '{"field1": "value1", "field2": "value2"}' of type "Json" in Couchbase

  Scenario: A MiNiFi instance can insert binary data to test bucket with PutCouchbaseKey processor
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content '{"field1": "value1"}' is present in '/tmp/input'
    And a PutCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And the "Document Id" property of the PutCouchbaseKey processor is set to "test_doc_id"
    And the "Document Type" property of the PutCouchbaseKey processor is set to "Binary"
    And the "Couchbase Cluster Controller Service" property of the PutCouchbaseKey processor is set to "CouchbaseClusterService"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And a CouchbaseClusterService is setup up with the name "CouchbaseClusterService"

    And the "success" relationship of the GetFile processor is connected to the PutCouchbaseKey
    And the "success" relationship of the PutCouchbaseKey processor is connected to the LogAttribute

    When a Couchbase server is started
    And all instances start up

    Then the Minifi logs contain the following message: "key:couchbase.bucket value:test_bucket" in less than 60 seconds
    And the Minifi logs contain the following message: "key:couchbase.doc.id value:test_doc_id" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.cas value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.sequence.number value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.partition.uuid value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.partition.id value:[1-9][0-9]*" in less than 1 seconds
    And a document with id "test_doc_id" in bucket "test_bucket" is present with data '{"field1": "value1"}' of type "Binary" in Couchbase
