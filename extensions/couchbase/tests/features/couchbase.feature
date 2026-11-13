# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

@ENABLE_COUCHBASE
Feature: Executing Couchbase operations from MiNiFi-C++

  Scenario: A MiNiFi instance can insert json data to test bucket with PutCouchbaseKey processor
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at '/tmp/input' has a file with the content '{"field1": "value1", "field2": "value2"}'
    And a PutCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And PutCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the PutCouchbaseKey processor is set to "test_doc_id"
    And the "Document Type" property of the PutCouchbaseKey processor is set to "Json"
    And the "Couchbase Cluster Controller Service" property of the PutCouchbaseKey processor is set to "CouchbaseClusterService"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And a CouchbaseClusterService controller service is set up to communicate with the Couchbase server

    And the "success" relationship of the GetFile processor is connected to the PutCouchbaseKey
    And the "failure" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "retry" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "success" relationship of the PutCouchbaseKey processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated

    When a Couchbase server is started
    And the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "key:couchbase.bucket value:test_bucket" in less than 100 seconds
    And the Minifi logs contain the following message: "key:couchbase.doc.id value:test_doc_id" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.cas value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.sequence.number value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.partition.uuid value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.partition.id value:[1-9][0-9]*" in less than 1 seconds
    And a document with id "test_doc_id" in bucket "test_bucket" is present with data '{"field1": "value1", "field2": "value2"}' of type "Json" in Couchbase

  Scenario: A MiNiFi instance can insert binary data to test bucket with PutCouchbaseKey processor
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at '/tmp/input' has a file with the content '{"field1": "value1"}'
    And a PutCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And PutCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the PutCouchbaseKey processor is set to "test_doc_id"
    And the "Document Type" property of the PutCouchbaseKey processor is set to "Binary"
    And the "Couchbase Cluster Controller Service" property of the PutCouchbaseKey processor is set to "CouchbaseClusterService"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And a CouchbaseClusterService controller service is set up to communicate with the Couchbase server

    And the "success" relationship of the GetFile processor is connected to the PutCouchbaseKey
    And the "failure" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "retry" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "success" relationship of the PutCouchbaseKey processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated

    When a Couchbase server is started
    And the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "key:couchbase.bucket value:test_bucket" in less than 100 seconds
    And the Minifi logs contain the following message: "key:couchbase.doc.id value:test_doc_id" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.cas value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.sequence.number value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.partition.uuid value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.partition.id value:[1-9][0-9]*" in less than 1 seconds
    And a document with id "test_doc_id" in bucket "test_bucket" is present with data '{"field1": "value1"}' of type "Binary" in Couchbase

  Scenario: A MiNiFi instance can get data from test bucket with GetCouchbaseKey processor
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at '/tmp/input' has a file with the content '{"field1": "value1", "field2": "value2"}'
    And a PutCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And PutCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the PutCouchbaseKey processor is set to "test_doc_id"
    And the "Couchbase Cluster Controller Service" property of the PutCouchbaseKey processor is set to "CouchbaseClusterService"
    And a GetCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And GetCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the GetCouchbaseKey processor is set to "test_doc_id"
    And the "Couchbase Cluster Controller Service" property of the GetCouchbaseKey processor is set to "CouchbaseClusterService"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And a CouchbaseClusterService controller service is set up to communicate with the Couchbase server

    And the "success" relationship of the GetFile processor is connected to the PutCouchbaseKey
    And the "failure" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "retry" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "success" relationship of the PutCouchbaseKey processor is connected to the GetCouchbaseKey
    And the "success" relationship of the GetCouchbaseKey processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated

    When a Couchbase server is started
    And the MiNiFi instance starts up

    Then a file with the JSON content '{"field1": "value1", "field2": "value2"}' is placed in the '/tmp/output' directory in less than 100 seconds
    And the Minifi logs contain the following message: "key:couchbase.bucket value:test_bucket" in less than 10 seconds
    And the Minifi logs contain the following message: "key:couchbase.doc.id value:test_doc_id" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.cas value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.expiry value:\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}" in less than 1 seconds

  Scenario: A MiNiFi instance can get data from test bucket with GetCouchbaseKey processor using binary storage
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at '/tmp/input' has a file with the content '{"field1": "value1", "field2": "value2"}'
    And a PutCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And PutCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the PutCouchbaseKey processor is set to "test_doc_id"
    And the "Document Type" property of the PutCouchbaseKey processor is set to "Binary"
    And the "Couchbase Cluster Controller Service" property of the PutCouchbaseKey processor is set to "CouchbaseClusterService"
    And a GetCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And GetCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the GetCouchbaseKey processor is set to "test_doc_id"
    And the "Document Type" property of the GetCouchbaseKey processor is set to "Binary"
    And the "Couchbase Cluster Controller Service" property of the GetCouchbaseKey processor is set to "CouchbaseClusterService"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And a CouchbaseClusterService controller service is set up to communicate with the Couchbase server

    And the "success" relationship of the GetFile processor is connected to the PutCouchbaseKey
    And the "failure" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "retry" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "success" relationship of the PutCouchbaseKey processor is connected to the GetCouchbaseKey
    And the "success" relationship of the GetCouchbaseKey processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated

    When a Couchbase server is started
    And the MiNiFi instance starts up

    Then a file with the JSON content '{"field1": "value1", "field2": "value2"}' is placed in the '/tmp/output' directory in less than 100 seconds
    And the Minifi logs contain the following message: "key:couchbase.bucket value:test_bucket" in less than 10 seconds
    And the Minifi logs contain the following message: "key:couchbase.doc.id value:test_doc_id" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.cas value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.expiry value:\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}" in less than 1 seconds

  Scenario: A MiNiFi instance can get data from test bucket with GetCouchbaseKey processor and put the result in an attribute
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at '/tmp/input' has a file with the content '{"field1": "value1", "field2": "value2"}'
    And a PutCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And PutCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the PutCouchbaseKey processor is set to "test_doc_id"
    And the "Document Type" property of the PutCouchbaseKey processor is set to "String"
    And the "Couchbase Cluster Controller Service" property of the PutCouchbaseKey processor is set to "CouchbaseClusterService"
    And a GetCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And GetCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the GetCouchbaseKey processor is set to "test_doc_id"
    And the "Document Type" property of the GetCouchbaseKey processor is set to "String"
    And the "Couchbase Cluster Controller Service" property of the GetCouchbaseKey processor is set to "CouchbaseClusterService"
    And the "Put Value to Attribute" property of the GetCouchbaseKey processor is set to "get_couchbase_result"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And a CouchbaseClusterService controller service is set up to communicate with the Couchbase server

    And the "success" relationship of the GetFile processor is connected to the PutCouchbaseKey
    And the "failure" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "retry" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "success" relationship of the PutCouchbaseKey processor is connected to the GetCouchbaseKey
    And the "success" relationship of the GetCouchbaseKey processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated

    When a Couchbase server is started
    And the MiNiFi instance starts up

    Then a file with the JSON content '{"field1": "value1", "field2": "value2"}' is placed in the '/tmp/output' directory in less than 100 seconds
    And the Minifi logs contain the following message: "key:couchbase.bucket value:test_bucket" in less than 10 seconds
    And the Minifi logs contain the following message: "key:couchbase.doc.id value:test_doc_id" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.cas value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.expiry value:\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}" in less than 1 seconds
    And the Minifi logs contain the following message: 'key:get_couchbase_result value:{"field1": "value1", "field2": "value2"}' in less than 1 seconds

  Scenario: GetCouchbaseKey transfers FlowFile to failure relationship on Couchbase value type mismatch
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at '/tmp/input' has a file with the content '{"field1": "value1", "field2": "value2"}'
    And a PutCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And PutCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the PutCouchbaseKey processor is set to "test_doc_id"
    And the "Document Type" property of the PutCouchbaseKey processor is set to "String"
    And the "Couchbase Cluster Controller Service" property of the PutCouchbaseKey processor is set to "CouchbaseClusterService"
    And a GetCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And GetCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the GetCouchbaseKey processor is set to "test_doc_id"
    And the "Document Type" property of the GetCouchbaseKey processor is set to "Binary"
    And the "Couchbase Cluster Controller Service" property of the GetCouchbaseKey processor is set to "CouchbaseClusterService"
    And a CouchbaseClusterService controller service is set up to communicate with the Couchbase server

    And the "success" relationship of the GetFile processor is connected to the PutCouchbaseKey
    And the "failure" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "retry" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "success" relationship of the PutCouchbaseKey processor is connected to the GetCouchbaseKey
    And GetCouchbaseKey's failure relationship is auto-terminated

    When a Couchbase server is started
    And the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "Failed to get content for document 'test_doc_id' from collection 'test_bucket._default._default' with the following exception: 'raw_binary_transcoder expects document to have BINARY common flags" in less than 100 seconds

  Scenario: A MiNiFi instance can get data from test bucket with GetCouchbaseKey processor using SSL connection
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And the scheduling period of the GetFile processor is set to "20 seconds"
    And a directory at '/tmp/input' has a file with the content '{"field1": "value1", "field2": "value2"}'
    And a PutCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And PutCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the PutCouchbaseKey processor is set to "test_doc_id"
    And the "Couchbase Cluster Controller Service" property of the PutCouchbaseKey processor is set to "CouchbaseClusterService"
    And a GetCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And GetCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the GetCouchbaseKey processor is set to "test_doc_id"
    And the "Couchbase Cluster Controller Service" property of the GetCouchbaseKey processor is set to "CouchbaseClusterService"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN

    And a CouchbaseClusterService is set up using SSL connection

    And the "success" relationship of the GetFile processor is connected to the PutCouchbaseKey
    And the "failure" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "retry" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "success" relationship of the PutCouchbaseKey processor is connected to the GetCouchbaseKey
    And the "success" relationship of the GetCouchbaseKey processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated

    When a Couchbase server is started
    And the MiNiFi instance starts up

    Then a file with the JSON content '{"field1": "value1", "field2": "value2"}' is placed in the '/tmp/output' directory in less than 100 seconds
    And the Minifi logs contain the following message: "key:couchbase.bucket value:test_bucket" in less than 10 seconds
    And the Minifi logs contain the following message: "key:couchbase.doc.id value:test_doc_id" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.cas value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.expiry value:\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}" in less than 1 seconds

  Scenario: A MiNiFi instance can get data from test bucket with GetCouchbaseKey processor using mTLS authentication
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at '/tmp/input' has a file with the content '{"field1": "value1", "field2": "value2"}'
    And a PutCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And PutCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the PutCouchbaseKey processor is set to "test_doc_id"
    And the "Couchbase Cluster Controller Service" property of the PutCouchbaseKey processor is set to "CouchbaseClusterService"
    And a GetCouchbaseKey processor with the "Bucket Name" property set to "test_bucket"
    And GetCouchbaseKey is EVENT_DRIVEN
    And the "Document Id" property of the GetCouchbaseKey processor is set to "test_doc_id"
    And the "Couchbase Cluster Controller Service" property of the GetCouchbaseKey processor is set to "CouchbaseClusterService"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And a CouchbaseClusterService is setup up using mTLS authentication

    And the "success" relationship of the GetFile processor is connected to the PutCouchbaseKey
    And the "failure" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "retry" relationship of the PutCouchbaseKey processor is connected to the PutCouchbaseKey
    And the "success" relationship of the PutCouchbaseKey processor is connected to the GetCouchbaseKey
    And the "success" relationship of the GetCouchbaseKey processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated

    When a Couchbase server is started
    And the MiNiFi instance starts up

    Then a file with the JSON content '{"field1": "value1", "field2": "value2"}' is placed in the '/tmp/output' directory in less than 100 seconds
    And the Minifi logs contain the following message: "key:couchbase.bucket value:test_bucket" in less than 10 seconds
    And the Minifi logs contain the following message: "key:couchbase.doc.id value:test_doc_id" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.cas value:[1-9][0-9]*" in less than 1 seconds
    And the Minifi logs match the following regex: "key:couchbase.doc.expiry value:\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}" in less than 1 seconds
