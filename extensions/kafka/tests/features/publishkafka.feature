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
Feature: Sending data to using Kafka streaming platform using PublishKafka
  In order to send data to a Kafka stream
  As a user of MiNiFi
  I need to have PublishKafka processor

  Scenario Outline: A MiNiFi instance transfers data to a kafka broker
    Given a Kafka server is set up
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And a UpdateAttribute processor
    And UpdateAttribute is EVENT_DRIVEN
    And these processor properties are set
      | processor name  | property name          | property value         |
      | UpdateAttribute | kafka_require_num_acks | 1                      |
      | UpdateAttribute | kafka_message_key      | unique_message_key_123 |
    And a PublishKafka processor
    And PublishKafka is EVENT_DRIVEN
    And these processor properties are set
      | processor name | property name      | property value                                        |
      | PublishKafka   | Topic Name         | test                                                  |
      | PublishKafka   | Delivery Guarantee | ${kafka_require_num_acks}                             |
      | PublishKafka   | Request Timeout    | 12 s                                                  |
      | PublishKafka   | Message Timeout    | 13 s                                                  |
      | PublishKafka   | Known Brokers      | kafka-server-${scenario_id}:${literal(9000):plus(92)} |
      | PublishKafka   | Client Name        | client_no_${literal(6):multiply(7)}                   |
      | PublishKafka   | Kafka Key          | ${kafka_message_key}                                  |
      | PublishKafka   | retry.backoff.ms   | ${literal(2):multiply(25):multiply(3)}                |
      | PublishKafka   | Message Key Field  | kafka.key                                             |
      | PublishKafka   | Compress Codec     | <compress_codec>                                      |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile
    And the "failure" relationship of the PublishKafka processor is connected to the PublishKafka
    And PutFile's success relationship is auto-terminated

    When both instances start up
    Then there is a single file with "test" content in the "/tmp/output" directory in less than 60 seconds
    And the Minifi logs contain the following message: " is 'test'" in less than 60 seconds
    And the Minifi logs contain the following message: "PublishKafka: request.required.acks [1]" in less than 10 seconds
    And the Minifi logs contain the following message: "PublishKafka: request.timeout.ms [12000]" in less than 10 seconds
    And the Minifi logs contain the following message: "PublishKafka: message.timeout.ms [13000]" in less than 10 seconds
    And the Minifi logs contain the following message: "PublishKafka: bootstrap.servers [kafka-server-${scenario_id}:9092]" in less than 10 seconds
    And the Minifi logs contain the following message: "PublishKafka: client.id [client_no_42]" in less than 10 seconds
    And the Minifi logs contain the following message: "PublishKafka: Message Key [unique_message_key_123]" in less than 10 seconds
    And the Minifi logs contain the following message: "PublishKafka: DynamicProperty: [retry.backoff.ms] -> [150]" in less than 10 seconds
    And the Minifi logs contain the following message: "The Message Key Field property is set. This property is DEPRECATED and has no effect; please use Kafka Key instead." in less than 10 seconds

    Examples: Compression formats
      | compress_codec |
      | none           |
      | snappy         |
      | gzip           |
      | lz4            |
      | zstd           |

  Scenario: PublishKafka sends flowfiles to failure when the broker is not available
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "no broker"
    And PublishKafka processor is set up to communicate with that server
    And PublishKafka is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "failure" relationship of the PublishKafka processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When the MiNiFi instance starts up
    Then there is a single file with "no broker" content in the "/tmp/output" directory in less than 60 seconds

  Scenario: PublishKafka sends can use SSL connect with security properties
    Given a Kafka server is set up
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And PublishKafka processor is set up to communicate with that server
    And these processor properties are set
      | processor name | property name        | property value                    |
      | PublishKafka   | Client Name          | LMN                               |
      | PublishKafka   | Known Brokers        | kafka-server-${scenario_id}:9093  |
      | PublishKafka   | Topic Name           | test                              |
      | PublishKafka   | Batch Size           | 10                                |
      | PublishKafka   | Compress Codec       | none                              |
      | PublishKafka   | Delivery Guarantee   | 1                                 |
      | PublishKafka   | Request Timeout      | 10 sec                            |
      | PublishKafka   | Message Timeout      | 12 sec                            |
      | PublishKafka   | Security CA          | /tmp/resources/root_ca.crt        |
      | PublishKafka   | Security Cert        | /tmp/resources/minifi_client.crt  |
      | PublishKafka   | Security Private Key | /tmp/resources/minifi_client.key  |
      | PublishKafka   | Security Protocol    | ssl                               |
    And PublishKafka is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile
    And the "failure" relationship of the PublishKafka processor is connected to the PublishKafka
    And PutFile's success relationship is auto-terminated

    When both instances start up
    Then there is a single file with "test" content in the "/tmp/output" directory in less than 60 seconds

    # We fallback to the flowfile's uuid as message key if the Kafka Key property is not set
    And the Minifi logs match the following regex: "PublishKafka: Message Key \[[a-z0-9]{8}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{12}\]" in less than 10 seconds

  Scenario: A MiNiFi instance transfers data to a kafka broker through SASL Plain security protocol
    Given a Kafka server is set up
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And PublishKafka processor is set up to communicate with that server
    And these processor properties are set
      | processor name | property name     | property value                   |
      | PublishKafka   | Topic Name        | test                             |
      | PublishKafka   | Request Timeout   | 10 sec                           |
      | PublishKafka   | Message Timeout   | 12 sec                           |
      | PublishKafka   | Known Brokers     | kafka-server-${scenario_id}:9094 |
      | PublishKafka   | Client Name       | LMN                              |
      | PublishKafka   | Security Protocol | sasl_plaintext                   |
      | PublishKafka   | SASL Mechanism    | PLAIN                            |
      | PublishKafka   | Username          | alice                            |
      | PublishKafka   | Password          | alice-secret                     |
    And PublishKafka is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile
    And the "failure" relationship of the PublishKafka processor is connected to the PublishKafka
    And PutFile's success relationship is auto-terminated

    When both instances start up
    Then there is a single file with "test" content in the "/tmp/output" directory in less than 60 seconds

  Scenario: PublishKafka sends can use SASL SSL connect with security properties
    Given a Kafka server is set up
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And PublishKafka processor is set up to communicate with that server
    And these processor properties are set
      | processor name | property name        | property value                    |
      | PublishKafka   | Client Name          | LMN                               |
      | PublishKafka   | Known Brokers        | kafka-server-${scenario_id}:9095  |
      | PublishKafka   | Topic Name           | test                              |
      | PublishKafka   | Batch Size           | 10                                |
      | PublishKafka   | Compress Codec       | none                              |
      | PublishKafka   | Delivery Guarantee   | 1                                 |
      | PublishKafka   | Request Timeout      | 10 sec                            |
      | PublishKafka   | Message Timeout      | 12 sec                            |
      | PublishKafka   | Security CA          | /tmp/resources/root_ca.crt        |
      | PublishKafka   | Security Cert        | /tmp/resources/minifi_client.crt  |
      | PublishKafka   | Security Private Key | /tmp/resources/minifi_client.key  |
      | PublishKafka   | Security Protocol    | sasl_ssl                          |
      | PublishKafka   | SASL Mechanism       | PLAIN                             |
      | PublishKafka   | Username             | alice                             |
      | PublishKafka   | Password             | alice-secret                      |
    And PublishKafka is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile
    And the "failure" relationship of the PublishKafka processor is connected to the PublishKafka
    And PutFile's success relationship is auto-terminated

    When both instances start up
    Then there is a single file with "test" content in the "/tmp/output" directory in less than 60 seconds

  Scenario: PublishKafka sends can use SASL SSL connect with SSL Context
    Given a Kafka server is set up
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And PublishKafka processor is set up to communicate with that server
    And these processor properties are set
      | processor name | property name      | property value                   |
      | PublishKafka   | Client Name        | LMN                              |
      | PublishKafka   | Known Brokers      | kafka-server-${scenario_id}:9095 |
      | PublishKafka   | Topic Name         | test                             |
      | PublishKafka   | Batch Size         | 10                               |
      | PublishKafka   | Compress Codec     | none                             |
      | PublishKafka   | Delivery Guarantee | 1                                |
      | PublishKafka   | Request Timeout    | 10 sec                           |
      | PublishKafka   | Message Timeout    | 12 sec                           |
      | PublishKafka   | Security Protocol  | sasl_ssl                         |
      | PublishKafka   | SASL Mechanism     | PLAIN                            |
      | PublishKafka   | Username           | alice                            |
      | PublishKafka   | Password           | alice-secret                     |
    And PublishKafka is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And an ssl context service is set up for PublishKafka
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile
    And the "failure" relationship of the PublishKafka processor is connected to the PublishKafka
    And PutFile's success relationship is auto-terminated

    When both instances start up
    Then there is a single file with "test" content in the "/tmp/output" directory in less than 60 seconds

  Scenario: PublishKafka sends can use SSL connect with SSL Context Service
    Given a Kafka server is set up
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And PublishKafka processor is set up to communicate with that server
    And these processor properties are set
      | processor name | property name      | property value                   |
      | PublishKafka   | Client Name        | LMN                              |
      | PublishKafka   | Known Brokers      | kafka-server-${scenario_id}:9093 |
      | PublishKafka   | Topic Name         | test                             |
      | PublishKafka   | Batch Size         | 10                               |
      | PublishKafka   | Compress Codec     | none                             |
      | PublishKafka   | Delivery Guarantee | 1                                |
      | PublishKafka   | Request Timeout    | 10 sec                           |
      | PublishKafka   | Message Timeout    | 12 sec                           |
      | PublishKafka   | Security Protocol  | ssl                              |
    And PublishKafka is EVENT_DRIVEN
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And an ssl context service is set up for PublishKafka
    And the "success" relationship of the GetFile processor is connected to the PublishKafka
    And the "success" relationship of the PublishKafka processor is connected to the PutFile
    And the "failure" relationship of the PublishKafka processor is connected to the PublishKafka
    And PutFile's success relationship is auto-terminated

    When both instances start up
    Then there is a single file with "test" content in the "/tmp/output" directory in less than 60 seconds

    # We fallback to the flowfile's uuid as message key if the Kafka Key property is not set
    And the Minifi logs match the following regex: "PublishKafka: Message Key \[[a-z0-9]{8}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{12}\]" in less than 10 seconds
