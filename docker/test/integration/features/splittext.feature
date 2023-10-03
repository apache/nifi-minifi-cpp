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
Feature: Split input text line-by-line using SplitText
  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: Split textfile containing header lines specified by line count
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with filename "test_file.log" and content "[HEADER]header line 1\n[HEADER]header line 2\nDATA LINE 1\nDATA LINE 2\n\n" is present in "/tmp/input"
    And a SplitText processor with the "Line Split Count" property set to "1"
    And the "Header Line Count" property of the SplitText processor is set to "2"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the SplitText
    And the "splits" relationship of the SplitText processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then flowfiles with these contents are placed in the monitored directory in less than 15 seconds: "[HEADER]header line 1\n[HEADER]header line 2\nDATA LINE 1,[HEADER]header line 1\n[HEADER]header line 2\nDATA LINE 2,[HEADER]header line 1\n[HEADER]header line 2"

  Scenario: Split textfile containing header lines specified by header line marker characters
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with filename "test_file.log" and content "[HEADER]header line 1\n[HEADER]header line 2\nA BIT LONGER DATA LINE 1\nDATA 2\nDATA 3\n\n" is present in "/tmp/input"
    And a SplitText processor with the "Line Split Count" property set to "3"
    And the "Maximum Fragment Size" property of the SplitText processor is set to "60 B"
    And the "Header Line Marker Characters" property of the SplitText processor is set to "[HEADER]"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the SplitText
    And the "splits" relationship of the SplitText processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then flowfiles with these contents are placed in the monitored directory in less than 30 seconds: "[HEADER]header line 1\n[HEADER]header line 2\nA BIT LONGER DATA LINE 1,[HEADER]header line 1\n[HEADER]header line 2\nDATA 2\nDATA 3"
