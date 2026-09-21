@ENABLE_ATLAS
Feature: SiteToSitePortExtractor emits cross-instance-correlatable port entities

  # Mirrors extensions/atlas/tests/features/baseline/nifi/file-mode-with-s2s/. A MiNiFi-side
  # SEND to a remote NiFi input port must produce a `nifi_input_port` entity keyed on the
  # REMOTE PORT UUID (<port-uuid>@<namespace>), because that is exactly the entity the
  # receiving instance advertises for the same port - so Atlas merges the two and lineage
  # spans both systems. The port UUID is delivered to the extractor via the `s2s.port.id`
  # attribute stamped by SiteToSiteClient (mirroring NiFi's SiteToSiteAttributes.S2S_PORT_ID);
  # the type is `nifi_input_port` because we send to a remote *input* port (the same kind both
  # sides see - it is not perspective-flipped).
  #
  # The remote port's UUID here is the RPG port's id, which this flow also assigns to the NiFi
  # receiver's input port, so the assertion can spell the exact qualifiedName. Namespace note:
  # with no `hostnamePattern.*` rule the port resolves to MiNiFi's default namespace
  # (${scenario_id}); a real cross-instance merge needs a `hostnamePattern.<receiver-ns>` rule
  # so the port lands in the receiver's namespace (see the features README).

  Scenario: MiNiFi's RPG-driven Site-to-Site produces a correlatable nifi_input_port
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

    # The nifi_input_port is keyed on the remote port UUID (the "to_nifi" RPG port id), not the
    # transit URI, so its qualifiedName is a stable <port-uuid>@<namespace> that matches what the
    # receiver advertises. We resolve that id from the flow definition to assert the exact qn.
    Then a "nifi_input_port" entity for the RemoteProcessGroup "RemoteProcessGroup" input port "to_nifi" exists in Atlas in namespace "${scenario_id}" within 180 seconds
