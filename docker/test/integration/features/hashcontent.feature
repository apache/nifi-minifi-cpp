Feature: Hash value is added to Flowfiles by HashContent processor
  In order to avoid duplication of content of Flowfiles
  As a user of MiNiFi
  I need to have HashContent processor to calculate and add hash value

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario Outline: HashContent adds hash attribute to flowfiles
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content <content> is present in "/tmp/input"
    And a HashContent processor with the "Hash Attribute" property set to "hash"
    And the "Hash Algorithm" property of the HashContent processor is set to "<hash_algorithm>"
    And a LogAttribute processor
    And the "success" relationship of the GetFile processor is connected to the HashContent
    And the "success" relationship of the HashContent processor is connected to the LogAttribute
    When the MiNiFi instance starts up
    Then the Minifi logs contain the following message: "key:hash value:<hash_value>" in less than 60 seconds

    Examples:
      | content  | hash_algorithm | hash_value                                                       |
      | "apple"  | MD5            | 1F3870BE274F6C49B3E31A0C6728957F                                 |
      | "test"   | SHA1           | A94A8FE5CCB19BA61C4C0873D391E987982FBBD3                         |
      | "coffee" | SHA256         | 37290D74AC4D186E3A8E5785D259D2EC04FAC91AE28092E7620EC8BC99E830AA |


  Scenario: HashContent fails for an empty file if 'fail on empty' property is set to true
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And an empty file is present in "/tmp/input"
    And a HashContent processor with the "Hash Attribute" property set to "hash"
    And the "Hash Algorithm" property of the HashContent processor is set to "MD5"
    And the "Fail on empty" property of the HashContent processor is set to "true"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the HashContent
    And the "failure" relationship of the HashContent processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then at least one empty flowfile is placed in the monitored directory in less than 10 seconds
