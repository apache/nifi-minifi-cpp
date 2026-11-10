# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

@SUPPORTS_WINDOWS
Feature: minifi_tensor extension loads

  @MINIFI_EXTENSION_TENSOR
  Scenario: All high-level processors and the TractModelService register cleanly
    Given log property "logger.org::apache::nifi::minifi::core::extension::ExtensionManager" is set to "TRACE,stderr"
    And log property "logger.org::apache::nifi::minifi::core::ClassLoader" is set to "TRACE,stderr"

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "Registering class 'ClassifyImage' at '/minifi_tensor'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'DetectObject' at '/minifi_tensor'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'DrawBoundingBox' at '/minifi_tensor'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'TractModelService' at '/minifi_tensor'" in less than 1 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings

  Scenario: All low-level processors register cleanly
    Given log property "logger.org::apache::nifi::minifi::core::extension::ExtensionManager" is set to "TRACE,stderr"
    And log property "logger.org::apache::nifi::minifi::core::ClassLoader" is set to "TRACE,stderr"

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "Registering class 'ImageToTensor' at '/minifi_tensor'" in less than 10 seconds
    And the Minifi logs contain the following message: "Registering class 'InvokeTractModel' at '/minifi_tensor'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'FilterBoundingBoxes' at '/minifi_tensor'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'ClassifyOutput' at '/minifi_tensor'" in less than 1 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings
