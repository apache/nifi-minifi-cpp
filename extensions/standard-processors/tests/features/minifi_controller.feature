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

@ENABLE_CONTROLLER
Feature: MiNiFi Controller functionalities
  Test MiNiFi Controller functionalities

  Scenario: Flow config can be updated through MiNiFi controller
    Given a GenerateFlowFile processor
    And a directory at "/tmp/input" has a file with the content "test"
    And controller socket properties are set up
    When all instances start up
    And MiNiFi config is updated through MiNiFi controller
    Then a single file with the content "test" is placed in the "/tmp/output" directory in less than 60 seconds
    And the updated config is persisted

  Scenario: A component can be stopped
    Given a GenerateFlowFile processor
    And a directory at "/tmp/input" has a file with the content "test"
    And controller socket properties are set up
    When all instances start up
    And the GenerateFlowFile component is stopped through MiNiFi controller
    Then the GenerateFlowFile component is not running
    And the FlowController component is running

  Scenario: If FlowController is stopped all other components are stopped
    Given a GenerateFlowFile processor
    And a directory at "/tmp/input" has a file with the content "test"
    And controller socket properties are set up
    When all instances start up
    And the FlowController component is stopped through MiNiFi controller
    Then the GenerateFlowFile component is not running
    And the FlowController component is not running

  Scenario: FlowController can be stopped and restarted
    Given a GenerateFlowFile processor
    And a directory at "/tmp/input" has a file with the content "test"
    And controller socket properties are set up
    When all instances start up
    And the FlowController component is stopped through MiNiFi controller
    And the FlowController component is started through MiNiFi controller
    Then the GenerateFlowFile component is running
    And the FlowController component is running

  Scenario: Queue state can be queried
    Given a GenerateFlowFile processor
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And the "success" relationship of the GenerateFlowFile processor is connected to the LogAttribute
    And controller socket properties are set up
    When all instances start up
    And MiNiFi config is updated through MiNiFi controller
    Then connection "GetFile/success/PutFile" can be seen through MiNiFi controller
    And 0 connections can be seen full through MiNiFi controller
    And connection "GetFile/success/PutFile" has 0 size and 2000 max size through MiNiFi controller

  Scenario: Manifest can be retrieved
    Given a GenerateFlowFile processor
    And a directory at "/tmp/input" has a file with the content "test"
    And controller socket properties are set up
    When all instances start up
    Then manifest can be retrieved through MiNiFi controller

  Scenario: Debug bundle can be retrieved
    Given a GenerateFlowFile processor
    And a directory at "/tmp/input" has a file with the content "test"
    And controller socket properties are set up
    When all instances start up
    Then debug bundle can be retrieved through MiNiFi controller
