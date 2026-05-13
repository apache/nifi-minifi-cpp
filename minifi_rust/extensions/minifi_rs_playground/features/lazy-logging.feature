@SUPPORTS_WINDOWS
Feature: Logs should be lazily evaluated

  Scenario: CountActualLogging only increments self when actually logging
    Given a CountActualLogging processor
    And log property "logger.minifi_rs_playground::processors::count_actual_logging::CountActualLogging" is set to "INFO,stderr"
    And CountActualLogging is TIMER_DRIVEN with 1 min scheduling period

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "[minifi_rs_playground::processors::count_actual_logging::CountActualLogging] [info] info 1" in less than 10 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings
