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

Feature: Core flow functionalities
  Test core flow configuration functionalities

  Background:
    Given the content of "/tmp/output" is monitored

  @CORE
  Scenario: A funnel can merge multiple connections from different processors
    Given a GenerateFlowFile processor with the name "Generate1" and the "Custom Text" property set to "first_custom_text"
    And the "Data Format" property of the Generate1 processor is set to "Text"
    And the "Unique FlowFiles" property of the Generate1 processor is set to "false"
    And a GenerateFlowFile processor with the name "Generate2" and the "Custom Text" property set to "second_custom_text"
    And the "Data Format" property of the Generate2 processor is set to "Text"
    And the "Unique FlowFiles" property of the Generate2 processor is set to "false"
    And "Generate2" processor is a start node
    And a Funnel with the name "Funnel1" is set up
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the Generate1 processor is connected to the Funnel1
    And the "success" relationship of the Generate2 processor is connected to the Funnel1
    And the Funnel with the name "Funnel1" is connected to the PutFile

    When all instances start up

    Then at least one flowfile with the content "first_custom_text" is placed in the monitored directory in less than 20 seconds
    And at least one flowfile with the content "second_custom_text" is placed in the monitored directory in less than 20 seconds

  @CORE
  Scenario: The default configuration uses RocksDB for both the flow file and content repositories
    Given a GenerateFlowFile processor with the "Data Format" property set to "Text"
    When the MiNiFi instance starts up
    Then the Minifi logs contain the following message: "Using plaintext FlowFileRepository" in less than 5 seconds
    And the Minifi logs contain the following message: "Using plaintext DatabaseContentRepository" in less than 1 second

  @ENABLE_TEST_PROCESSORS
  Scenario: Processors are destructed when agent is stopped
    Given a transient MiNiFi flow with the name "transient-minifi" is set up
    And a LogOnDestructionProcessor processor with the name "logOnDestruction" in the "transient-minifi" flow
    When the MiNiFi instance starts up
    Then the Minifi logs contain the following message: "LogOnDestructionProcessor is being destructed" in less than 100 seconds

  @CORE
  Scenario: Agent does not crash when using provenance repositories
    Given a GenerateFlowFile processor with the name "generateFlowFile" in the "minifi-cpp-with-provenance-repo" flow
    And the provenance repository is enabled in MiNiFi
    When the MiNiFi instance starts up
    Then the "minifi-cpp-with-provenance-repo-${feature_id}" flow has a log line matching "MiNiFi started" in less than 30 seconds

  @SKIP_CI
  Scenario: Memory usage returns after peak usage
    Given a GenerateFlowFile processor with the "Batch Size" property set to "50000"
    And the "Data Format" property of the GenerateFlowFile processor is set to "Text"
    And the scheduling period of the GenerateFlowFile processor is set to "1 hours"
    And the "Unique FlowFiles" property of the GenerateFlowFile processor is set to "false"
    And the "Custom Text" property of the GenerateFlowFile processor is set to "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur tellus quam, sagittis quis ante ac, finibus ornare lectus. Morbi libero mauris, mollis sed mi at."
    When all instances start up
    Then the peak memory usage of the agent is more than 130 MB in less than 20 seconds
    And the memory usage of the agent decreases to 70% peak usage in less than 20 seconds

  Scenario: Metrics can be logged
    Given a GenerateFlowFile processor
    And log metrics publisher is enabled in MiNiFi
    When all instances start up
    Then the Minifi logs contain the following message: '[info] {' in less than 30 seconds
    And the Minifi logs contain the following message: '    "LogMetrics": {' in less than 2 seconds
    And the Minifi logs contain the following message: '        "RepositoryMetrics": {' in less than 2 seconds
    And the Minifi logs contain the following message: '            "flowfile": {' in less than 2 seconds
    And the Minifi logs contain the following message: '                "running": "true",' in less than 2 seconds
    And the Minifi logs contain the following message: '                "full": "false",' in less than 2 seconds
    And the Minifi logs contain the following message: '                "size": "0"' in less than 2 seconds
    And the Minifi logs contain the following message: '            },' in less than 2 seconds
    And the Minifi logs contain the following message: '            "provenance": {' in less than 2 seconds
