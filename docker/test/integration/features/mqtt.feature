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

@ENABLE_MQTT
Feature: Sending data to MQTT streaming platform using PublishMQTT
  In order to send and receive data via MQTT
  As a user of MiNiFi
  I need to have PublishMQTT and ConsumeMQTT processors

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario Outline: A MiNiFi instance transfers data to an MQTT broker
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the PublishMQTT processor is set to "<version>"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT
    And the "success" relationship of the PublishMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(4 bytes\)"

    Examples: MQTT versions
    | version  |
    | 3.x AUTO |
    | 5.0      |

  Scenario Outline: If the MQTT broker does not exist, then no flow files are processed
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the PublishMQTT processor is set to "<version>"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT
    And the "success" relationship of the PublishMQTT processor is connected to the PutFile
    And the "failure" relationship of the PublishMQTT processor is connected to the PutFile

    When the MiNiFi instance starts up
    Then no files are placed in the monitored directory in 30 seconds of running time

    Examples: MQTT versions
    | version  |
    | 3.x AUTO |
    | 5.0      |

  Scenario Outline: Verify delivery of message when MQTT broker is unstable
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the PublishMQTT processor is set to "<version>"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT
    And the "success" relationship of the PublishMQTT processor is connected to the PutFile

    When the MiNiFi instance starts up
    Then no files are placed in the monitored directory in 10 seconds of running time

    When an MQTT broker is set up in correspondence with the PublishMQTT
    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(4 bytes\)"

    Examples: MQTT versions
    | version  |
    | 3.x AUTO |
    | 5.0      |

  Scenario Outline: A MiNiFi instance publishes and consumes data to/from an MQTT broker
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the PublishMQTT processor is set to "<version>"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    And a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "<version>"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And a LogAttribute processor
    And "ConsumeMQTT" processor is a start node
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When both instances start up
    Then the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And the MQTT broker has a log line matching "New client connected from .* as publisher-client"
    And a file with the content "test" is placed in "/tmp/input"
    And a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(4 bytes\)"
    And the Minifi logs contain the following message: "key:mqtt.broker value:mqtt-broker-" in less than 60 seconds
    And the Minifi logs contain the following message: "key:mqtt.topic value:testtopic" in less than 1 seconds
    And the Minifi logs contain the following message: "key:mqtt.topic.segment.0 value:testtopic" in less than 1 seconds
    And the Minifi logs contain the following message: "key:mqtt.qos value:0" in less than 1 seconds
    And the Minifi logs contain the following message: "key:mqtt.isDuplicate value:false" in less than 1 seconds
    And the Minifi logs contain the following message: "key:mqtt.isRetained value:false" in less than 1 seconds

    Examples: MQTT versions
    | version  |
    | 3.x AUTO |
    | 3.1.0    |
    | 3.1.1    |
    | 5.0      |

  Scenario Outline: Subscription to topics with wildcards
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "Topic" property of the PublishMQTT processor is set to "test/my/topic"
    And the "MQTT Version" property of the PublishMQTT processor is set to "<version>"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    And a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And the "Topic" property of the ConsumeMQTT processor is set to "<topic wildcard pattern>"
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "<version>"
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
    | topic wildcard pattern | version  |
    | test/#                 | 3.x AUTO |
    | test/+/topic           | 3.x AUTO |
    | test/#                 | 5.0      |
    | test/+/topic           | 5.0      |

  Scenario Outline: Subscription and publishing with disconnecting clients in persistent sessions
    # publishing MQTT client
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "publisher-client" flow
    And a PublishMQTT processor in the "publisher-client" flow
    And the "MQTT Version" property of the PublishMQTT processor is set to "<version>"
    And the "Quality of Service" property of the PublishMQTT processor is set to "1"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    # consuming MQTT client
    And a ConsumeMQTT processor in the "consumer-client" flow
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "<version>"
    And the "Quality of Service" property of the ConsumeMQTT processor is set to "1"
    And the "<persistent_session_property_1>" property of the ConsumeMQTT processor is set to "<persistent_session_property_1_value>"
    And the "<persistent_session_property_2>" property of the ConsumeMQTT processor is set to "<persistent_session_property_2_value>"
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

    Examples: MQTT versions
    | version  | persistent_session_property_1 | persistent_session_property_1_value | persistent_session_property_2 | persistent_session_property_2_value |
    | 3.x AUTO | Clean Session                 |  false                              | Clean Session                 | false                               |
    | 5.0      | Clean Start                   |  false                              | Session Expiry Interval       | 1 h                                 |

  Scenario Outline: UTF-8 topics and messages
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the PublishMQTT processor is set to "<version>"
    And the "Topic" property of the PublishMQTT processor is set to "<topic>"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    And a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "<version>"
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

    Examples: Topics, messages and version
    | topic                  | message     | version  |
    | Лев Николаевич Толстой | Война и мир | 3.x AUTO |
    | 孙子                    | 孫子兵法     | 3.x AUTO |
    | 孙子                    | 孫子兵法     | 5.0      |
    | محمد بن موسی خوارزمی   | ٱلْجَبْر       | 3.x AUTO |
    | תַּלְמוּד                  | תּוֹרָה        | 3.x AUTO |


  Scenario Outline: QoS 0 message flow is correct
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the PublishMQTT processor is set to "<version>"
    And the "Quality of Service" property of the PublishMQTT processor is set to "0"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    And a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "<version>"
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

    Examples: MQTT versions
    | version  |
    | 3.x AUTO |
    | 5.0      |

  Scenario Outline: QoS 1 Subscriber sends PUBACK on a PUBLISH message, with correct packet ID
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the PublishMQTT processor is set to "<version>"
    And the "Quality of Service" property of the PublishMQTT processor is set to "1"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    And a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "<version>"
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

    Examples: MQTT versions
    | version  |
    | 3.x AUTO |
    | 5.0      |

  Scenario Outline: QoS 2 message flow is correct
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the PublishMQTT processor is set to "<version>"
    And the "Quality of Service" property of the PublishMQTT processor is set to "2"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    And a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "<version>"
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

    Examples: MQTT versions
    | version  |
    | 3.x AUTO |
    | 5.0      |

  Scenario Outline: Retained message
    # publishing MQTT client
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "publisher-client" flow
    And a file with the content "test" is present in "/tmp/input"
    And a PublishMQTT processor in the "publisher-client" flow
    And the "MQTT Version" property of the PublishMQTT processor is set to "<version>"
    And the "Retain" property of the PublishMQTT processor is set to "true"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    # consuming MQTT client
    And a ConsumeMQTT processor in the "consumer-client" flow
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "<version>"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "consumer-client" flow
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When both instances start up
    Then the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(4 bytes\)"

    # consumer is joining late, but it will still see the retained message
    And "consumer-client" flow is started
    And the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"

    And a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds

    Examples: MQTT versions
    | version  |
    | 3.x AUTO |
    | 5.0      |

  Scenario Outline: Last will
    # publishing MQTT client with last will
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "publisher-client" flow
    And a PublishMQTT processor in the "publisher-client" flow
    And the "MQTT Version" property of the PublishMQTT processor is set to "<version>"
    And the "Last Will Topic" property of the PublishMQTT processor is set to "last_will_topic"
    And the "Last Will Message" property of the PublishMQTT processor is set to "last_will_message"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    # consuming MQTT client set to consume last will topic
    And a ConsumeMQTT processor in the "consumer-client" flow
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "<version>"
    And the "Topic" property of the ConsumeMQTT processor is set to "last_will_topic"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "consumer-client" flow
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When all instances start up
    Then the MQTT broker has a log line matching "Sending CONNACK to publisher-client"
    And the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And "publisher-client" flow is killed
    And the MQTT broker has a log line matching "Sending PUBLISH to consumer-client"
    And a flowfile with the content "last_will_message" is placed in the monitored directory in less than 60 seconds

    Examples: MQTT versions
    | version  |
    | 3.x AUTO |
    | 5.0      |

  Scenario Outline: Keep Alive
    Given a ConsumeMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "<version>"
    And the "Keep Alive Interval" property of the ConsumeMQTT processor is set to "1 sec"

    And an MQTT broker is set up in correspondence with the ConsumeMQTT

    When both instances start up
    Then the MQTT broker has a log line matching "Received PINGREQ from consumer-client"
    Then the MQTT broker has a log line matching "Sending PINGRESP to consumer-client"

    Examples: MQTT versions
    | version  |
    | 3.x AUTO |
    | 5.0      |

  Scenario Outline: Message Expiry Interval - MQTT 5
    # publishing MQTT client
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "publisher-client" flow
    And a PublishMQTT processor in the "publisher-client" flow
    And the "MQTT Version" property of the PublishMQTT processor is set to "5.0"
    And the "Message Expiry Interval" property of the PublishMQTT processor is set to "<message_expiry_interval>"
    And the "Quality of Service" property of the PublishMQTT processor is set to "1"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    # consuming MQTT client
    And a ConsumeMQTT processor in the "consumer-client" flow
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "5.0"
    And the "Quality of Service" property of the ConsumeMQTT processor is set to "1"
    And the "Clean Start" property of the ConsumeMQTT processor is set to "false"
    And the "Session Expiry Interval" property of the ConsumeMQTT processor is set to "1 h"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "consumer-client" flow
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When all instances start up
    Then the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And "consumer-client" flow is stopped
    And the MQTT broker has a log line matching "Received DISCONNECT from consumer-client"

    And a file with the content "test" is placed in "/tmp/input"
    And the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(4 bytes\)"

    And 2 seconds later
    And "consumer-client" flow is restarted
    And the MQTT broker has 2 log lines matching "New client connected from .* as consumer-client"
    And <expectation_num_files> placed in the monitored directory in <expectation_time_limit>

    Examples: Message Expiry Intervals
    | message_expiry_interval | expectation_num_files                  | expectation_time_limit     |
    | 1 h                     | a flowfile with the content "test" is  | less than 60 seconds       |
    | 1 s                     | no files are                           | 30 seconds of running time |

  Scenario: User properties - MQTT 5
    # publishing MQTT client: GetFile -> UpdateAttribute (my_attr1:true, my_attr2:true) -> PublishMQTT
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "publisher-client" flow
    And a UpdateAttribute processor in the "publisher-client" flow
    And the "my_attr1" property of the UpdateAttribute processor is set to "true"
    And the "my_attr2" property of the UpdateAttribute processor is set to "true"
    And a PublishMQTT processor in the "publisher-client" flow
    And the "MQTT Version" property of the PublishMQTT processor is set to "5.0"
    And the "success" relationship of the GetFile processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the PublishMQTT

    # consuming MQTT client: ConsumeMQTT -> RouteOnAttribute (my_attr1) -> RouteOnAttribute (my_attr2) -> PutFile
    And a ConsumeMQTT processor in the "consumer-client" flow
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "5.0"
    And a RouteOnAttribute processor with the name "RouteAttr1" in the "consumer-client" flow
    And the "matched_my_attr1" property of the RouteAttr1 processor is set to "${my_attr1}"
    And a RouteOnAttribute processor with the name "RouteAttr2" in the "consumer-client" flow
    And the "matched_my_attr2" property of the RouteAttr2 processor is set to "${my_attr2}"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "consumer-client" flow
    And the "success" relationship of the ConsumeMQTT processor is connected to the RouteAttr1
    And the "matched_my_attr1" relationship of the RouteAttr1 processor is connected to the RouteAttr2
    And the "matched_my_attr2" relationship of the RouteAttr2 processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When both instances start up
    Then the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And the MQTT broker has a log line matching "New client connected from .* as publisher-client"
    And a file with the content "test" is placed in "/tmp/input"
    And a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(4 bytes\)"

  Scenario: Content type - MQTT 5
    # publishing MQTT client: GetFile -> PublishMQTT (Content Type: text/plain)
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "publisher-client" flow
    And a PublishMQTT processor in the "publisher-client" flow
    And the "MQTT Version" property of the PublishMQTT processor is set to "5.0"
    And the "Content Type" property of the PublishMQTT processor is set to "text/plain"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    # consuming MQTT client: ConsumeMQTT (Attribute From Content Type: content_type) -> RouteOnAttribute (content_type = text/plain) -> PutFile
    And a ConsumeMQTT processor in the "consumer-client" flow
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "5.0"
    And the "Attribute From Content Type" property of the ConsumeMQTT processor is set to "content_type"
    And a RouteOnAttribute processor in the "consumer-client" flow
    And the "matched_content_type" property of the RouteOnAttribute processor is set to "${content_type:equals('text/plain')}"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "consumer-client" flow
    And the "success" relationship of the ConsumeMQTT processor is connected to the RouteOnAttribute
    And the "matched_content_type" relationship of the RouteOnAttribute processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When both instances start up
    Then the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And the MQTT broker has a log line matching "New client connected from .* as publisher-client"
    And a file with the content "test" is placed in "/tmp/input"
    And a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(4 bytes\)"

  Scenario: Will content type - MQTT 5
    # publishing MQTT client: GetFile -> PublishMQTT (Last Will Content Type: text/plain)
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input" in the "publisher-client" flow
    And a PublishMQTT processor in the "publisher-client" flow
    And the "MQTT Version" property of the PublishMQTT processor is set to "5.0"
    And the "Last Will Topic" property of the PublishMQTT processor is set to "last_will_topic"
    And the "Last Will Message" property of the PublishMQTT processor is set to "last_will_message"
    And the "Last Will Content Type" property of the PublishMQTT processor is set to "text/plain"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT

    # consuming MQTT client: ConsumeMQTT (Attribute From Content Type: content_type, Topic: last_will_topic) -> RouteOnAttribute (content_type = text/plain) -> PutFile
    And a ConsumeMQTT processor in the "consumer-client" flow
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "5.0"
    And the "Topic" property of the ConsumeMQTT processor is set to "last_will_topic"
    And the "Attribute From Content Type" property of the ConsumeMQTT processor is set to "content_type"
    And a RouteOnAttribute processor in the "consumer-client" flow
    And the "matched_content_type" property of the RouteOnAttribute processor is set to "${content_type:equals('text/plain')}"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "consumer-client" flow
    And the "success" relationship of the ConsumeMQTT processor is connected to the RouteOnAttribute
    And the "matched_content_type" relationship of the RouteOnAttribute processor is connected to the PutFile

    And an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT

    When all instances start up
    Then the MQTT broker has a log line matching "Sending CONNACK to publisher-client"
    And the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And "publisher-client" flow is killed
    And the MQTT broker has a log line matching "Sending PUBLISH to consumer-client"
    And a flowfile with the content "last_will_message" is placed in the monitored directory in less than 60 seconds

  Scenario: A MiNiFi instance uses record reader and writer to convert consumed message from an MQTT broker
    Given a XMLReader controller service is set up
    And a JsonRecordSetWriter controller service is set up with "Array" output grouping
    And a ConsumeMQTT processor with the "Topic" property set to "test/my/topic"
    And the "MQTT Version" property of the ConsumeMQTT processor is set to "3.x AUTO"
    And the "Record Reader" property of the ConsumeMQTT processor is set to "XMLReader"
    And the "Record Writer" property of the ConsumeMQTT processor is set to "JsonRecordSetWriter"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And a LogAttribute processor
    And the "success" relationship of the ConsumeMQTT processor is connected to the PutFile
    And the "success" relationship of the PutFile processor is connected to the LogAttribute
    And an MQTT broker is set up in correspondence with the ConsumeMQTT

    When both instances start up
    And a test message "<root><element>test</element></root>" is published to the MQTT broker on topic "test/my/topic"

    Then the MQTT broker has a log line matching "Received SUBSCRIBE from consumer-client"
    And a flowfile with the JSON content '[{"_isRetained": false, "_isDuplicate": false, "_qos": 0, "_topicSegments": ["test", "my", "topic"], "_topic": "test/my/topic", "element": "test"}]' is placed in the monitored directory in less than 60 seconds
    And the Minifi logs contain the following message: "key:record.count value:1" in less than 60 seconds
    And the Minifi logs contain the following message: "key:mqtt.broker value:mqtt-broker-" in less than 1 seconds

  Scenario: A MiNiFi instance uses record reader and writer to convert and publish records to an MQTT broker
    Given a JsonTreeReader controller service is set up
    And a XMLRecordSetWriter controller service is set up
    And the "Name of Record Tag" property of the XMLRecordSetWriter controller is set to "record"
    And the "Name of Root Tag" property of the XMLRecordSetWriter controller is set to "root"
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content '[{"string": "test"}, {"int": 42}]' is present in '/tmp/input'
    And a PublishMQTT processor set up to communicate with an MQTT broker instance
    And the "MQTT Version" property of the PublishMQTT processor is set to "3.x AUTO"
    And the "Record Reader" property of the PublishMQTT processor is set to "JsonTreeReader"
    And the "Record Writer" property of the PublishMQTT processor is set to "XMLRecordSetWriter"
    And a UpdateAttribute processor with the "filename" property set to "${UUID()}.xml"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PublishMQTT
    And the "success" relationship of the PublishMQTT processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the PutFile
    And an MQTT broker is set up in correspondence with the PublishMQTT

    When both instances start up

    Then two flowfiles with the contents '<?xml version="1.0"?><root><record><string>test</string></record></root>' and '<?xml version="1.0"?><root><record><int>42</int></record></root>' are placed in the monitored directory in less than 60 seconds
    And the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(72 bytes\)"
    And the MQTT broker has a log line matching "Received PUBLISH from .*testtopic.*\(64 bytes\)"
