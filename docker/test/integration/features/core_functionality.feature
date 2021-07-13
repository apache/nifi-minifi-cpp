Feature: Core flow functionalities
  Test core flow configurations functionalities

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
    And a Funnel with the name "Funnel1" is set up in the flow
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the Generate1 processor is connected to the Funnel1
    And the "success" relationship of the Generate2 processor is connected to the Funnel1
    And in the flow the Funnel with the name "Funnel1" is connected to the PutFile

    When all instances start up

    Then at least one flowfile with the content "first_custom_text" is placed in the monitored directory in less than 20 seconds
    And at least one flowfile with the content "second_custom_text" is placed in the monitored directory in less than 20 seconds
