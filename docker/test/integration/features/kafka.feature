Feature: Sending data to using Kafka streaming platform using PublishKafka
  In order to send data to a Kafka stream
  As a user of MiNiFi
  I need to have PublishKafka processor

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance transfers data to a kafka broker
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishKafka processor set up to communicate with a kafka broker instance
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile

    And a kafka broker "broker" is set up in correspondence with the PublishKafka

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds

  Scenario: PublishKafka sends flowfiles to failure when the broker is not available
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "no broker" is present in "/tmp/input"
    And a PublishKafka processor set up to communicate with a kafka broker instance
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "failure" relationship of the PublishKafka processor is connected to the PutFile

    When the MiNiFi instance starts up
    Then a flowfile with the content "no broker" is placed in the monitored directory in less than 60 seconds

  Scenario: PublishKafka sends can use SSL connect
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishKafka processor set up to communicate with a kafka broker instance
    And these processor properties are set:
      | processor name | property name          | property value                             |
      | PublishKafka   | Client Name            | LMN                                        |
      | PublishKafka   | Known Brokers          | kafka-broker:9093                          |
      | PublishKafka   | Topic Name             | test                                       |
      | PublishKafka   | Batch Size             | 10                                         |
      | PublishKafka   | Compress Codec         | none                                       |
      | PublishKafka   | Delivery Guarantee     | 1                                          |
      | PublishKafka   | Request Timeout        | 10 sec                                     |
      | PublishKafka   | Message Timeout Phrase | 12 sec                                     |
      | PublishKafka   | Security CA Key        | /tmp/resources/certs/ca-cert               |
      | PublishKafka   | Security Cert          | /tmp/resources/certs/client_LMN_client.pem |
      | PublishKafka   | Security Pass Phrase   | abcdefgh                                   |
      | PublishKafka   | Security Private Key   | /tmp/resources/certs/client_LMN_client.key |
      | PublishKafka   | Security Protocol      | ssl                                        |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the GetFile processor is connected to the PutFile

    And a kafka broker "broker" is set up in correspondence with the PublishKafka

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
