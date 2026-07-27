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
Feature: Logs should be lazily evaluated

  Scenario: CountActualLogging only increments self when actually logging
    Given a CountActualLogging processor
    And log property "logger.minifi_rs_playground::processors::count_actual_logging::CountActualLogging" is set to "INFO,stderr"
    And CountActualLogging is TIMER_DRIVEN with 1 min scheduling period

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "[minifi_rs_playground::processors::count_actual_logging::CountActualLogging] [info] info 1" in less than 10 seconds
    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings
