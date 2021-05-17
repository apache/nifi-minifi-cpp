Feature: Sending data from MiNiFi-C++ to NiFi using S2S protocol
  In order to transfer data inbetween NiFi and MiNiFi flows
  As a user of MiNiFi
  I need to have RemoteProcessGroup flow nodes

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance produces and transfers data to a NiFi instance via s2s
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a RemoteProcessGroup node opened on "http://nifi:8080/nifi"
    And the "success" relationship of the GetFile processor is connected to the input port on the RemoteProcessGroup

    And a NiFi flow "nifi" receiving data from a RemoteProcessGroup "from-minifi" on port 8080
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And the "success" relationship of the from-minifi is connected to the PutFile

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 90 seconds

  Scenario: Zero length files are transfered between via s2s if the "drop empty" connection property is false
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a RemoteProcessGroup node opened on "http://nifi:8080/nifi"
    And the "success" relationship of the GenerateFlowFile processor is connected to the input port on the RemoteProcessGroup

    And a NiFi flow "nifi" receiving data from a RemoteProcessGroup "from-minifi" on port 8080
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And the "success" relationship of the from-minifi is connected to the PutFile

    When both instances start up
    Then one empty flowfile is placed in the monitored directory in less than 90 seconds

  Scenario: Zero length files are not transfered between via s2s if the "drop empty" connection property is true
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a RemoteProcessGroup node opened on "http://nifi:8080/nifi"
    And the "success" relationship of the GenerateFlowFile processor is connected to the input port on the RemoteProcessGroup
    And the connection going to the RemoteProcessGroup has "drop empty" set

    And a NiFi flow "nifi" receiving data from a RemoteProcessGroup "from-minifi" on port 8080
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And the "success" relationship of the from-minifi is connected to the PutFile

    When both instances start up
    Then no files are placed in the monitored directory in 90 seconds of running time
