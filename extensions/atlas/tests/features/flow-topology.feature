@ENABLE_ATLAS
Feature: FlowPathBuilder emits nifi_flow / nifi_flow_path / nifi_queue topology entities

  # These entities are derived from the flow graph itself (not from provenance), so the reporting
  # task publishes them on its first cycle regardless of whether any data has moved. Their
  # qualifiedNames embed component UUIDs a scenario can't predict, so we match on the deterministic
  # `name` attribute (root group name, comma-joined processor names, or "queue to <dest>").

  Scenario: A linear flow produces a nifi_flow and a nifi_flow_path
    Given an Atlas server is available
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file "hello.txt" with the content "hello-atlas"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    And PutFile's failure relationship is auto-terminated
    And MiNiFi configuration "nifi.provenance.repository.class.name" is set to "ProvenanceRepository"
    And a ReportLineageToAtlas reporting task with the name "atlas-rt"
    And the "atlas-rt" reporting task is configured for the Atlas server
    And the scheduling period of the "atlas-rt" reporting task is set to "10 sec"
    When all instances start up
    # The root group name is fixed to "MiNiFi Flow" by the test framework.
    Then a "nifi_flow" entity with attribute "name" equal to "MiNiFi Flow" exists in Atlas within 120 seconds
    # GetFile -> PutFile collapses into a single linear path named after both processors.
    And a "nifi_flow_path" entity with attribute "name" equal to "GetFile, PutFile" exists in Atlas within 60 seconds

  Scenario: A fan-out flow produces nifi_queue entities between the paths
    Given an Atlas server is available
    # A non-zero payload keeps GenerateFlowFile producing flow files; the topology is reported
    # regardless, but this keeps the flow live like the other scenarios.
    And a GenerateFlowFile processor with the "File Size" property set to "10B"
    And a PutFile processor with the name "PutFileA" and the "Directory" property set to "/tmp/outputA"
    And a PutFile processor with the name "PutFileB" and the "Directory" property set to "/tmp/outputB"
    And the "success" relationship of the GenerateFlowFile processor is connected to the PutFileA
    And the "success" relationship of the GenerateFlowFile processor is connected to the PutFileB
    And PutFileA's success relationship is auto-terminated
    And PutFileA's failure relationship is auto-terminated
    And PutFileB's success relationship is auto-terminated
    And PutFileB's failure relationship is auto-terminated
    And MiNiFi configuration "nifi.provenance.repository.class.name" is set to "ProvenanceRepository"
    And a ReportLineageToAtlas reporting task with the name "atlas-rt"
    And the "atlas-rt" reporting task is configured for the Atlas server
    And the scheduling period of the "atlas-rt" reporting task is set to "10 sec"
    When all instances start up
    # The fan-out splits into three single-processor paths; each cross-path connection becomes a
    # nifi_queue named "queue to <connection name>". The test framework names connections
    # "<source>/<relationship>/<target>" (see MinifiFlowDefinition.to_yaml), so the two edges out
    # of GenerateFlowFile yield the queue names below.
    Then a "nifi_queue" entity with attribute "name" equal to "queue to GenerateFlowFile/success/PutFileA" exists in Atlas within 120 seconds
    And a "nifi_queue" entity with attribute "name" equal to "queue to GenerateFlowFile/success/PutFileB" exists in Atlas within 60 seconds
