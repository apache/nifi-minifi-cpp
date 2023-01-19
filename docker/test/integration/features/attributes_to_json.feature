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

Feature: Writing attribute data using AttributesToJSON processor
  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: Write selected attribute data to file
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with filename "test_file.log" and content "test_data" is present in "/tmp/input"
    And a AttributesToJSON processor with the "Attributes List" property set to "filename,invalid"
    And the "Destination" property of the AttributesToJSON processor is set to "flowfile-content"
    And the "Null Value" property of the AttributesToJSON processor is set to "true"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the AttributesToJSON
    And the "success" relationship of the AttributesToJSON processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then a flowfile with the JSON content "{"filename":"test_file.log","invalid":null}" is placed in the monitored directory in less than 10 seconds
