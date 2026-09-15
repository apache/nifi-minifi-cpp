@ENABLE_ATLAS
Feature: SiteToSitePortExtractor emits nifi_output_port entities matching NiFi's baseline

  # Mirrors extensions/atlas/tests/features/baseline/nifi/file-mode-with-s2s/. MiNiFi's
  # SiteToSiteClient already emits SEND provenance events with the peer URL as transit URI,
  # so no processor-side changes are needed for this scenario.
  #
  # Note on the entity type: SiteToSitePortExtractor hardcodes `nifi_output_port` regardless
  # of direction - a MiNiFi-side SEND to a NiFi input port still lands in Atlas as a
  # nifi_output_port entity. This is a known simplification in the C++ port that this test
  # locks in.

  Scenario: MiNiFi's RPG-driven Site-to-Site produces nifi_output_port lineage
    Given an Atlas server is available
    And a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a RemoteProcessGroup node with name "RemoteProcessGroup" is opened on "http://nifi-${scenario_id}:8080/nifi"
    And an input port with name "to_nifi" is created on the RemoteProcessGroup named "RemoteProcessGroup"
    And the "success" relationship of the GenerateFlowFile processor is connected to the to_nifi
    And MiNiFi configuration "nifi.provenance.repository.class.name" is set to "ProvenanceRepository"
    And a ReportLineageToAtlas reporting task with the name "atlas-rt"
    And the "atlas-rt" reporting task is configured for the Atlas server
    And the scheduling period of the "atlas-rt" reporting task is set to "10 sec"

    And a NiFi container is set up
    And a NiFi flow is receiving data from the RemoteProcessGroup named "RemoteProcessGroup" in an input port named "from-minifi" which has the same id as the port named "to_nifi"
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "nifi" flow
    And in the "nifi" flow the "success" relationship of the from-minifi node is connected to the PutFile
    And PutFile's success relationship is auto-terminated in the "nifi" flow
    And PutFile's failure relationship is auto-terminated in the "nifi" flow

    When NiFi is started
    And all instances start up

    # SiteToSiteClient's transit URI is "<peer-url>/<port-uuid>". The extractor uses the
    # transit URI verbatim (plus the @<ns> suffix) as the nifi_output_port's qualifiedName,
    # so we can't spell the full qn up-front - assert on the namespace suffix instead.
    Then a "nifi_output_port" entity with qualified name ending in "@${scenario_id}" exists in Atlas within 180 seconds
