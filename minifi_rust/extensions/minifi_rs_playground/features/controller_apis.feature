@SUPPORTS_WINDOWS
Feature: Testing controller service api casting

  Scenario: Zoo has a jetpack dog and a normal duck
    Given a ZooProcessor processor with the "Can fly service" property set to "Wolfie the magical"
    And the "Number of Legs Service" property of the ZooProcessor processor is set to "Wolfie the magical"
    And a DogController controller service named "Wolfie the magical" is set up and the "Has Jetpack" property set to "true"
    And the "Extra information" property of the Wolfie the magical controller service is set to "The dog (Canis familiaris or Canis lupus familiaris) is a domesticated descendant of wolves."
    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "[minifi_rs_playground::processors::zoo_processor::ZooProcessor] [critical] Can DogController { has_jetpack: true, extra_info: "The dog (Canis familiaris or Canis lupus familiaris) is a domesticated descendant of wolves." } fly? true" in less than 10 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings

