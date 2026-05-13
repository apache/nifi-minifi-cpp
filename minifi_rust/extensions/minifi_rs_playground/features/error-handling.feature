@SUPPORTS_WINDOWS
Feature: API error handling and logging

  Scenario: The Api handles empty flow-files
    Given a GenerateFlowFileRs processor with the "Custom Text" property set to "${invalid_attribute}"
    And the "Data Format" property of the GenerateFlowFileRs processor is set to "Text"
    And the "Unique FlowFiles" property of the GenerateFlowFileRs processor is set to "false"
    And a LogAttributeRs processor with the "Log Level" property set to "Critical"
    And the "success" relationship of the GenerateFlowFileRs processor is connected to the LogAttributeRs
    And LogAttributeRs's success relationship is auto-terminated

    When the MiNiFi instance starts up

    Then after 3 sec have passed
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings

  Scenario: Minifi handles errors from on_schedule
    Given a KamikazeProcessorRs processor with the "On Schedule Behaviour" property set to "ReturnErr"
    And KamikazeProcessorRs's success relationship is auto-terminated

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "KamikazeProcessorRs] [error] Error during on_schedule: ScheduleError("it was designed to fail during schedule")" in less than 10 seconds
    And the Minifi logs contain the following message: "(KamikazeProcessorRs): Process Schedule Operation: Error while scheduling processor" in less than 10 seconds

  Scenario: Minifi handles errors from on_trigger
    Given a KamikazeProcessorRs processor with the "On Schedule Behaviour" property set to "ReturnOk"
    And the "On Trigger Behaviour" property of the KamikazeProcessorRs processor is set to "ReturnErr"
    And KamikazeProcessorRs's success relationship is auto-terminated

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "KamikazeProcessorRs] [error] Error during on_trigger TriggerError("it was designed to fail in trigger")" in less than 10 seconds
    And the Minifi logs contain the following message: "Trigger and commit failed for processor KamikazeProcessorRs" in less than 10 seconds

  Scenario: Panic in extension's on_schedule crashes the agent aswell
    Given a KamikazeProcessorRs processor with the "On Schedule Behaviour" property set to "Panic"
    And KamikazeProcessorRs's success relationship is auto-terminated

    When the MiNiFi instance is started without assertions
    Then Minifi crashes with the following "KamikazeProcessor::on_schedule panic" in less than 10 seconds

  Scenario: Panic in extension's on_trigger crashes the agent aswell
    Given a KamikazeProcessorRs processor with the "On Schedule Behaviour" property set to "ReturnOk"
    And the "On Trigger Behaviour" property of the KamikazeProcessorRs processor is set to "Panic"
    And KamikazeProcessorRs's success relationship is auto-terminated

    When the MiNiFi instance is started without assertions
    Then Minifi crashes with the following "KamikazeProcessor::on_trigger panic" in less than 10 seconds

  Scenario: Get not supported property
    Given a KamikazeProcessorRs processor with the "On Schedule Behaviour" property set to "ReturnOk"
    And the "On Trigger Behaviour" property of the KamikazeProcessorRs processor is set to "GetNotRegisteredProperty"
    And KamikazeProcessorRs's success relationship is auto-terminated

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "MinifiProcessContextGetProperty("Kamikaze Processor Property"), not supported property" in less than 10 seconds
    And the Minifi logs contain the following message: "Trigger and commit failed for processor KamikazeProcessorRs" in less than 10 seconds

  Scenario: Get wrong typed Controller Service
    Given a LoremIpsumCSUser processor with the "Lorem Ipsum Controller Service" property set to "My Controller Service"
    And a DummyControllerService controller service is set up
    And LoremIpsumCSUser's success relationship is auto-terminated

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "MinifiProcessContextGetControllerService::<"minifi_rs_playground::controller_services::lorem_ipsum_controller_service::LoremIpsumControllerService">("My Controller Service"), validation failed" in less than 10 seconds
    And the Minifi logs contain the following message: "Trigger and commit failed for processor LoremIpsumCSUser" in less than 10 seconds
