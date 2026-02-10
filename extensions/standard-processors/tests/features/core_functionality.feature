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
Feature: Core flow functionalities
  Test core flow configuration functionalities

  Scenario: A funnel can merge multiple connections from different processors
    Given a GenerateFlowFile processor with the name "Generate1" and the "Custom Text" property set to "first_custom_text"
    And the "Data Format" property of the Generate1 processor is set to "Text"
    And the "Unique FlowFiles" property of the Generate1 processor is set to "false"
    And a GenerateFlowFile processor with the name "Generate2" and the "Custom Text" property set to "second_custom_text"
    And the "Data Format" property of the Generate2 processor is set to "Text"
    And the "Unique FlowFiles" property of the Generate2 processor is set to "false"
    And a Funnel with the name "Funnel1" is set up
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the Generate1 processor is connected to the Funnel1
    And the "success" relationship of the Generate2 processor is connected to the Funnel1
    And the Funnel with the name "Funnel1" is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up

    Then at least one file with the content "first_custom_text" is placed in the "/tmp/output" directory in less than 20 seconds
    And at least one file with the content "second_custom_text" is placed in the "/tmp/output" directory in less than 20 seconds

  Scenario: A funnel can be used as a terminator
    Given a GenerateFlowFile processor with the "Data Format" property set to "Text"
    And a Funnel with the name "TerminalFunnel" is set up
    And the "success" relationship of the GenerateFlowFile processor is connected to the TerminalFunnel
    When the MiNiFi instance starts up
    Then the Minifi logs do not contain the following message: "Connect empty for non auto terminated relationship" after 3 seconds

  Scenario: The default configuration uses RocksDB for both the flow file and content repositories
    Given a GenerateFlowFile processor with the "Data Format" property set to "Text"
    When the MiNiFi instance starts up
    Then the Minifi logs contain the following message: "Using plaintext FlowFileRepository" in less than 5 seconds
    And the Minifi logs contain the following message: "Using plaintext DatabaseContentRepository" in less than 1 second

  Scenario: Agent does not crash when using provenance repositories
    Given a GenerateFlowFile processor with the name "generateFlowFile"
    And MiNiFi configuration "nifi.provenance.repository.class.name" is set to "VolatileProvenanceRepository"
    When the MiNiFi instance starts up
    Then the Minifi logs contain the following message: "MiNiFi started" in less than 40 seconds

  Scenario: Metrics can be logged
    Given a GenerateFlowFile processor
    And log metrics publisher is enabled in MiNiFi
    And GenerateFlowFile's success relationship is auto-terminated
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

  Scenario: MiNiFi uses parameter contexts correctly
    Given parameter context name is set to 'my-context'
    And a non-sensitive parameter in the flow config called 'FILENAME' with the value '${filename}' in the parameter context 'my-context'
    And a non-sensitive parameter in the flow config called 'FILENAME_IN_EXPRESSION' with the value 'filename' in the parameter context 'my-context'
    And a non-sensitive parameter in the flow config called 'FILE_INPUT_PATH' with the value '/tmp/input' in the parameter context 'my-context'
    And a non-sensitive parameter in the flow config called 'FILE_OUTPUT_UPPER_PATH_ATTR' with the value 'upper_out_path_attr' in the parameter context 'my-context'
    And a GetFile processor with the "Input Directory" property set to "#{FILE_INPUT_PATH}"
    And a directory at "/tmp/input" has a file ("test_file_name") with the content "test content"
    And a UpdateAttribute processor with the "expr-lang-filename" property set to "#{FILENAME}"
    And the "is-upper-correct" property of the UpdateAttribute processor is set to "${#{FILENAME_IN_EXPRESSION}:toUpper():equals('TEST_FILE_NAME')}"
    And the "upper_out_path_attr" property of the UpdateAttribute processor is set to "/TMP/OUTPUT"
    And a PutFile processor with the "Directory" property set to "${#{FILE_OUTPUT_UPPER_PATH_ATTR}:toLower()}"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"

    And the "success" relationship of the GetFile processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated

    When all instances start up

    Then the Minifi logs contain the following message: "key:expr-lang-filename value:test_file_name" in less than 60 seconds
    And the Minifi logs contain the following message: "key:is-upper-correct value:true" in less than 60 seconds


  @CORE
  Scenario: ConvertRecord processor can convert records from one format to another
    Given a XMLReader controller service is set up
    And a JsonRecordSetWriter controller service is set up and the "Output Grouping" property set to "Array"
    And a GenerateFlowFile processor with the "Data Format" property set to "Text"
    And the "Custom Text" property of the GenerateFlowFile processor is set to "<record><numbers>1</numbers><numbers>2</numbers></record>"
    And the "Unique FlowFiles" property of the GenerateFlowFile processor is set to "false"
    And a ConvertRecord processor with the "Record Reader" property set to "XMLReader"
    And the "Record Writer" property of the ConvertRecord processor is set to "JsonRecordSetWriter"
    And the "success" relationship of the GenerateFlowFile processor is connected to the ConvertRecord
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the ConvertRecord processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then at least one file with the content "[{"numbers":[1,2]}]" is placed in the "/tmp/output" directory in less than 10 seconds

