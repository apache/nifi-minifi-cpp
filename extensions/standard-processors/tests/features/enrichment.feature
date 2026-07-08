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

@CORE @SUPPORTS_WINDOWS
Feature: ForkEnrichment and JoinEnrichmentAttributes

  Scenario: Merges correctly
    Given a GenerateFlowFile processor with the "Custom Text" property set to "original_${literal("content")}"
    And the scheduling period of the GenerateFlowFile processor is set to "1 hour"
    And the "Data Format" property of the GenerateFlowFile processor is set to "Text"
    And the "Unique FlowFiles" property of the GenerateFlowFile processor is set to "false"

    And a ForkEnrichment processor
    And a JoinEnrichmentAttributes processor

    And a ReplaceText processor with the "Evaluation Mode" property set to "Entire text"
    And the "Replacement Strategy" property of the ReplaceText processor is set to "Always Replace"
    And the "Replacement Value" property of the ReplaceText processor is set to "replaced_content"

    And an UpdateAttribute processor with the "extra_prop" property set to "foo"

    And a LogAttribute processor with the "Log Payload" property set to "true"

    And the "success" relationship of the GenerateFlowFile processor is connected to the ForkEnrichment
    And the "original" relationship of the ForkEnrichment processor is connected to the JoinEnrichmentAttributes
    And the "enrichment" relationship of the ForkEnrichment processor is connected to the ReplaceText
    And the "success" relationship of the ReplaceText processor is connected to the UpdateAttribute
    And the "success" relationship of the UpdateAttribute processor is connected to the JoinEnrichmentAttributes
    And the "joined" relationship of the JoinEnrichmentAttributes processor is connected to the LogAttribute
    And JoinEnrichmentAttributes's original relationship is auto-terminated
    And LogAttribute's success relationship is auto-terminated
    When the MiNiFi instance starts up
    Then the Minifi logs contain the following message: "key:enrichment.role value:JOINED" in less than 10 seconds
    And the Minifi logs contain the following message: "key:extra_prop value:foo" in less than 1 seconds
    And the Minifi logs contain the following message: "original_content" in less than 1 seconds
