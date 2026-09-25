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

@SUPPORTS_WINDOWS
Feature: minifi_tensor extension loads

  Scenario: All processors and the TractModelService register cleanly
    Given log property "logger.org::apache::nifi::minifi::core::extension::ExtensionManager" is set to "TRACE,stderr"
    And log property "logger.org::apache::nifi::minifi::core::ClassLoader" is set to "TRACE,stderr"

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "Registering class 'ImageToTensor' at '/minifi_tensor'" in less than 10 seconds
    And the Minifi logs contain the following message: "Registering class 'InvokeTractModel' at '/minifi_tensor'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'FilterBoundingBoxes' at '/minifi_tensor'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'ClassifyOutput' at '/minifi_tensor'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'TractModelService' at '/minifi_tensor'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'ClassifyImage' at '/minifi_tensor'" in less than 1 seconds
    And the Minifi logs contain the following message: "Registering class 'DetectObject' at '/minifi_tensor'" in less than 1 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings
