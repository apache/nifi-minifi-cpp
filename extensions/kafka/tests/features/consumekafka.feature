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
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    And the Kafka server is started
    And the topic "ConsumeKafkaTest" is initialized on the kafka broker

    When a message with content "<message 1>" is published to the "ConsumeKafkaTest" topic
    And the MiNiFi instance starts up
    And a message with content "<message 2>" is published to the "ConsumeKafkaTest" topic

    Then the contents of "/tmp/output" in less than 30 seconds are: "<message 1>" and "<message 2>"

    Examples: Topic names and formats to test
      | message 1            | message 2           | topic names              | topic name format |
      | The Great Gatsby     | F. Scott Fitzgerald | ConsumeKafkaTest         | Names             |
      | War and Peace        | Lev Tolstoy         | a,b,c,ConsumeKafkaTest,d | Names             |
      | Nineteen Eighty Four | George Orwell       | ConsumeKafkaTest         | Patterns          |
      | Hamlet               | William Shakespeare | Cons[emu]*KafkaTest      | Patterns          |

  Scenario Outline: ConsumeKafka key attribute is encoded according to the "Key Attribute Encoding" property
    Given a Kafka server is set up
    And ConsumeKafka processor is set up to communicate with that server
    And the "Key Attribute Encoding" property of the ConsumeKafka processor is set to "<key attribute encoding>"
    And a RouteOnAttribute processor
    And a LogAttribute processor
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" property of the RouteOnAttribute processor is set to match <key attribute encoding> encoded kafka message key "consume_kafka_test_key"

    And the "success" relationship of the ConsumeKafka processor is connected to the LogAttribute
    And the "success" relationship of the LogAttribute processor is connected to the RouteOnAttribute
    And the "success" relationship of the RouteOnAttribute processor is connected to the PutFile

    When all instances start up
    And a message with content "<message 1>" is published to the "ConsumeKafkaTest" topic with key "consume_kafka_test_key"
    And a message with content "<message 2>" is published to the "ConsumeKafkaTest" topic with key "consume_kafka_test_key"

    Then the contents of "/tmp/output" in less than 30 seconds are: "<message 1>" and "<message 2>"

    Examples: Key attribute encoding values
      | message 1            | message 2                     | key attribute encoding |
      | The Odyssey          | Ὅμηρος                        | (not set)              |
      | Lolita               | Владимир Владимирович Набоков | UTF-8                  |
      | Crime and Punishment | Фёдор Михайлович Достоевский  | Hex                    |

