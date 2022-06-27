Feature: Minifi C++ can act as a network listener

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A TCP client can send messages to Minifi
    Given a ListenTCP processor
    And the "Listening Port" property of the ListenTCP processor is set to "10254"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And a TCP client is set up to send a test TCP message to minifi
    And the "success" relationship of the ListenTCP processor is connected to the PutFile

    When both instances start up
    Then at least one flowfile with the content "test_tcp_message" is placed in the monitored directory in less than 60 seconds
