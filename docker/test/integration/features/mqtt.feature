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
    And a file with the content "test" is present in "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    And a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And "ConsumeMQTT" processor is a start node
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(4 bytes\)"
    And the MQTT broker has a log line matching "Received SUBSCRIBE from"

  Scenario Outline: Subscription to topics with wildcards
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
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
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*test/my/topic.*\(4 bytes\)"
    And the MQTT broker has a log line matching "Received SUBSCRIBE from"

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
    And the "Client ID" property of the ConsumeMQTT processor is set to "consumer-client"
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
    And a file with the content "<message>" is present in "/tmp/input"
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
    Then a flowfile with the content "<message>" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*<topic>"
    And the MQTT broker has a log line matching "Received SUBSCRIBE from"

    Examples: Topic wildcard patterns
    | topic                  | message     |
    | Лев Николаевич Толстой | Война и мир |
    | 孙子                    | 孫子兵法     |
    | محمد بن موسی خوارزمی   | ٱلْجَبْر       |
    | תַּלְמוּד                  | תּוֹרָה        |