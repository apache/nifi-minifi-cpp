@ENABLE_ATLAS
Feature: FilePathExtractor emits fs_path entities matching NiFi's baseline

  # Mirrors extensions/atlas/tests/features/baseline/nifi/file-mode/ - a GetFile -> PutFile
  # flow produces one fs_path per input and one per output, plus a nifi_flow_path connecting
  # them. Both processors already call session.getProvenanceReporter()->receive/send() today,
  # so no processor-side changes are required for this scenario.

  Scenario: GetFile to PutFile produces fs_path entities with FILE-level qualified names
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
    Then a "fs_path" entity with qualified name "/tmp/input/hello.txt@${scenario_id}" exists in Atlas within 120 seconds
    And a "fs_path" entity with qualified name "/tmp/output/hello.txt@${scenario_id}" exists in Atlas within 60 seconds
