# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
