Feature: Core flow functionalities
  Test core flow configuration functionalities

  Background:
    Given the content of "/tmp/output" is monitored

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


  Scenario: The default configuration uses RocksDB for both the flow file and content repositories
    Given a GenerateFlowFile processor with the "Data Format" property set to "Text"
    When the MiNiFi instance starts up
    Then the Minifi logs contain the following message: "Using plaintext FlowFileRepository" in less than 5 seconds
    And the Minifi logs contain the following message: "Using plaintext DatabaseContentRepository" in less than 1 second


  Scenario: Processors are destructed when agent is stopped
    Given a LogOnDestructionProcessor processor with the name "logOnDestruction" in the "transient-minifi" flow with engine "transient-minifi"
    When the MiNiFi instance starts up
    Then the Minifi logs contain the following message: "LogOnDestructionProcessor is being destructed" in less than 100 seconds

  Scenario: Agent does not crash when using provenance repositories
    Given a GenerateFlowFile processor with the name "generateFlowFile" in the "minifi-cpp-with-provenance-repo" flow with engine "minifi-cpp-with-provenance-repo"
    When the MiNiFi instance starts up
    Then the "minifi-cpp-with-provenance-repo" flow has a log line matching "MiNiFi started" in less than 30 seconds
