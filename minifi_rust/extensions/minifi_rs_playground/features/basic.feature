@SUPPORTS_WINDOWS
Feature: Basic scenarios
  
  Scenario: The rust library is loaded into minifi
    Given log property "logger.org::apache::nifi::minifi::core::extension::ExtensionManager" is set to "TRACE,stderr"
    And log property "logger.org::apache::nifi::minifi::core::ClassLoader" is set to "TRACE,stderr"

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "Registering class 'GenerateFlowFileRs' at '/minifi_rs_playground'" in less than 10 seconds
    And the Minifi logs contain the following message: "Registering class 'GetFileRs' at '/minifi_rs_playground'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'KamikazeProcessorRs' at '/minifi_rs_playground'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'LogAttributeRs' at '/minifi_rs_playground'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'PutFileRs' at '/minifi_rs_playground'" in less than 1 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings

  Scenario: Simple GenerateFlowFileRs -> PutFileRs
    Given a GenerateFlowFileRs processor with the "Custom Text" property set to "Ferris the crab"
    And the "Data Format" property of the GenerateFlowFileRs processor is set to "Text"
    And the "Unique FlowFiles" property of the GenerateFlowFileRs processor is set to "false"
    And a PutFileRs processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GenerateFlowFileRs processor is connected to the PutFileRs
    And PutFileRs's success relationship is auto-terminated

    When the MiNiFi instance starts up

    Then at least one file with the content "Ferris the crab" is placed in the "/tmp/output" directory in less than 10 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings

  Scenario: Simple GetFileRs -> PutFileRs
    Given a GetFileRs processor with the "Input Directory" property set to "/tmp/input"
    And a PutFileRs processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFileRs processor is connected to the PutFileRs
    And PutFileRs's success relationship is auto-terminated
    And PutFileRs's failure relationship is auto-terminated
    And a directory at "/tmp/input" has a file "test_file.log" with the content "test content"

    When the MiNiFi instance starts up

    Then at least one file with the content "test content" is placed in the "/tmp/output" directory in less than 10 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings

  Scenario Outline: The LogAttributeRs can read and log FlowFile content
    Given a GenerateFlowFileRs processor with the "Custom Text" property set to "<custom_text>"
    And the "Data Format" property of the GenerateFlowFileRs processor is set to "Text"
    And the "Unique FlowFiles" property of the GenerateFlowFileRs processor is set to "false"
    And a LogAttributeRs processor with the "Log Level" property set to "<log_level>"
    And the "Log Payload" property of the LogAttributeRs processor is set to "true"
    And the "success" relationship of the GenerateFlowFileRs processor is connected to the LogAttributeRs
    And LogAttributeRs's success relationship is auto-terminated
    And log property "logger.minifi_rs_playground::processors::log_attribute::LogAttributeRs" is set to "TRACE,stderr"

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "<expected_log_1>" in less than 20 seconds
    And the Minifi logs contain the following message: "<expected_log_2>" in less than 1 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings
    Examples:
      | custom_text | log_level | expected_log_1                   | expected_log_2 |
      | Elephant    | Critical  | [critical] Logging for flow file | Elephant       |
      | Lynx        | Info      | [info] Logging for flow file     | Lynx           |
      | Ant         | Trace     | [trace] Logging for flow file    | Ant            |

  Scenario Outline: Controller services work
    Given a LoremIpsumCSUser processor with the "Lorem Ipsum Controller Service" property set to "LoremIpsumControllerService"
    And the "Write Method" property of the LoremIpsumCSUser processor is set to "<write_method>"
    And a LoremIpsumControllerService controller service is set up and the "Length" property set to "200000"
    And a PutFileRs processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the LoremIpsumCSUser processor is connected to the PutFileRs
    And PutFileRs's success relationship is auto-terminated

    When the MiNiFi instance starts up

    Then at least one file with minimum size of "1 MB" is placed in the "/tmp/output" directory in less than 10 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings
    Examples:
      | write_method |
      | Buffer       |
      | Stream       |
