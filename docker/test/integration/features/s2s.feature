Feature: Verify sending data from a MiNiFi-C++ to NiFi using S2S protocol.
  In order to transfer data inbetween NiFi and MiNiFi flows
  As a user of MiNiFi
  I need to have RemoteProcessGroup flow nodes

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance produces and transfers data to a NiFi instance
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a RemoteProcessGroup node opened on "http://nifi:8080/nifi"
    And the "success" relationship of the GetFile processor is connected to the input port on the RemoteProcessGroup

    And a NiFi flow "nifi" receiving data from a RemoteProcessGroup "from-minifi" on port 8080
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And the "success" relationship of the from-minifi is connected to the PutFile

    When both instances start up
    Then flowfiles are placed in the monitored directory in less than 60 seconds
