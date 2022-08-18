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

Feature: Sending data to MQTT streaming platform using PublishMQTT
  In order to send and receive data via MQTT
  As a user of MiNiFi
  I need to have PublishMQTT and ConsumeMQTT processors

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance transfers data to an MQTT broker
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT
    And the "success" relationship of the PublishMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(4 bytes\)"

  Scenario: If the MQTT broker does not exist, then no flow files are processed
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT
    And the "success" relationship of the PublishMQTT processor is connected to the PutFile
    And the "failure" relationship of the PublishMQTT processor is connected to the PutFile

    When the MiNiFi instance starts up
    Then no files are placed in the monitored directory in 30 seconds of running time

  Scenario: Verify delivery of message when MQTT broker is unstable
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT
    And the "success" relationship of the PublishMQTT processor is connected to the PutFile

    When the MiNiFi instance starts up
    Then no files are placed in the monitored directory in 30 seconds of running time

    And an MQTT broker is deployed in correspondence with the PublishMQTT
    And a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(4 bytes\)"

  Scenario: A MiNiFi instance publishes and consumes data to/from an MQTT broker
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    And a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And "ConsumeMQTT" processor is a start node
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When both instances start up
    Then the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And the MQTT broker has a log line matching "New client connected from .* as publisher-client"
    And a file with the content "test" is placed in "/tmp/input"
    And a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(4 bytes\)"

  Scenario Outline: Subscription to topics with wildcards
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "Topic" property of the PublishMQTT processor is set to "test/my/topic"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    And a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And the "Topic" property of the ConsumeMQTT processor is set to "<topic wildcard pattern>"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And "ConsumeMQTT" processor is a start node
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When both instances start up
    Then the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And the MQTT broker has a log line matching "New client connected from .* as publisher-client"
    And a file with the content "test" is placed in "/tmp/input"
    And a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*test/my/topic.*\(4 bytes\)"

    Examples: Topic wildcard patterns
    | topic wildcard pattern |
    | test/#                 |
    | test/+/topic           |

  Scenario: Subscription and publishing with disconnecting clients in persistent sessions
    # publishing MQTT client
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "publisher-client" flow
    And a PublishMQTT processor in the "publisher-client" flow
    And the "Quality of Service" property of the PublishMQTT processor is set to "1"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    # consuming MQTT client
    And a ConsumeMQTT processor in the "consumer-client" flow
    And the "Quality of Service" property of the ConsumeMQTT processor is set to "1"
    And the "Clean Session" property of the ConsumeMQTT processor is set to "false"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "consumer-client" flow
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When all instances start up
    Then the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And "consumer-client" flow is stopped
    And the MQTT broker has a log line matching "Received DISCONNECT from consumer-client"

    And a file with the content "test" is placed in "/tmp/input"
    And the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(4 bytes\)"

    And "consumer-client" flow is restarted
    And the MQTT broker has 2 log lines matching "New client connected from .* as consumer-client"
    And a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds

  Scenario Outline: UTF-8 topics and messages
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "Topic" property of the PublishMQTT processor is set to "<topic>"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    And a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And the "Topic" property of the ConsumeMQTT processor is set to "<topic>"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And "ConsumeMQTT" processor is a start node
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When both instances start up
    Then the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And the MQTT broker has a log line matching "New client connected from .* as publisher-client"
    And a file with the content "<message>" is placed in "/tmp/input"
    And a flowfile with the content "<message>" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*<topic>"

    Examples: Topic wildcard patterns
    | topic                  | message     |
    | Лев Николаевич Толстой | Война и мир |
    | 孙子                    | 孫子兵法     |
    | محمد بن موسی خوارزمی   | ٱلْجَبْر       |
    | תַּלְמוּד                  | תּוֹרָה        |


  Scenario: QoS 0 message flow is correct
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "Quality of Service" property of the PublishMQTT processor is set to "0"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    And a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And the "Quality of Service" property of the ConsumeMQTT processor is set to "0"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And "ConsumeMQTT" processor is a start node
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When both instances start up
    Then the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And the MQTT broker has a log line matching "New client connected from .* as publisher-client"
    And a file with the content "test" is placed in "/tmp/input"
    And a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from publisher-client \(d0, q0, r0, m0, 'testtopic'.*\(4 bytes\)"
    And the MQTT broker has a log line matching "Sending PUBLISH to consumer-client \(d0, q0, r0, m0, 'testtopic',.*\(4 bytes\)\)"

  Scenario: QoS 1 Subscriber sends PUBACK on a PUBLISH message, with correct packet ID
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "Quality of Service" property of the PublishMQTT processor is set to "1"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    And a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And the "Quality of Service" property of the ConsumeMQTT processor is set to "1"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And "ConsumeMQTT" processor is a start node
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When both instances start up
    Then the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And the MQTT broker has a log line matching "New client connected from .* as publisher-client"
    And a file with the content "test" is placed in "/tmp/input"
    And a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from publisher-client.*m1, 'testtopic'.*\(4 bytes\)"
    And the MQTT broker has a log line matching "Sending PUBACK to publisher-client \(m1, rc0\)"
    And the MQTT broker has a log line matching "Sending PUBLISH to consumer-client \(d0, q1, r0, m1, 'testtopic',.*\(4 bytes\)\)"
    And the MQTT broker has a log line matching "Received PUBACK from consumer-client \(Mid: 1, RC:0\)"


  Scenario: QoS 2 message flow is correct
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "Quality of Service" property of the PublishMQTT processor is set to "2"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    And a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And the "Quality of Service" property of the ConsumeMQTT processor is set to "2"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And "ConsumeMQTT" processor is a start node
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When both instances start up
    Then the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And the MQTT broker has a log line matching "New client connected from .* as publisher-client"
    And a file with the content "test" is placed in "/tmp/input"
    And a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from publisher-client.*m1, 'testtopic'.*\(4 bytes\)"
    And the MQTT broker has a log line matching "Sending PUBREC to publisher-client \(m1, rc0\)"
    And the MQTT broker has a log line matching "Received PUBREL from publisher-client \(Mid: 1\)"
    And the MQTT broker has a log line matching "Sending PUBLISH to consumer-client \(d0, q2, r0, m1, 'testtopic',.*\(4 bytes\)\)"
    And the MQTT broker has a log line matching "Sending PUBCOMP to publisher-client \(m1\)"
    And the MQTT broker has a log line matching "Received PUBREC from consumer-client \(Mid: 1\)"
    And the MQTT broker has a log line matching "Sending PUBREL to consumer-client \(m1\)"
    And the MQTT broker has a log line matching "Received PUBCOMP from consumer-client \(Mid: 1, RC:0\)"

  Scenario: Retained message
    # publishing MQTT client
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "publisher-client" flow
    And a file with the content "test" is present in "/tmp/input"
    And a PublishMQTT processor in the "publisher-client" flow
    And the "Retain" property of the PublishMQTT processor is set to "true"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    # consuming MQTT client
    And a ConsumeMQTT processor in the "consumer-client" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "consumer-client" flow
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When the MQTT broker is started
    And "publisher-client" flow is started
    Then the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(4 bytes\)"

    # consumer is joining late, but it will still see the retained message
    And "consumer-client" flow is started
    And the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"

    And a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds

  Scenario: Last will
    # publishing MQTT client with last will
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "publisher-client" flow
    And a PublishMQTT processor in the "publisher-client" flow
    And the "Last Will Topic" property of the PublishMQTT processor is set to "last_will_topic"
    And the "Last Will Message" property of the PublishMQTT processor is set to "last_will_message"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    # consuming MQTT client set to consume last will topic
    And a ConsumeMQTT processor in the "consumer-client" flow
    And the "Topic" property of the ConsumeMQTT processor is set to "last_will_topic"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "consumer-client" flow
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When all instances start up
    Then the MQTT broker has a log line matching "Sending CONNACK to publisher-client"
    Then the MQTT broker has a log line matching "Sending CONNACK to consumer-client"
    And "publisher-client" flow is killed
    And the MQTT broker has a log line matching "Sending PUBLISH to consumer-client"
    And a flowfile with the content "last_will_message" is placed in the monitored directory in less than 60 seconds

  Scenario: Keep Alive
    Given a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And the "Keep Alive Interval" property of the ConsumeMQTT processor is set to "1 sec"

    And an MQTT broker is set up in correspondence with the ConsumeMQTT

    When both instances start up
    Then the MQTT broker has a log line matching "Received PINGREQ from consumer-client"
    Then the MQTT broker has a log line matching "Sending PINGRESP to consumer-client"
