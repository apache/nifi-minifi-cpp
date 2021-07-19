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
