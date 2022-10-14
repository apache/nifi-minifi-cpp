Feature: MiNiFi can communicate with Apache NiFi MiNiFi C2 server

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: MiNiFi flow config is updated from MiNiFi C2 server
    Given a GetFile processor with the name "GetFile1" and the "Input Directory" property set to "/tmp/non-existent"
    And a file with the content "test" is present in "/tmp/input"
    And a MiNiFi C2 server is set up
    When all instances start up
    Then the MiNiFi C2 server logs contain the following message: "acknowledged with a state of FULLY_APPLIED(DONE)" in less than 30 seconds
    And a flowfile with the content "test" is placed in the monitored directory in less than 10 seconds

  Scenario: MiNiFi flow config is updated from MiNiFi C2 server through SSL
    Given a file with the content "test" is present in "/tmp/input"
    And a ssl context service is set up for MiNiFi C2 server
    And a MiNiFi C2 server is set up with SSL
    When all instances start up
    Then the MiNiFi C2 SSL server logs contain the following message: "acknowledged with a state of FULLY_APPLIED(DONE)" in less than 60 seconds
    And a flowfile with the content "test" is placed in the monitored directory in less than 10 seconds
