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

Feature: Minifi C++ can act as a syslog listener

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A syslog client can send messages to Minifi over UDP
    Given a ListenSyslog processor
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "Protocol" property of the ListenSyslog processor is set to "UDP"
    And the "Parse Messages" property of the ListenSyslog processor is set to "true"
    And a Syslog client with UDP protocol is setup to send logs to minifi
    And a ReplaceText processor with the "Evaluation Mode" property set to "Entire text"
    And the "Replacement Strategy" property of the ReplaceText processor is set to "Always Replace"
    And the "Replacement Value" property of the ReplaceText processor is set to "${syslog.timestamp}	${syslog.priority}	${reverseDnsLookup(${syslog.sender}, 100)}	${syslog.msg}"
    And the "success" relationship of the ListenSyslog processor is connected to the ReplaceText
    And the "success" relationship of the ReplaceText processor is connected to the PutFile

    When both instances start up
    Then at least one flowfile's content match the following regex: "^((?:(\d{4}-\d{2}-\d{2})T(\d{2}:\d{2}:\d{2}(?:\.\d+)?))(Z|[\+-]\d{2}:\d{2})?)\t13\t.*\tsample_log$" in less than 10 seconds

  Scenario: A syslog client can send messages to Minifi over TCP
    Given a ListenSyslog processor
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "Protocol" property of the ListenSyslog processor is set to "TCP"
    And the "Parse Messages" property of the ListenSyslog processor is set to "true"
    And a Syslog client with TCP protocol is setup to send logs to minifi
    And the "success" relationship of the ListenSyslog processor is connected to the PutFile

    When both instances start up
    Then at least one flowfile is placed in the monitored directory in less than 10 seconds
