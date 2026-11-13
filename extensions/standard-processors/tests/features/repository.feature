# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

@CORE
Feature: Flow file and content repositories work as expected

  @ALPINE_ONLY
  Scenario: Flow file content is removed from memory when terminated when using Volatile Content Repository
    Given a GenerateFlowFile processor with the "File Size" property set to "20 MB"
    And the scheduling period of the GenerateFlowFile processor is set to "1 sec"
    And a LogAttribute processor
    And LogAttribute is EVENT_DRIVEN
    And the "success" relationship of the GenerateFlowFile processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated
    And MiNiFi configuration "nifi.content.repository.class.name" is set to "VolatileContentRepository"
    And MiNiFi configuration "nifi.flowfile.repository.class.name" is set to "NoOpRepository"

    When the MiNiFi instance starts up
    And after 5 seconds have passed

    Then MiNiFi's memory usage does not increase by more than 50 MB after 30 seconds
