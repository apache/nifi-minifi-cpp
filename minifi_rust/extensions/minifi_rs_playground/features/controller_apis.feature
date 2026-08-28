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
Feature: Testing controller service api casting

  Scenario: Zoo has a jetpack dog
    Given a ZooProcessorRs processor with the "Can fly service" property set to "Wolfie the magical"
    And the "Number of Legs service" property of the ZooProcessorRs processor is set to "Wolfie the magical"
    And a DogControllerRs controller service named "Wolfie the magical" is set up and the "Has Jetpack" property set to "true"
    And the "Extra information" property of the Wolfie the magical controller service is set to "The dog (Canis familiaris or Canis lupus familiaris) is a domesticated descendant of wolves."
    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "[minifi_rs_playground::processors::zoo_processor::ZooProcessorRs] [critical] Can DogControllerRs { has_jetpack: true, extra_info: Some("The dog (Canis familiaris or Canis lupus familiaris) is a domesticated descendant of wolves.") } fly? true" in less than 10 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings
