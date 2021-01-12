Feature: File system operations

Scenario: Get and put operations run in a simple flow
  Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
  And a PutFile processor with the "Directory" property set to "/tmp/output"
  And the "success" relationship of the GetFile processor is connected to Putfile
  When the MiNiFi instance starts up
  Then flowfiles are produced in "/tmp/output" in less than 10 seconds
