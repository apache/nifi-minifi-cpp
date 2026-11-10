# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

@CORE
Feature: Minifi C++ can act as a syslog listener

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
    And PutFile's success relationship is auto-terminated

    When both instances start up
    Then at least one file in "/tmp/output" content match the following regex: "^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}.*13.*sample_log$" in less than 10 seconds

  Scenario: A syslog client can send messages to Minifi over TCP
    Given a ListenSyslog processor
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "Protocol" property of the ListenSyslog processor is set to "TCP"
    And the "Parse Messages" property of the ListenSyslog processor is set to "true"
    And a Syslog client with TCP protocol is setup to send logs to minifi
    And the "success" relationship of the ListenSyslog processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated

    When both instances start up
    Then at least 1 file is placed in the "/tmp/output" directory in less than 10 seconds
