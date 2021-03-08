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

  Scenario: MiNiFi consumes data from a kafka topic
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker "broker" is set up in correspondence with the third-party kafka publisher

    When all instances start up
    And a message with content "some test message" is published to the "ConsumeKafkaTest" topic

    Then at least one flowfile with the content "some test message" is placed in the monitored directory in less than 60 seconds

  Scenario Outline: ConsumeKafka parses and uses kafka topics and topic name formats
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And the "Topic Names" of the ConsumeKafka processor is set to "<topic names>"
    And the "Topic Name Format" of the ConsumeKafka processor is set to "<topic name format>"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker "broker" is set up in correspondence with the third-party kafka publisher
    And the kafka broker "broker" is started
    And the topic "ConsumeKafkaTest" is initialized on the kafka broker

    When all other processes start up
    And a message with content "<message 1>" is published to the "ConsumeKafkaTest" topic
    And a message with content "<message 2>" is published to the "ConsumeKafkaTest" topic

    Then two flowfiles with the contents "<message 1>" and "<message 2>" are placed in the monitored directory in less than 45 seconds

  Examples: Topic names and formats to test
    | message 1            | message 2           | topic names              | topic name format |
    | Ulysses              | James Joyce         | ConsumeKafkaTest         | (not set)         |
    | The Great Gatsby     | F. Scott Fitzgerald | ConsumeKafkaTest         | Names             |
    | War and Peace        | Lev Tolstoy         | a,b,c,ConsumeKafkaTest,d | Names             |
    | Nineteen Eighty Four | George Orwell       | ConsumeKafkaTest         | Patterns          |
    | Hamlet               | William Shakespeare | Cons[emu]*KafkaTest      | Patterns          |

  Scenario Outline: ConsumeKafka key attribute is encoded according to the "Key Attribute Encoding" property
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And the "Key Attribute Encoding" of the ConsumeKafka processor is set to "<key attribute encoding>"
    And a RouteOnAttribute processor in the "kafka-consumer-flow" flow
    And a LogAttribute processor in the "kafka-consumer-flow" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And the "success" of the RouteOnAttribute processor is set to match <key attribute encoding> encoded kafka message key "consume_kafka_test_key"

    And the "success" relationship of the ConsumeKafka processor is connected to the LogAttribute
    And the "success" relationship of the LogAttribute processor is connected to the RouteOnAttribute
    And the "success" relationship of the RouteOnAttribute processor is connected to the PutFile

    And a kafka broker "broker" is set up in correspondence with the third-party kafka publisher

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
    And the "Topic Names" of the ConsumeKafka processor is set to "ConsumeKafkaTest"
    And the "Honor Transactions" of the ConsumeKafka processor is set to "<honor transactions>"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker "broker" is set up in correspondence with the third-party kafka publisher

    When all instances start up
    And the publisher performs a <transaction type> transaction publishing to the "ConsumeKafkaTest" topic these messages: <messages sent>

    Then <number of flowfiles expected> flowfiles are placed in the monitored directory in less than 45 seconds

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
    And the "Headers To Add As Attributes" of the ConsumeKafka processor is set to "<headers to add as attributes>"
    And the "Duplicate Header Handling" of the ConsumeKafka processor is set to "<duplicate header handling>"
    And a RouteOnAttribute processor in the "kafka-consumer-flow" flow
    And a LogAttribute processor in the "kafka-consumer-flow" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" of the RouteOnAttribute processor is set to match the attribute "<headers to add as attributes>" to "<expected value>"

    And the "success" relationship of the ConsumeKafka processor is connected to the LogAttribute
    And the "success" relationship of the LogAttribute processor is connected to the RouteOnAttribute
    And the "success" relationship of the RouteOnAttribute processor is connected to the PutFile

    And a kafka broker "broker" is set up in correspondence with the third-party kafka publisher

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
    And the "Message Demarcator" of the ConsumeKafka processor is set to "a"
    And a PutFile processor with the "Directory" property set to "/tmp/output"

    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker "broker" is set up in correspondence with the third-party kafka publisher

    When all instances start up
    And a message with content "Barbapapa" is published to the "ConsumeKafkaTest" topic
    And a message with content "Anette Tison and Talus Taylor" is published to the "ConsumeKafkaTest" topic

    Then flowfiles with these contents are placed in the monitored directory in less than 30 seconds: "B,rb,p,Anette Tison ,nd T,lus T,ylor"

  Scenario Outline: The ConsumeKafka "Maximum Poll Records" property sets a limit on the messages processed in a single batch
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And a LogAttribute processor in the "kafka-consumer-flow" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output"

    And the "Max Poll Records" of the ConsumeKafka processor is set to "<max poll records>"
    And the scheduling period of the ConsumeKafka processor is set to "<scheduling period>"
    And the scheduling period of the LogAttribute processor is set to "<scheduling period>"
    And the "FlowFiles To Log" of the LogAttribute processor is set to "<max poll records>"

    And the "success" relationship of the ConsumeKafka processor is connected to the LogAttribute
    And the "success" relationship of the LogAttribute processor is connected to the PutFile

    And a kafka broker "broker" is set up in correspondence with the third-party kafka publisher

    When all instances start up
    And 1000 kafka messages are sent to the topic "ConsumeKafkaTest"

    Then minimum <min expected messages>, maximum <max expected messages> flowfiles are produced and placed in the monitored directory in less than <polling time>

  Examples: Message batching
    | max poll records | scheduling period | polling time | min expected messages | max expected messages |
    | 3                | 10 sec            | 60 seconds   | 12                    | 24                    |
    | 6                | 10 sec            | 60 seconds   | 24                    | 48                    |

  Scenario Outline: Unsupported encoding attributes for ConsumeKafka throw scheduling errors
    Given a ConsumeKafka processor set up in a "kafka-consumer-flow" flow
    And the "<property name>" of the ConsumeKafka processor is set to "<property value>"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "kafka-consumer-flow" flow
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile

    And a kafka broker "broker" is set up in correspondence with the third-party kafka publisher

    When all instances start up
    And a message with content "<message 1>" is published to the "ConsumeKafkaTest" topic
    And a message with content "<message 2>" is published to the "ConsumeKafkaTest" topic

    Then no files are placed in the monitored directory in 45 seconds of running time

    Examples: Unsupported property values
      | message 1        | message 2      | property name           | property value |
      | Miyamoto Musashi | Eiji Yoshikawa | Key Attribute Encoding  | UTF-32         |
      | Shogun           | James Clavell  | Message Header Encoding | UTF-32         |
