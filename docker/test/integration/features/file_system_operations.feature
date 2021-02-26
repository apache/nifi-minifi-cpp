Feature: File system operations are handled by the GetFile and PutFile processors
  In order to store and access data on the local file system
  As a user of MiNiFi
  I need to have GetFile and PutFile processors

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: Get and put operations run in a simple flow
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 10 seconds

  Scenario: PutFile does not overwrite a file that already exists
    Given a set of processors:
      | type    | name      | uuid                                 |
      | GetFile | GetFile   | 66259995-11da-41df-bff7-e262d5f6d7c9 |
      | PutFile | PutFile_1 | 694423a0-26f3-4e95-9f9f-c03b6d6c189d |
      | PutFile | PutFile_2 | f37e51e9-ad67-4e16-9dc6-ad853b0933e3 |
      | PutFile | PutFile_3 | f37e51e9-ad67-4e16-9dc6-ad853b0933e4 |

    And these processor properties are set:
      | processor name | property name   | property value |
      | GetFile        | Input Directory | /tmp/input     |
      | PutFile_1      | Input Directory | /tmp           |
      | PutFile_2      | Directory       | /tmp           |
      | PutFile_3      | Directory       | /tmp/output    |

    And the processors are connected up as described here:
      | source name | relationship name | destination name |
      | GetFile     | success           | PutFile_1        |
      | PutFile_1   | success           | PutFile_2        |
      | PutFile_2   | failure           | PutFile_3        |

    And a file with the content "test" is present in "/tmp/input"
    When the MiNiFi instance starts up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 10 seconds

  Scenario Outline: MiNiFi is capable of manipulating flowfiles of different sizes
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with <file size> of content is present in "/tmp/input"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then a flowfile with matching content is placed in the monitored directory in less than 20 seconds

  Examples: File size
    | file size |
    | 10 B      |
    | 1.5 KiB   |
    | 10 MiB    |
    | 1.0 GiB   |
    | 2.1 GiB   |
