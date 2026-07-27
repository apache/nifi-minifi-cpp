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

@SUPPORTS_WINDOWS
Feature: Testing streaming reads and writes

  Scenario: Streaming Transforms work
    Given a GetFileRs processor with the "Input Directory" property set to "/tmp/input"
    And a AsciifyGerman processor
    And a PutFileRs processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFileRs processor is connected to the AsciifyGerman
    And the "success" relationship of the AsciifyGerman processor is connected to the PutFileRs
    And PutFileRs's success relationship is auto-terminated
    And PutFileRs's failure relationship is auto-terminated
    And a directory at "/tmp/input" has a file "german.txt" with the content "Üben von Xylophon und Querflöte ist ja zweckmäßig."

    When the MiNiFi instance starts up

    Then at least one file with the content "Ueben von Xylophon und Querfloete ist ja zweckmaessig." is placed in the "/tmp/output" directory in less than 10 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings

  Scenario: Streaming can be cancelled
    Given a GetFileRs processor with the "Input Directory" property set to "/tmp/input"
    And a AsciifyGerman processor
    And a PutFileRs processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFileRs processor is connected to the AsciifyGerman
    And the "failure" relationship of the AsciifyGerman processor is connected to the PutFileRs
    And PutFileRs's success relationship is auto-terminated
    And PutFileRs's failure relationship is auto-terminated
    And a directory at "/tmp/input" has a file "french.txt" with the content "Voix ambiguë d'un cœur qui, au zéphyr, préfère les jattes de kiwis."

    When the MiNiFi instance starts up

    Then at least one file with the content "Voix ambiguë d'un cœur qui, au zéphyr, préfère les jattes de kiwis." is placed in the "/tmp/output" directory in less than 10 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings
