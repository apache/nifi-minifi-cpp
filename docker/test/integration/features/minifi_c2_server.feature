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
Feature: MiNiFi can communicate with Apache NiFi MiNiFi C2 server

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: MiNiFi flow config is updated from MiNiFi C2 server
    Given a GetFile processor with the name "GetFile1" and the "Input Directory" property set to "/tmp/non-existent"
    And C2 is enabled in MiNiFi
    And a file with the content "test" is present in "/tmp/input"
    And a MiNiFi C2 server is set up
    When all instances start up
    Then the MiNiFi C2 server logs contain the following message: "acknowledged with a state of FULLY_APPLIED(DONE)" in less than 30 seconds
    And a flowfile with the content "test" is placed in the monitored directory in less than 10 seconds

  Scenario: MiNiFi flow config is updated from MiNiFi C2 server through SSL with SSL controller service
    Given a file with the content "test" is present in "/tmp/input"
    And a ssl context service is set up for MiNiFi C2 server
    And a MiNiFi C2 server is set up with SSL
    When all instances start up
    Then the MiNiFi C2 SSL server logs contain the following message: "acknowledged with a state of FULLY_APPLIED(DONE)" in less than 60 seconds
    And a flowfile with the content "test" is placed in the monitored directory in less than 10 seconds

  Scenario: MiNiFi can get flow config from C2 server through flow url when it is not available at start
    Given flow configuration path is set up in flow url property
    And C2 is enabled in MiNiFi
    And a file with the content "test" is present in "/tmp/input"
    And a MiNiFi C2 server is started
    When all instances start up
    Then the MiNiFi C2 server logs contain the following message: "acknowledged with a state of FULLY_APPLIED(DONE)" in less than 30 seconds
    And a flowfile with the content "test" is placed in the monitored directory in less than 10 seconds

  Scenario: MiNiFi flow config is updated from MiNiFi C2 server through SSL with SSL properties
    Given a file with the content "test" is present in "/tmp/input"
    And a GenerateFlowFile processor
    And a ssl properties are set up for MiNiFi C2 server
    And a MiNiFi C2 server is set up with SSL
    When all instances start up
    Then the MiNiFi C2 SSL server logs contain the following message: "acknowledged with a state of FULLY_APPLIED(DONE)" in less than 60 seconds
    And a flowfile with the content "test" is placed in the monitored directory in less than 10 seconds
