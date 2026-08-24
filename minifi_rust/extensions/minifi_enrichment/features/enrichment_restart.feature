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

@MINIFI_EXTENSION_ENRICHMENT
Feature: JoinEnrichmentAttributesRs recovers released FlowFiles after an agent restart

  # JoinEnrichmentAttributesRs releases the first half of a pair and holds it in memory until the
  # other half arrives. The released FlowFile is not routed, persisted-as-moved, nor deleted on
  # commit, so its record stays in the (default, RocksDB-backed) flow file repository. When the
  # agent is restarted the held FlowFile must be recovered from disk and re-enqueued, so that the
  # join still completes once its pair finally arrives.
  #
  # The two halves are produced independently with GetFile + UpdateAttribute (instead of
  # ForkEnrichmentRs) so their arrival can be separated across the restart: the ORIGINAL is present
  # at startup, and its ENRICHMENT pair is only delivered after the restart.

  Scenario: A released FlowFile held across an agent restart is recovered and joined with its late-arriving pair
    Given a GetFile processor with the name "GetOriginal" and the "Input Directory" property set to "/tmp/original_input"
    And the scheduling period of the GetOriginal processor is set to "100 ms"
    And a UpdateAttribute processor with the name "TagOriginal" and the "enrichment.role" property set to "ORIGINAL"
    And the "enrichment.group.id" property of the TagOriginal processor is set to "group-1"

    And a GetFile processor with the name "GetEnrichment" and the "Input Directory" property set to "/tmp/enrichment_input"
    And the scheduling period of the GetEnrichment processor is set to "100 ms"
    And a UpdateAttribute processor with the name "TagEnrichment" and the "enrichment.role" property set to "ENRICHMENT"
    And the "enrichment.group.id" property of the TagEnrichment processor is set to "group-1"

    And a JoinEnrichmentAttributesRs processor
    And a PutFile processor with the "Directory" property set to "/tmp/output"

    And the "success" relationship of the GetOriginal processor is connected to the TagOriginal
    And the "success" relationship of the TagOriginal processor is connected to the JoinEnrichmentAttributesRs
    And the "success" relationship of the GetEnrichment processor is connected to the TagEnrichment
    And the "success" relationship of the TagEnrichment processor is connected to the JoinEnrichmentAttributesRs
    And the "joined" relationship of the JoinEnrichmentAttributesRs processor is connected to the PutFile

    And JoinEnrichmentAttributesRs's original relationship is auto-terminated
    And JoinEnrichmentAttributesRs's invalid relationship is auto-terminated
    And JoinEnrichmentAttributesRs's timeout relationship is auto-terminated
    And PutFile's success relationship is auto-terminated
    And PutFile's failure relationship is auto-terminated

    # Only the ORIGINAL half exists at startup; the ENRICHMENT half arrives after the restart.
    And a directory at "/tmp/original_input" has a file with the content "original_content"

    When the MiNiFi instance starts up
    # JoinEnrichmentAttributesRs gets the ORIGINAL, releases it and holds it waiting for its pair,
    # so nothing is joined yet.
    Then no files are placed in the "/tmp/output" directory in 5 seconds of running time

    # Graceful stop destroys JoinEnrichmentAttributesRs while it still holds the released ORIGINAL,
    # then restart brings the agent back with the persistent repositories intact.
    When MiNiFi is stopped
    And MiNiFi is restarted

    # The released ORIGINAL is recovered from the flow file repository and re-enqueued. Once its
    # ENRICHMENT pair arrives, the join produces a single FlowFile with the ORIGINAL's content.
    And a file with the content "enrichment_content" is placed in "/tmp/enrichment_input"
    Then a single file with the content "original_content" is placed in the "/tmp/output" directory in less than 60 seconds
