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

@ENABLE_ELASTICSEARCH
Feature: PostElasticsearch works on Opensearch (Opensearch doesnt support API Keys)

  Scenario Outline: MiNiFi instance creates a document on Opensearch using Basic Authentication
    Given an Opensearch server is set up and running
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at '/tmp/input' has a file with the content '{ "field1" : "value1" }'
    And a PostElasticsearch processor
    And PostElasticsearch is EVENT_DRIVEN
    And the "Hosts" property of the PostElasticsearch processor is set to "https://opensearch-${scenario_id}:9200"
    And the "Index" property of the PostElasticsearch processor is set to "my_index"
    And the "Identifier" property of the PostElasticsearch processor is set to "my_id"
    And the "Action" property of the PostElasticsearch processor is set to <action>
    And the "Elasticsearch Credentials Provider Service" property of the PostElasticsearch processor is set to "ElasticsearchCredentialsControllerService"
    And an ssl context service is set up for PostElasticsearch
    And an ElasticsearchCredentialsControllerService is set up with Basic Authentication
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the PostElasticsearch
    And the "success" relationship of the PostElasticsearch processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When the MiNiFi instance starts up
    Then a single file with the content "{ "field1" : "value1" }" is placed in the "/tmp/output" directory in less than 20 seconds
    And Opensearch has a document with "my_id" in "my_index" that has "value1" set in "field1"

    Examples:
      | action   |
      | "index"  |
      | "create" |

  Scenario: MiNiFi instance deletes a document from Opensearch using Basic Authentication
    Given an Opensearch server is set up and a single document is present with "preloaded_id" in "my_index"
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "hello world"
    And a PostElasticsearch processor
    And PostElasticsearch is EVENT_DRIVEN
    And the "Hosts" property of the PostElasticsearch processor is set to "https://opensearch-${scenario_id}:9200"
    And the "Index" property of the PostElasticsearch processor is set to "my_index"
    And the "Identifier" property of the PostElasticsearch processor is set to "preloaded_id"
    And the "Action" property of the PostElasticsearch processor is set to "delete"
    And the "Elasticsearch Credentials Provider Service" property of the PostElasticsearch processor is set to "ElasticsearchCredentialsControllerService"
    And an ssl context service is set up for PostElasticsearch
    And an ElasticsearchCredentialsControllerService is set up with Basic Authentication
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the PostElasticsearch
    And the "success" relationship of the PostElasticsearch processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When the MiNiFi instance starts up
    Then a single file with the content "hello world" is placed in the "/tmp/output" directory in less than 20 seconds
    And Opensearch is empty

  Scenario: MiNiFi instance partially updates a document in Opensearch using Basic Authentication
    Given an Opensearch server is set up and a single document is present with "preloaded_id" in "my_index" with "value1" in "field1"
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at '/tmp/input' has a file with the content '{ "field2" : "value2" }'
    And a PostElasticsearch processor
    And PostElasticsearch is EVENT_DRIVEN
    And the "Hosts" property of the PostElasticsearch processor is set to "https://opensearch-${scenario_id}:9200"
    And the "Index" property of the PostElasticsearch processor is set to "my_index"
    And the "Identifier" property of the PostElasticsearch processor is set to "preloaded_id"
    And the "Action" property of the PostElasticsearch processor is set to "update"
    And the "Elasticsearch Credentials Provider Service" property of the PostElasticsearch processor is set to "ElasticsearchCredentialsControllerService"
    And an ssl context service is set up for PostElasticsearch
    And an ElasticsearchCredentialsControllerService is set up with Basic Authentication
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the PostElasticsearch
    And the "success" relationship of the PostElasticsearch processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When the MiNiFi instance starts up
    Then a single file with the content "{ "field2" : "value2" }" is placed in the "/tmp/output" directory in less than 20 seconds
    And Opensearch has a document with "preloaded_id" in "my_index" that has "value1" set in "field1"
    And Opensearch has a document with "preloaded_id" in "my_index" that has "value2" set in "field2"
