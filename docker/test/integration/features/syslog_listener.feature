Feature: Minifi C++ can act as a syslog listener

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A syslog client can send messages to Minifi over UDP
    Given a ListenSyslog processor
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "Protocol" property of the ListenSyslog processor is set to "UDP"
    And the "Parse Messages" property of the ListenSyslog processor is set to "yes"
    And a Syslog client with UDP protocol is setup to send logs to minifi
    And the "success" relationship of the ListenSyslog processor is connected to the PutFile

    When both instances start up
    Then at least one flowfile is placed in the monitored directory in less than 10 seconds

  Scenario: A syslog client can send messages to Minifi over TCP
    Given a ListenSyslog processor
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "Protocol" property of the ListenSyslog processor is set to "TCP"
    And the "Parse Messages" property of the ListenSyslog processor is set to "yes"
    And a Syslog client with TCP protocol is setup to send logs to minifi
    And the "success" relationship of the ListenSyslog processor is connected to the PutFile

    When both instances start up
    Then at least one flowfile is placed in the monitored directory in less than 10 seconds
