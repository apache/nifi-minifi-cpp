Feature: MiNiFi can use python processors in its flows
  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance can update attributes through custom python processor
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a ExecutePythonProcessor processor with the "Script File" property set to "/tmp/resources/python/add_attribute_to_flowfile.py"
    And a LogAttribute processor
    And the "success" relationship of the GenerateFlowFile processor is connected to the ExecutePythonProcessor
    And the "success" relationship of the ExecutePythonProcessor processor is connected to the LogAttribute

    When all instances start up
    Then the Minifi logs contain the following message: "key:Python attribute value:attributevalue" in less than 60 seconds

  Scenario: A MiNiFi instance can update attributes through native python processor
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a AddPythonAttribute processor
    And a LogAttribute processor
    And the "success" relationship of the GenerateFlowFile processor is connected to the AddPythonAttribute
    And the "success" relationship of the AddPythonAttribute processor is connected to the LogAttribute

    When all instances start up
    Then the Minifi logs contain the following message: "key:Python attribute value:attributevalue" in less than 60 seconds
