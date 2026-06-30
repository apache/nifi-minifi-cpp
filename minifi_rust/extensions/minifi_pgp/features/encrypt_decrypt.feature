@SUPPORTS_WINDOWS
Feature: Test PGP extension's encryption and decryption capabilities

  Background: The pgp library is successfully built on linux

  Scenario: The pgp library is loaded into minifi
    Given log property "logger.org::apache::nifi::minifi::core::extension::ExtensionManager" is set to "TRACE,stderr"
    And log property "logger.org::apache::nifi::minifi::core::ClassLoader" is set to "TRACE,stderr"

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "Registering class 'EncryptContentPGP' at '/minifi_pgp'" in less than 10 seconds
    And the Minifi logs contain the following message: "Registering class 'DecryptContentPGP' at '/minifi_pgp'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'PGPPublicKeyService' at '/minifi_pgp'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'PGPPrivateKeyService' at '/minifi_pgp'" in less than 1 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings

  Scenario: Encrypted for Alice but not for Bob
    Given log property "logger.minifi_pgp::processors::decrypt_content::DecryptContentPGP" is set to "TRACE,stderr"
    And log property "logger.minigi_pgp::processors::encrypt_content::EncryptContentPGP" is set to "TRACE,stderr"

    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And an EncryptContentPGP processor with a PGPPublicKeyService is set up
    And a DecryptContentPGP processor named DecryptAlice with a PGPPrivateKeyService is set up for Alice
    And a DecryptContentPGP processor named DecryptBob with a PGPPrivateKeyService is set up for Bob
    And a PutFile processor with the name "AliceSuccess"
    And a PutFile processor with the name "BobFailure"

    And these processor properties are set
      | processor name    | property name     | property value       |
      | EncryptContentPGP | File Encoding     | ASCII                |
      | EncryptContentPGP | Public Key Search | Alice                |
      | AliceSuccess      | Directory         | /tmp/output/alice_ok |
      | BobFailure        | Directory         | /tmp/output/bob_fail |

    And the processors are connected up as described here
      | source name       | relationship name | destination name  |
      | GetFile           | success           | EncryptContentPGP |
      | EncryptContentPGP | success           | DecryptAlice      |
      | EncryptContentPGP | success           | DecryptBob        |
      | DecryptAlice      | success           | AliceSuccess      |
      | DecryptBob        | failure           | BobFailure        |

    And AliceSuccess's success relationship is auto-terminated
    And BobFailure's success relationship is auto-terminated

    And a directory at "/tmp/input" has a file "test_file.log" with the content "test content"

    When the MiNiFi instance starts up

    Then at least one file with the content "test content" is placed in the "/tmp/output/alice_ok" directory in less than 5 seconds
    And an encrypted armored pgp file is placed in the "/tmp/output/bob_fail" directory in less than 5 seconds
    And the Minifi logs do not contain errors
