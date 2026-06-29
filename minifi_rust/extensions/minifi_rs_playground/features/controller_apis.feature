@SUPPORTS_WINDOWS
Feature: Testing controller service api casting

  Scenario: Zoo has a jetpack dog
    Given a ZooProcessorRs processor with the "Can fly service" property set to "Wolfie the magical"
    And the "Number of Legs service" property of the ZooProcessorRs processor is set to "Wolfie the magical"
    And a DogControllerRs controller service named "Wolfie the magical" is set up and the "Has Jetpack" property set to "true"
    And the "Extra information" property of the Wolfie the magical controller service is set to "The dog (Canis familiaris or Canis lupus familiaris) is a domesticated descendant of wolves."
    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "[minifi_rs_playground::processors::zoo_processor::ZooProcessorRs] [critical] Can DogControllerRs { has_jetpack: true, extra_info: "The dog (Canis familiaris or Canis lupus familiaris) is a domesticated descendant of wolves." } fly? true" in less than 10 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings

