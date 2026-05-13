@SUPPORTS_WINDOWS
Feature: Testing streaming reads and writes

  Scenario: Streaming Transforms work
    Given a GetFileRs processor with the "Input Directory" property set to "/tmp/input"
    And a AsciifyGerman processor
    And a PutFileRs processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFileRs processor is connected to the AsciifyGerman
    And the "success" relationship of the AsciifyGerman processor is connected to the PutFileRs
    And PutFileRs's success relationship is auto-terminated
    And PutFileRs's failure relationship is auto-terminated
    And a directory at "/tmp/input" has a file "german.txt" with the content "Üben von Xylophon und Querflöte ist ja zweckmäßig."

    When the MiNiFi instance starts up

    Then at least one file with the content "Ueben von Xylophon und Querfloete ist ja zweckmaessig." is placed in the "/tmp/output" directory in less than 10 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings

  Scenario: Streaming can be cancelled
    Given a GetFileRs processor with the "Input Directory" property set to "/tmp/input"
    And a AsciifyGerman processor
    And a PutFileRs processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFileRs processor is connected to the AsciifyGerman
    And the "failure" relationship of the AsciifyGerman processor is connected to the PutFileRs
    And PutFileRs's success relationship is auto-terminated
    And PutFileRs's failure relationship is auto-terminated
    And a directory at "/tmp/input" has a file "french.txt" with the content "Voix ambiguë d'un cœur qui, au zéphyr, préfère les jattes de kiwis."

    When the MiNiFi instance starts up

    Then at least one file with the content "Voix ambiguë d'un cœur qui, au zéphyr, préfère les jattes de kiwis." is placed in the "/tmp/output" directory in less than 10 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings
