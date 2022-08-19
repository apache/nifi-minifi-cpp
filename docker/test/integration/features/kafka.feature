Feature: Sending data to using Kafka streaming platform using PublishKafka
  In order to send data to a Kafka stream
  As a user of MiNiFi
  I need to have PublishKafka processor

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance transfers data to a kafka broker
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a UpdateAttribute processor
    And these processor properties are set:
      | processor name  | property name          | property value                        |
      | UpdateAttribute | kafka_require_num_acks | 1                                     |
      | UpdateAttribute | kafka_message_key      | unique_message_key_123                |
    And a PublishKafka processor set up to communicate with a kafka broker instance
    And these processor properties are set:
      | processor name | property name          | property value                         |
      | PublishKafka   | Topic Name             | test                                   |
      | PublishKafka   | Delivery Guarantee     | ${kafka_require_num_acks}              |
      | PublishKafka   | Request Timeout        | 12 s                                   |
      | PublishKafka   | Message Timeout        | 13 s                                   |
      | PublishKafka   | Known Brokers          | kafka-broker:${literal(9000):plus(92)} |
      | PublishKafka   | Client Name            | client_no_${literal(6):multiply(7)}    |
      | PublishKafka   | Kafka Key              | ${kafka_message_key}                   |
      | PublishKafka   | retry.backoff.ms       | ${literal(2):multiply(25):multiply(3)} |
      | PublishKafka   | Message Key Field      | kafka.key                              |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the PublishKafka

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the Minifi logs contain the following message: " is 'test'" in less than 60 seconds
    And the Minifi logs contain the following message: "PublishKafka: request.required.acks [1]" in less than 10 seconds
    And the Minifi logs contain the following message: "PublishKafka: request.timeout.ms [12000]" in less than 10 seconds
    And the Minifi logs contain the following message: "PublishKafka: message.timeout.ms [13000]" in less than 10 seconds
    And the Minifi logs contain the following message: "PublishKafka: bootstrap.servers [kafka-broker:9092]" in less than 10 seconds
    And the Minifi logs contain the following message: "PublishKafka: client.id [client_no_42]" in less than 10 seconds
    And the Minifi logs contain the following message: "PublishKafka: Message Key [unique_message_key_123]" in less than 10 seconds
    And the Minifi logs contain the following message: "PublishKafka: DynamicProperty: [retry.backoff.ms] -> [150]" in less than 10 seconds
    And the Minifi logs contain the following message: "The Message Key Field property is set. This property is DEPRECATED and has no effect; please use Kafka Key instead." in less than 10 seconds

  Scenario: PublishKafka sends flowfiles to failure when the broker is not available
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "no broker" is present in "/tmp/input"
    And a PublishKafka processor set up to communicate with a kafka broker instance
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "failure" relationship of the PublishKafka processor is connected to the PutFile

    When the MiNiFi instance starts up
    Then a flowfile with the content "no broker" is placed in the monitored directory in less than 60 seconds

  Scenario: PublishKafka sends can use SSL connect with security properties
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
      | PublishKafka   | Message Timeout        | 12 sec                                     |
      | PublishKafka   | Security CA            | /tmp/resources/certs/ca-cert               |
      | PublishKafka   | Security Cert          | /tmp/resources/certs/client_cert.pem |
      | PublishKafka   | Security Pass Phrase   | abcdefgh                                   |
      | PublishKafka   | Security Private Key   | /tmp/resources/certs/client_cert.key |
      | PublishKafka   | Security Protocol      | ssl                                        |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the PublishKafka

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds

    # We fallback to the flowfile's uuid as message key if the Kafka Key property is not set
    And the Minifi logs match the following regex: "PublishKafka: Message Key \[[a-z0-9]{8}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{12}\]" in less than 10 seconds

  Scenario: A MiNiFi instance transfers data to a kafka broker through SASL Plain security protocol
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishKafka processor set up to communicate with a kafka broker instance
    And these processor properties are set:
      | processor name | property name          | property value                         |
      | PublishKafka   | Topic Name             | test                                   |
      | PublishKafka   | Request Timeout        | 10 sec                                 |
      | PublishKafka   | Message Timeout        | 12 sec                                 |
      | PublishKafka   | Known Brokers          | kafka-broker:9094                      |
      | PublishKafka   | Client Name            | LMN                                    |
      | PublishKafka   | Security Protocol      | sasl_plaintext                         |
      | PublishKafka   | SASL Mechanism         | PLAIN                                  |
      | PublishKafka   | Username               | alice                                  |
      | PublishKafka   | Password               | alice-secret                           |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the PublishKafka

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds

  Scenario: PublishKafka sends can use SASL SSL connect with security properties
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishKafka processor set up to communicate with a kafka broker instance
    And these processor properties are set:
      | processor name | property name          | property value                             |
      | PublishKafka   | Client Name            | LMN                                        |
      | PublishKafka   | Known Brokers          | kafka-broker:9095                          |
      | PublishKafka   | Topic Name             | test                                       |
      | PublishKafka   | Batch Size             | 10                                         |
      | PublishKafka   | Compress Codec         | none                                       |
      | PublishKafka   | Delivery Guarantee     | 1                                          |
      | PublishKafka   | Request Timeout        | 10 sec                                     |
      | PublishKafka   | Message Timeout        | 12 sec                                     |
      | PublishKafka   | Security CA            | /tmp/resources/certs/ca-cert               |
      | PublishKafka   | Security Cert          | /tmp/resources/certs/client_cert.pem |
      | PublishKafka   | Security Pass Phrase   | abcdefgh                                   |
      | PublishKafka   | Security Private Key   | /tmp/resources/certs/client_cert.key |
      | PublishKafka   | Security Protocol      | sasl_ssl                                   |
      | PublishKafka   | SASL Mechanism         | PLAIN                                      |
      | PublishKafka   | Username               | alice                                      |
      | PublishKafka   | Password               | alice-secret                               |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the PublishKafka

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds

  Scenario: PublishKafka sends can use SASL SSL connect with SSL Context
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishKafka processor set up to communicate with a kafka broker instance
    And these processor properties are set:
      | processor name | property name          | property value                             |
      | PublishKafka   | Client Name            | LMN                                        |
      | PublishKafka   | Known Brokers          | kafka-broker:9095                          |
      | PublishKafka   | Topic Name             | test                                       |
      | PublishKafka   | Batch Size             | 10                                         |
      | PublishKafka   | Compress Codec         | none                                       |
      | PublishKafka   | Delivery Guarantee     | 1                                          |
      | PublishKafka   | Request Timeout        | 10 sec                                     |
      | PublishKafka   | Message Timeout        | 12 sec                                     |
      | PublishKafka   | Security Protocol      | sasl_ssl                                   |
      | PublishKafka   | SASL Mechanism         | PLAIN                                      |
      | PublishKafka   | Username               | alice                                      |
      | PublishKafka   | Password               | alice-secret                               |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And an ssl context service set up for PublishKafka
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the PublishKafka

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds

  Scenario: PublishKafka sends can use SSL connect with SSL Context Service
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
      | PublishKafka   | Message Timeout        | 12 sec                                     |
      | PublishKafka   | Security Protocol      | ssl                                        |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And an ssl context service set up for PublishKafka
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the PublishKafka

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds

    # We fallback to the flowfile's uuid as message key if the Kafka Key property is not set
    And the Minifi logs match the following regex: "PublishKafka: Message Key \[[a-z0-9]{8}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{12}\]" in less than 10 seconds

  Scenario: MiNiFi consumes data from a kafka topic
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the third-party kafka publisher

    When all instances start up
    And a message with content "some test message" is published to the "ConsumeKafkaTest" topic

    Then at least one flowfile with the content "some test message" is placed in the monitored directory in less than 60 seconds

  Scenario Outline: ConsumeKafka parses and uses kafka topics and topic name formats
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And the "Topic Names" property of the ConsumeKafka processor is set to "<topic names>"
    And the "Topic Name Format" property of the ConsumeKafka processor is set to "<topic name format>"
    And the "Offset Reset" property of the ConsumeKafka processor is set to "earliest"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the third-party kafka publisher
    And the kafka broker is started
    And the topic "ConsumeKafkaTest" is initialized on the kafka broker

    When a message with content "<message 1>" is published to the "ConsumeKafkaTest" topic
    And all other processes start up
    And a message with content "<message 2>" is published to the "ConsumeKafkaTest" topic

    Then two flowfiles with the contents "<message 1>" and "<message 2>" are placed in the monitored directory in less than 90 seconds

  Examples: Topic names and formats to test
    | message 1            | message 2           | topic names              | topic name format |
    | Ulysses              | James Joyce         | ConsumeKafkaTest         | (not set)         |
    | The Great Gatsby     | F. Scott Fitzgerald | ConsumeKafkaTest         | Names             |
    | War and Peace        | Lev Tolstoy         | a,b,c,ConsumeKafkaTest,d | Names             |
    | Nineteen Eighty Four | George Orwell       | ConsumeKafkaTest         | Patterns          |
    | Hamlet               | William Shakespeare | Cons[emu]*KafkaTest      | Patterns          |

  Scenario: ConsumeKafka consumes only messages after MiNiFi startup when "Offset Reset" property is set to latest
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And the "Topic Names" property of the ConsumeKafka processor is set to "ConsumeKafkaTest"
    And the "Offset Reset" property of the ConsumeKafka processor is set to "latest"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the third-party kafka publisher
    And the kafka broker is started
    And the topic "ConsumeKafkaTest" is initialized on the kafka broker

    When a message with content "Ulysses" is published to the "ConsumeKafkaTest" topic
    And all other processes start up
    And the Kafka consumer is registered in kafka broker
    And a message with content "James Joyce" is published to the "ConsumeKafkaTest" topic

    Then a flowfile with the content "James Joyce" is placed in the monitored directory in less than 60 seconds

  Scenario Outline: ConsumeKafka key attribute is encoded according to the "Key Attribute Encoding" property
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And the "Key Attribute Encoding" property of the ConsumeKafka processor is set to "<key attribute encoding>"
    And a RouteOnAttribute processor in the "kafka-consumer-flow" flow
    And a LogAttribute processor in the "kafka-consumer-flow" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And the "success" property of the RouteOnAttribute processor is set to match <key attribute encoding> encoded kafka message key "consume_kafka_test_key"

    And the "success" relationship of the ConsumeKafka processor is connected to the LogAttribute
    And the "success" relationship of the LogAttribute processor is connected to the RouteOnAttribute
    And the "success" relationship of the RouteOnAttribute processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the third-party kafka publisher

    When all instances start up
    And a message with content "<message 1>" is published to the "ConsumeKafkaTest" topic with key "consume_kafka_test_key"
    And a message with content "<message 2>" is published to the "ConsumeKafkaTest" topic with key "consume_kafka_test_key"

    Then two flowfiles with the contents "<message 1>" and "<message 2>" are placed in the monitored directory in less than 45 seconds

  Examples: Key attribute encoding values
    | message 1            | message 2                     | key attribute encoding |
    | The Odyssey          | Ὅμηρος                        | (not set)              |
    | Lolita               | Владимир Владимирович Набоков | utf-8                  |
    | Crime and Punishment | Фёдор Михайлович Достоевский  | hex                    |
    | Paradise Lost        | John Milton                   | hEX                    |

  Scenario Outline: ConsumeKafka transactional behaviour is supported
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And the "Topic Names" property of the ConsumeKafka processor is set to "ConsumeKafkaTest"
    And the "Honor Transactions" property of the ConsumeKafka processor is set to "<honor transactions>"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the third-party kafka publisher

    When all instances start up
    And the publisher performs a <transaction type> transaction publishing to the "ConsumeKafkaTest" topic these messages: <messages sent>

    Then <number of flowfiles expected> flowfiles are placed in the monitored directory in less than 15 seconds

  Examples: Transaction descriptions
    | messages sent                     | transaction type             | honor transactions | number of flowfiles expected |
    | Pride and Prejudice, Jane Austen  | SINGLE_COMMITTED_TRANSACTION | (not set)          | 2                            |
    | Dune, Frank Herbert               | TWO_SEPARATE_TRANSACTIONS    | (not set)          | 2                            |
    | The Black Sheep, Honore De Balzac | NON_COMMITTED_TRANSACTION    | (not set)          | 0                            |
    | Gospel of Thomas                  | CANCELLED_TRANSACTION        | (not set)          | 0                            |
    | Operation Dark Heart              | CANCELLED_TRANSACTION        | true               | 0                            |
    | Brexit                            | CANCELLED_TRANSACTION        | false              | 1                            |

  Scenario Outline: Headers on consumed kafka messages are extracted into attributes if requested on ConsumeKafka
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And the "Headers To Add As Attributes" property of the ConsumeKafka processor is set to "<headers to add as attributes>"
    And the "Duplicate Header Handling" property of the ConsumeKafka processor is set to "<duplicate header handling>"
    And a RouteOnAttribute processor in the "kafka-consumer-flow" flow
    And a LogAttribute processor in the "kafka-consumer-flow" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And the "success" property of the RouteOnAttribute processor is set to match the attribute "<headers to add as attributes>" to "<expected value>"

    And the "success" relationship of the ConsumeKafka processor is connected to the LogAttribute
    And the "success" relationship of the LogAttribute processor is connected to the RouteOnAttribute
    And the "success" relationship of the RouteOnAttribute processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the third-party kafka publisher

    When all instances start up
    And a message with content "<message 1>" is published to the "ConsumeKafkaTest" topic with headers "<message headers sent>"
    And a message with content "<message 2>" is published to the "ConsumeKafkaTest" topic with headers "<message headers sent>"

    Then two flowfiles with the contents "<message 1>" and "<message 2>" are placed in the monitored directory in less than 45 seconds

  Examples: Messages with headers
    | message 1             | message 2         | message headers sent        | headers to add as attributes | expected value       | duplicate header handling |
    | Homeland              | R. A. Salvatore   | Contains dark elves: yes    | (not set)                    | (not set)            | (not set)                 |
    | Magician              | Raymond E. Feist  | Rating: 10/10               | Rating                       | 10/10                | (not set)                 |
    | Mistborn              | Brandon Sanderson | Metal: Copper; Metal: Iron  | Metal                        | Copper               | Keep First                |
    | Mistborn              | Brandon Sanderson | Metal: Copper; Metal: Iron  | Metal                        | Iron                 | Keep Latest               |
    | Mistborn              | Brandon Sanderson | Metal: Copper; Metal: Iron  | Metal                        | Copper, Iron         | Comma-separated Merge     |
    | The Lord of the Rings | J. R. R. Tolkien  | Parts: First, second, third | Parts                        | First, second, third | (not set)                 |

  Scenario: Messages are separated into multiple flowfiles if the message demarcator is present in the message
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And the "Message Demarcator" property of the ConsumeKafka processor is set to "a"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow

    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the third-party kafka publisher

    When all instances start up
    And a message with content "Barbapapa" is published to the "ConsumeKafkaTest" topic
    And a message with content "Anette Tison and Talus Taylor" is published to the "ConsumeKafkaTest" topic

    Then flowfiles with these contents are placed in the monitored directory in less than 45 seconds: "B,rb,p,Anette Tison ,nd T,lus T,ylor"

  Scenario Outline: The ConsumeKafka "Maximum Poll Records" property sets a limit on the messages processed in a single batch
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And a LogAttribute processor in the "kafka-consumer-flow" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow

    And the "Max Poll Records" property of the ConsumeKafka processor is set to "<max poll records>"
    And the scheduling period of the ConsumeKafka processor is set to "<scheduling period>"
    And the scheduling period of the LogAttribute processor is set to "<scheduling period>"
    And the "FlowFiles To Log" property of the LogAttribute processor is set to "<max poll records>"

    And the "success" relationship of the ConsumeKafka processor is connected to the LogAttribute
    And the "success" relationship of the LogAttribute processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the third-party kafka publisher

    When all instances start up
    And 1000 kafka messages are sent to the topic "ConsumeKafkaTest"

    Then after a wait of <polling time>, at least <min expected messages> and at most <max expected messages> flowfiles are produced and placed in the monitored directory

  Examples: Message batching
    | max poll records | scheduling period | polling time | min expected messages | max expected messages |
    | 3                | 5 sec             | 15 seconds   | 6                     | 12                    |
    | 6                | 5 sec             | 15 seconds   | 12                    | 24                    |

  Scenario Outline: Unsupported encoding attributes for ConsumeKafka throw scheduling errors
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And the "<property name>" property of the ConsumeKafka processor is set to "<property value>"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the third-party kafka publisher

    When all instances start up
    And a message with content "<message 1>" is published to the "ConsumeKafkaTest" topic
    And a message with content "<message 2>" is published to the "ConsumeKafkaTest" topic

    Then no files are placed in the monitored directory in 20 seconds of running time

    Examples: Unsupported property values
      | message 1        | message 2      | property name           | property value |
      | Miyamoto Musashi | Eiji Yoshikawa | Key Attribute Encoding  | UTF-32         |
      | Shogun           | James Clavell  | Message Header Encoding | UTF-32         |

  Scenario: ConsumeKafka receives data via SSL
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And these processor properties are set:
      | processor name | property name        | property value                             |
      | ConsumeKafka   | Kafka Brokers        | kafka-broker:9093                          |
      | ConsumeKafka   | Security Protocol    | ssl                                        |
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And an ssl context service set up for ConsumeKafka
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the publisher flow

    When all instances start up
    And a message with content "Alice's Adventures in Wonderland" is published to the "ConsumeKafkaTest" topic using an ssl connection
    And a message with content "Lewis Carroll" is published to the "ConsumeKafkaTest" topic using an ssl connection

    Then two flowfiles with the contents "Alice's Adventures in Wonderland" and "Lewis Carroll" are placed in the monitored directory in less than 60 seconds

  Scenario: ConsumeKafka receives data via SASL SSL
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And these processor properties are set:
      | processor name | property name        | property value                             |
      | ConsumeKafka   | Kafka Brokers        | kafka-broker:9095                          |
      | ConsumeKafka   | Security Protocol    | sasl_ssl                                   |
      | ConsumeKafka   | SASL Mechanism       | PLAIN                                      |
      | ConsumeKafka   | Username             | alice                                      |
      | ConsumeKafka   | Password             | alice-secret                               |
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And an ssl context service set up for ConsumeKafka
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker is set up in correspondence with the publisher flow

    When all instances start up
    And a message with content "Alice's Adventures in Wonderland" is published to the "ConsumeKafkaTest" topic using an ssl connection
    And a message with content "Lewis Carroll" is published to the "ConsumeKafkaTest" topic using an ssl connection

    Then two flowfiles with the contents "Alice's Adventures in Wonderland" and "Lewis Carroll" are placed in the monitored directory in less than 60 seconds

  Scenario: MiNiFi consumes data from a kafka topic via SASL PLAIN connection
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile
    And these processor properties are set:
      | processor name | property name        | property value                             |
      | ConsumeKafka   | Kafka Brokers        | kafka-broker:9094                          |
      | ConsumeKafka   | Security Protocol    | sasl_plaintext                             |
      | ConsumeKafka   | SASL Mechanism       | PLAIN                                      |
      | ConsumeKafka   | Username             | alice                                      |
      | ConsumeKafka   | Password             | alice-secret                               |

    And a kafka broker is set up in correspondence with the third-party kafka publisher

    When all instances start up
    And a message with content "some test message" is published to the "ConsumeKafkaTest" topic

    Then at least one flowfile with the content "some test message" is placed in the monitored directory in less than 60 seconds
