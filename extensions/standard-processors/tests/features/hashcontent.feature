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
Feature: Hash value is added to Flowfiles by HashContent processor
  In order to avoid duplication of content of Flowfiles
  As a user of MiNiFi
  I need to have HashContent processor to calculate and add hash value

  Scenario Outline: HashContent adds hash attribute to flowfiles
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "<content>"
    And a HashContent processor with the "Hash Attribute" property set to "hash"
    And the "Hash Algorithm" property of the HashContent processor is set to "<hash_algorithm>"
    And HashContent is EVENT_DRIVEN
    And a LogAttribute processor
    And LogAttribute is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the HashContent
    And the "success" relationship of the HashContent processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then the Minifi logs contain the following message: "key:hash value:<hash_value>" in less than 10 seconds

    Examples:
      | content | hash_algorithm | hash_value                                                       |
      | apple   | MD5            | 1F3870BE274F6C49B3E31A0C6728957F                                 |
      | test    | SHA1           | A94A8FE5CCB19BA61C4C0873D391E987982FBBD3                         |
      | coffee  | SHA256         | 37290D74AC4D186E3A8E5785D259D2EC04FAC91AE28092E7620EC8BC99E830AA |

  Scenario: HashContent fails for an empty file if 'fail on empty' property is set to true
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And an empty file is present in "/tmp/input"
    And a HashContent processor with the "Hash Attribute" property set to "hash"
    And the "Hash Algorithm" property of the HashContent processor is set to "MD5"
    And the "Fail on empty" property of the HashContent processor is set to "true"
    And HashContent is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the HashContent
    And the "failure" relationship of the HashContent processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then at least one empty file is placed in the "/tmp/output" directory in less than 10 seconds

  Scenario: HashContent can use MD5 in FIPS mode
    Given OpenSSL FIPS mode is enabled in MiNiFi
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "apple"
    And a HashContent processor with the "Hash Attribute" property set to "hash"
    And the "Hash Algorithm" property of the HashContent processor is set to "MD5"
    And HashContent is EVENT_DRIVEN
    And a LogAttribute processor
    And LogAttribute is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the HashContent
    And the "success" relationship of the HashContent processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then the Minifi logs contain the following message: "key:hash value:1F3870BE274F6C49B3E31A0C6728957F" in less than 10 seconds
