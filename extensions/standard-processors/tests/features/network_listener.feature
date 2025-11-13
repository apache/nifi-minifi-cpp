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

@CORE
Feature: Minifi C++ can act as a network listener

  Scenario: A TCP client can send messages to Minifi
    Given a ListenTCP processor
    And the "Listening Port" property of the ListenTCP processor is set to "10254"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And a TCP client is set up to send a test TCP message to minifi
    And the "success" relationship of the ListenTCP processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When all instances start up
    Then at least one file with the content "test_tcp_message" is placed in the "/tmp/output" directory in less than 20 seconds
