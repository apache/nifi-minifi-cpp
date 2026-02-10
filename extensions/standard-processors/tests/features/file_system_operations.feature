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

@CORE
Feature: File system operations are handled by the GetFile, PutFile, ListFile and FetchFile processors
  In order to store and access data on the local file system
  As a user of MiNiFi
  I need to have GetFile, PutFile, ListFile and FetchFile processors

  Scenario: Get and put operations run in a simple flow
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then a single file with the content "test" is placed in the "/tmp/output" directory in less than 10 seconds

  Scenario: PutFile does not overwrite a file that already exists
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a PutFile processor with the name "PutFile_1"
    And PutFile_1 is EVENT_DRIVEN
    And a PutFile processor with the name "PutFile_2"
    And PutFile_2 is EVENT_DRIVEN
    And a PutFile processor with the name "PutFile_3"
    And PutFile_3 is EVENT_DRIVEN

    And these processor properties are set
      | processor name | property name   | property value |
      | GetFile        | Input Directory | /tmp/input     |
      | PutFile_1      | Directory       | /tmp           |
      | PutFile_2      | Directory       | /tmp           |
      | PutFile_3      | Directory       | /tmp/output    |

    And the processors are connected up as described here
      | source name | relationship name | destination name |
      | GetFile     | success           | PutFile_1        |
      | PutFile_1   | success           | PutFile_2        |
      | PutFile_2   | failure           | PutFile_3        |

    And PutFile_1's failure relationship is auto-terminated
    And PutFile_2's success relationship is auto-terminated
    And PutFile_3's success relationship is auto-terminated
    And PutFile_3's failure relationship is auto-terminated

    And a directory at "/tmp/input" has a file with the content "test"
    When the MiNiFi instance starts up
    Then a single file with the content "test" is placed in the "/tmp/output" directory in less than 10 seconds

  Scenario: List and fetch files from a directory in a simple flow
    Given a file with filename "test_file.log" and content "Test message" is present in "/tmp/input"
    And a file with filename "test_file2.log" and content "Another test message" is present in "/tmp/input"
    And a ListFile processor with the "Input Directory" property set to "/tmp/input"
    And a FetchFile processor
    And FetchFile is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the ListFile processor is connected to the FetchFile
    And the "success" relationship of the FetchFile processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then files with contents "Test message" and "Another test message" are placed in the "/tmp/output" directory in less than 10 seconds
