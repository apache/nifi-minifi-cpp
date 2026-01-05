# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

@ENABLE_KAFKA
Feature: Receiving data from using Kafka streaming platform using ConsumeKafka
  In order to send data to a Kafka stream
  As a user of MiNiFi
  I need to have ConsumeKafka processor

  Scenario Outline: ConsumeKafka parses and uses kafka topics and topic name formats
    Given a Kafka server is set up
    And ConsumeKafka processor is set up to communicate with that server
    And the "Topic Names" property of the ConsumeKafka processor is set to "<topic names>"
    And the "Topic Name Format" property of the ConsumeKafka processor is set to "<topic name format>"
    And the "Offset Reset" property of the ConsumeKafka processor is set to "earliest"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    And the Kafka server is started
    And the topic "ConsumeKafkaTest" is initialized on the kafka broker

    When a message with content "<message 1>" is published to the "ConsumeKafkaTest" topic
    And the MiNiFi instance starts up
    And a message with content "<message 2>" is published to the "ConsumeKafkaTest" topic

    Then files with contents "<message 1>" and "<message 2>" are placed in the "/tmp/output" directory in less than 30 seconds

    Examples: Topic names and formats to test
      | message 1            | message 2           | topic names              | topic name format |
      | Ulysses              | James Joyce         | ConsumeKafkaTest         | (not set)         |
      | The Great Gatsby     | F. Scott Fitzgerald | ConsumeKafkaTest         | Names             |
      | War and Peace        | Lev Tolstoy         | a,b,c,ConsumeKafkaTest,d | Names             |
      | Nineteen Eighty Four | George Orwell       | ConsumeKafkaTest         | Patterns          |
      | Hamlet               | William Shakespeare | Cons[emu]*KafkaTest      | Patterns          |

  Scenario Outline: ConsumeKafka key attribute is encoded according to the "Key Attribute Encoding" property
    Given a Kafka server is set up
    And ConsumeKafka processor is set up to communicate with that server
    And the "Key Attribute Encoding" property of the ConsumeKafka processor is set to "<key attribute encoding>"
    And a RouteOnAttribute processor
    And RouteOnAttribute is EVENT_DRIVEN
    And a LogAttribute processor
    And LogAttribute is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" property of the RouteOnAttribute processor is set to match <key attribute encoding> encoded kafka message key "consume_kafka_test_key"

    And the "success" relationship of the ConsumeKafka processor is connected to the LogAttribute
    And the "success" relationship of the LogAttribute processor is connected to the RouteOnAttribute
    And the "success" relationship of the RouteOnAttribute processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up
    And a message with content "<message 1>" is published to the "ConsumeKafkaTest" topic with key "consume_kafka_test_key"
    And a message with content "<message 2>" is published to the "ConsumeKafkaTest" topic with key "consume_kafka_test_key"

    Then files with contents "<message 1>" and "<message 2>" are placed in the "/tmp/output" directory in less than 30 seconds

    Examples: Key attribute encoding values
      | message 1            | message 2                     | key attribute encoding |
      | The Odyssey          | Ὅμηρος                        | (not set)              |
      | Lolita               | Владимир Владимирович Набоков | UTF-8                  |
      | Crime and Punishment | Фёдор Михайлович Достоевский  | Hex                    |

  Scenario Outline: ConsumeKafka transactional behaviour is supported
    Given a Kafka server is set up
    And ConsumeKafka processor is set up to communicate with that server
    And the "Topic Names" property of the ConsumeKafka processor is set to "ConsumeKafkaTest"
    And the "Honor Transactions" property of the ConsumeKafka processor is set to "<honor transactions>"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up
    And the publisher performs a <transaction type> transaction publishing to the "ConsumeKafkaTest" topic these messages: <messages sent>

    Then <number of flowfiles expected> files are placed in the "/tmp/output" directory in less than 15 seconds

    Examples: Transaction descriptions
      | messages sent                     | transaction type             | honor transactions | number of flowfiles expected |
      | Pride and Prejudice, Jane Austen  | SINGLE_COMMITTED_TRANSACTION | (not set)          | 2                            |
      | Dune, Frank Herbert               | TWO_SEPARATE_TRANSACTIONS    | (not set)          | 2                            |
      | The Black Sheep, Honore De Balzac | NON_COMMITTED_TRANSACTION    | (not set)          | 0                            |
      | Gospel of Thomas                  | CANCELLED_TRANSACTION        | (not set)          | 0                            |
      | Operation Dark Heart              | CANCELLED_TRANSACTION        | true               | 0                            |
      | Brexit                            | CANCELLED_TRANSACTION        | false              | 1                            |

  Scenario Outline: Headers on consumed kafka messages are extracted into attributes if requested on ConsumeKafka
    Given a Kafka server is set up
    And ConsumeKafka processor is set up to communicate with that server
    And the "Headers To Add As Attributes" property of the ConsumeKafka processor is set to "<headers to add as attributes>"
    And the "Duplicate Header Handling" property of the ConsumeKafka processor is set to "<duplicate header handling>"
    And a RouteOnAttribute processor
    And RouteOnAttribute is EVENT_DRIVEN
    And a LogAttribute processor
    And LogAttribute is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" property of the RouteOnAttribute processor is set to match the attribute "<headers to add as attributes>" to "<expected value>"

    And the "success" relationship of the ConsumeKafka processor is connected to the LogAttribute
    And the "success" relationship of the LogAttribute processor is connected to the RouteOnAttribute
    And the "success" relationship of the RouteOnAttribute processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up
    And a message with content "<message 1>" is published to the "ConsumeKafkaTest" topic with headers "<message headers sent>"
    And a message with content "<message 2>" is published to the "ConsumeKafkaTest" topic with headers "<message headers sent>"

    Then files with contents "<message 1>" and "<message 2>" are placed in the "/tmp/output" directory in less than 45 seconds

    Examples: Messages with headers
      | message 1             | message 2         | message headers sent        | headers to add as attributes | expected value       | duplicate header handling |
      | Homeland              | R. A. Salvatore   | Contains dark elves: yes    | (not set)                    | (not set)            | (not set)                 |
      | Magician              | Raymond E. Feist  | Rating: 10/10               | Rating                       | 10/10                | (not set)                 |
      | Mistborn              | Brandon Sanderson | Metal: Copper; Metal: Iron  | Metal                        | Copper               | Keep First                |
      | Mistborn              | Brandon Sanderson | Metal: Copper; Metal: Iron  | Metal                        | Iron                 | Keep Latest               |
      | Mistborn              | Brandon Sanderson | Metal: Copper; Metal: Iron  | Metal                        | Copper, Iron         | Comma-separated Merge     |
      | The Lord of the Rings | J. R. R. Tolkien  | Parts: First, second, third | Parts                        | First, second, third | (not set)                 |

  Scenario: Messages are merged if the message demarcator is present in the message
    Given a Kafka server is set up
    And ConsumeKafka processor is set up to communicate with that server
    And the "Message Demarcator" property of the ConsumeKafka processor is set to ","
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN

    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    And the Kafka server is started
    And the topic "ConsumeKafkaTest" is initialized on the kafka broker

    When the MiNiFi instance starts up
    And two messages with content "Barbapapa" and "Anette Tison and Talus Taylor" is published to the "ConsumeKafkaTest" topic

    Then a single file with the content "Barbapapa,Anette Tison and Talus Taylor" is placed in the "/tmp/output" directory in less than 45 seconds

  Scenario Outline: The ConsumeKafka "Maximum Poll Records" property sets a limit on the messages processed in a single batch
    Given a Kafka server is set up
    And ConsumeKafka processor is set up to communicate with that server
    And a LogAttribute processor
    And LogAttribute is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN

    And the "Max Poll Records" property of the ConsumeKafka processor is set to "<max poll records>"
    And the scheduling period of the ConsumeKafka processor is set to "5 sec"
    And the scheduling period of the LogAttribute processor is set to "1 sec"
    And the "FlowFiles To Log" property of the LogAttribute processor is set to "<max poll records>"

    And the "success" relationship of the ConsumeKafka processor is connected to the LogAttribute
    And the "success" relationship of the LogAttribute processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up
    And 1000 kafka messages are sent to the topic "ConsumeKafkaTest"

    Then after a wait of 15 seconds, at least <min expected messages> and at most <max expected messages> files are produced and placed in the "/tmp/output" directory

    Examples: Message batching
      | max poll records | min expected messages | max expected messages |
      | 3                | 6                     | 12                    |
      | 6                | 12                    | 24                    |

  Scenario: ConsumeKafka receives data via SSL
    Given a Kafka server is set up
    And ConsumeKafka processor is set up to communicate with that server
    And these processor properties are set
      | processor name | property name     | property value                   |
      | ConsumeKafka   | Kafka Brokers     | kafka-server-${scenario_id}:9093 |
      | ConsumeKafka   | Security Protocol | ssl                              |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And an ssl context service is set up for ConsumeKafka
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up
    And a message with content "Through the Looking-Glass" is published to the "ConsumeKafkaTest" topic
    And a message with content "Lewis Carroll" is published to the "ConsumeKafkaTest" topic

    Then files with contents "Through the Looking-Glass" and "Lewis Carroll" are placed in the "/tmp/output" directory in less than 60 seconds

  Scenario: ConsumeKafka receives data via SASL SSL
    Given a Kafka server is set up
    And ConsumeKafka processor is set up to communicate with that server
    And these processor properties are set
      | processor name | property name     | property value                   |
      | ConsumeKafka   | Kafka Brokers     | kafka-server-${scenario_id}:9095 |
      | ConsumeKafka   | Security Protocol | sasl_ssl                         |
      | ConsumeKafka   | SASL Mechanism    | PLAIN                            |
      | ConsumeKafka   | Username          | alice                            |
      | ConsumeKafka   | Password          | alice-secret                     |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And an ssl context service is set up for ConsumeKafka
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up
    And a message with content "Through the Looking-Glass" is published to the "ConsumeKafkaTest" topic
    And a message with content "Lewis Carroll" is published to the "ConsumeKafkaTest" topic

    Then files with contents "Through the Looking-Glass" and "Lewis Carroll" are placed in the "/tmp/output" directory in less than 45 seconds

  Scenario: MiNiFi consumes data from a kafka topic via SASL PLAIN connection
    Given a Kafka server is set up
    And ConsumeKafka processor is set up to communicate with that server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile
    And these processor properties are set
      | processor name | property name     | property value                   |
      | ConsumeKafka   | Kafka Brokers     | kafka-server-${scenario_id}:9094 |
      | ConsumeKafka   | Security Protocol | sasl_plaintext                   |
      | ConsumeKafka   | SASL Mechanism    | PLAIN                            |
      | ConsumeKafka   | Username          | alice                            |
      | ConsumeKafka   | Password          | alice-secret                     |

    When all instances start up
    And a message with content "some test message" is published to the "ConsumeKafkaTest" topic

    Then at least one file with the content "some test message" is placed in the "/tmp/output" directory in less than 60 seconds

  Scenario Outline: MiNiFi commit policy tests without incoming flowfiles
    Given a Kafka server is set up
    And ConsumeKafka processor is set up to communicate with that server
    And the "Topic Names" property of the ConsumeKafka processor is set to "ConsumeKafkaTest"
    And the "Commit Offsets Policy" property of the ConsumeKafka processor is set to "<commit_policy>"
    And the "Offset Reset" property of the ConsumeKafka processor is set to "<offset_reset>"
    And the "Security Protocol" property of the ConsumeKafka processor is set to "plaintext"
    And the "SASL Mechanism" property of the ConsumeKafka processor is set to "PLAIN"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    And the Kafka server is started
    And the topic "ConsumeKafkaTest" is initialized on the kafka broker

    When a message with content "Augustus" is published to the "ConsumeKafkaTest" topic
    And the MiNiFi instance starts up
    And the Kafka consumer is registered in kafka broker
    Then exactly these files are in the "/tmp/output" directory in less than 10 seconds: "<contents_after_augustus>"

    When a message with content "Tiberius" is published to the "ConsumeKafkaTest" topic
    Then exactly these files are in the "/tmp/output" directory in less than 10 seconds: "<contents_after_tiberius>"

    When MiNiFi is stopped
    And a message with content "Caligula" is published to the "ConsumeKafkaTest" topic
    Then exactly these files are in the "/tmp/output" directory in less than 10 seconds: "<contents_after_caligula>"

    When a message with content "Claudius" is published to the "ConsumeKafkaTest" topic
    Then exactly these files are in the "/tmp/output" directory in less than 10 seconds: "<contents_after_cladius>"

    When MiNiFi is restarted
    And the Kafka consumer is reregistered in kafka broker
    And a message with content "Nero" is published to the "ConsumeKafkaTest" topic
    Then exactly these files are in the "/tmp/output" directory in less than 10 seconds: "<contents_after_nero>"


    Examples: No Commit
      | commit_policy | offset_reset | contents_after_augustus | contents_after_tiberius | contents_after_caligula | contents_after_cladius | contents_after_nero                                        |
      | No Commit     | latest       |                         | Tiberius                | Tiberius                | Tiberius               | Tiberius,Nero                                              |
      | No Commit     | earliest     | Augustus                | Augustus,Tiberius       | Augustus,Tiberius       | Augustus,Tiberius      | Augustus,Tiberius,Augustus,Tiberius,Caligula,Claudius,Nero |
    Examples: Commit After Batch
      | commit_policy      | offset_reset | contents_after_augustus | contents_after_tiberius | contents_after_caligula | contents_after_cladius | contents_after_nero                      |
      | Commit After Batch | latest       |                         | Tiberius                | Tiberius                | Tiberius               | Tiberius,Caligula,Claudius,Nero          |
      | Commit After Batch | earliest     | Augustus                | Augustus,Tiberius       | Augustus,Tiberius       | Augustus,Tiberius      | Augustus,Tiberius,Caligula,Claudius,Nero |
    Examples: Commit from FlowFiles (without incoming flowfiles)
      | commit_policy                  | offset_reset | contents_after_augustus | contents_after_tiberius | contents_after_caligula | contents_after_cladius | contents_after_nero                                        |
      | Commit from incoming flowfiles | latest       |                         | Tiberius                | Tiberius                | Tiberius               | Tiberius,Nero                                              |
      | Commit from incoming flowfiles | earliest     | Augustus                | Augustus,Tiberius       | Augustus,Tiberius       | Augustus,Tiberius      | Augustus,Tiberius,Augustus,Tiberius,Caligula,Claudius,Nero |

  Scenario Outline: MiNiFi commit policy tests with incoming flowfiles
    Given a Kafka server is set up
    And ConsumeKafka processor is set up to communicate with that server
    And the "Topic Names" property of the ConsumeKafka processor is set to "ConsumeKafkaTest"
    And the "Commit Offsets Policy" property of the ConsumeKafka processor is set to "<commit_policy>"
    And the "Offset Reset" property of the ConsumeKafka processor is set to "<offset_reset>"
    And the "Security Protocol" property of the ConsumeKafka processor is set to "plaintext"
    And the "SASL Mechanism" property of the ConsumeKafka processor is set to "PLAIN"
    And a PutFile processor with the name "Consumed" and the "Directory" property set to "/tmp/output/processed"
    And a PutFile processor with the name "Committed" and the "Directory" property set to "/tmp/output/committed"
    And Consumed is EVENT_DRIVEN
    And Committed is EVENT_DRIVEN
    And the "success" relationship of the ConsumeKafka processor is connected to the Consumed
    And the "success" relationship of the Consumed processor is connected to the ConsumeKafka
    And the "committed" relationship of the ConsumeKafka processor is connected to the Committed
    And Consumed's success relationship is auto-terminated
    And Committed's success relationship is auto-terminated

    And the Kafka server is started
    And the topic "ConsumeKafkaTest" is initialized on the kafka broker

    When a message with content "Augustus" is published to the "ConsumeKafkaTest" topic
    And the MiNiFi instance starts up
    And the Kafka consumer is registered in kafka broker
    Then exactly these files are in the "/tmp/output/processed" directory in less than 15 seconds: "<contents_after_augustus>"

    When a message with content "Tiberius" is published to the "ConsumeKafkaTest" topic
    Then exactly these files are in the "/tmp/output/processed" directory in less than 15 seconds: "<contents_after_tiberius>"
    And exactly these files are in the "/tmp/output/committed" directory in less than 15 seconds: "<contents_after_tiberius>"

    When MiNiFi is stopped
    And a message with content "Caligula" is published to the "ConsumeKafkaTest" topic
    Then exactly these files are in the "/tmp/output/processed" directory in less than 15 seconds: "<contents_after_caligula>"

    When a message with content "Claudius" is published to the "ConsumeKafkaTest" topic
    Then exactly these files are in the "/tmp/output/processed" directory in less than 15 seconds: "<contents_after_cladius>"

    When MiNiFi is restarted
    And the Kafka consumer is reregistered in kafka broker
    And a message with content "Nero" is published to the "ConsumeKafkaTest" topic
    Then exactly these files are in the "/tmp/output/processed" directory in less than 15 seconds: "<contents_after_nero>"

    Examples: Commit from FlowFiles (with incoming flowfiles)
      | commit_policy                  | offset_reset | contents_after_augustus | contents_after_tiberius | contents_after_caligula | contents_after_cladius | contents_after_nero                      |
      | Commit from incoming flowfiles | latest       |                         | Tiberius                | Tiberius                | Tiberius               | Tiberius,Caligula,Claudius,Nero          |
      | Commit from incoming flowfiles | earliest     | Augustus                | Augustus,Tiberius       | Augustus,Tiberius       | Augustus,Tiberius      | Augustus,Tiberius,Caligula,Claudius,Nero |

