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
Feature: Writing attribute data using AttributesToJSON processor

  Scenario: Write selected attribute data to file
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file "test_file.log" with the content "test_data"
    And a AttributesToJSON processor with the "Attributes List" property set to "filename,invalid"
    And the "Destination" property of the AttributesToJSON processor is set to "flowfile-content"
    And the "Null Value" property of the AttributesToJSON processor is set to "true"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the AttributesToJSON
    And the "success" relationship of the AttributesToJSON processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then a file with the JSON content "{"invalid":null,"filename":"test_file.log"}" is placed in the "/tmp/output" directory in less than 20 seconds
