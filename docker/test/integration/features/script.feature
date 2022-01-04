Feature: MiNiFi can execute Lua and Python scripts
  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: ExecuteScript should only allow the number of parallel tasks defined by the max concurrent tasks attribute for Lua scripts
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And the scheduling period of the GenerateFlowFile processor is set to "500 ms"
    And a ExecuteScript processor with the "Script File" property set to "/tmp/resources/lua/sleep_forever.lua"
    And the "Script Engine" property of the ExecuteScript processor is set to "lua"
    And the max concurrent tasks attribute of the ExecuteScript processor is set to 3
    And the "success" relationship of the GenerateFlowFile processor is connected to the ExecuteScript

    When all instances start up
    Then the Minifi logs contain the following message: "Sleeping forever" 3 times after 5 seconds

  Scenario: ExecuteScript should only allow one Python script running at a time
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And the scheduling period of the GenerateFlowFile processor is set to "500 ms"
    And a ExecuteScript processor with the "Script File" property set to "/tmp/resources/python/sleep_forever.py"
    And the "Script Engine" property of the ExecuteScript processor is set to "python"
    And the max concurrent tasks attribute of the ExecuteScript processor is set to 3
    And the "success" relationship of the GenerateFlowFile processor is connected to the ExecuteScript

    When all instances start up
    Then the Minifi logs contain the following message: "Sleeping forever" 1 times after 5 seconds
