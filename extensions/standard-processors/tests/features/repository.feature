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
Feature: Flow file and content repositories work as expected

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
