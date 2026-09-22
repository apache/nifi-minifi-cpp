@ENABLE_ATLAS
Feature: Atlas reporting task emits fs_path, kafka_topic, flow-topology and site-to-site entities

  # All Atlas scenarios share ONE Atlas container (booted once in environment.py's before_all and
  # cleared between scenarios) because Atlas is heavy to cold-start. They live in a single feature
  # file so that behavex's feature-parallel scheme runs them in one worker against that one Atlas.
  # Each scenario is isolated by its own metadata namespace (`@${scenario_id}`), and the shared
  # Atlas graph is wiped between scenarios (see AtlasServerContainer.clear_entities).

  Scenario: GetFile to PutFile produces fs_path entities with FILE-level qualified names
    # Mirrors extensions/atlas/tests/features/baseline/nifi/file-mode/ - a GetFile -> PutFile
    # flow produces one fs_path per input and one per output, plus a nifi_flow_path connecting
    # them. Both processors already call session.getProvenanceReporter()->receive/send() today,
    # so no processor-side changes are required for this scenario.
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
    # FilePathExtractor sets name = the file's basename and path = the full path.
    And the "fs_path" entity with qualified name "/tmp/input/hello.txt@${scenario_id}" has attribute "name" equal to "hello.txt"
    And the "fs_path" entity with qualified name "/tmp/input/hello.txt@${scenario_id}" has attribute "path" equal to "/tmp/input/hello.txt"
    And the "fs_path" entity with qualified name "/tmp/output/hello.txt@${scenario_id}" has attribute "name" equal to "hello.txt"
    And the "fs_path" entity with qualified name "/tmp/output/hello.txt@${scenario_id}" has attribute "path" equal to "/tmp/output/hello.txt"
    # GetFile and PutFile form one linear path "GetFile, PutFile"; the RECEIVE lands as an input
    # fs_path and the SEND as an output fs_path on that flow path.
    And the "nifi_flow_path" entity named "GetFile, PutFile" has an input of type "fs_path" with qualified name "/tmp/input/hello.txt@${scenario_id}" within 60 seconds
    And the "nifi_flow_path" entity named "GetFile, PutFile" has an output of type "fs_path" with qualified name "/tmp/output/hello.txt@${scenario_id}" within 60 seconds

  Scenario: A linear flow produces a nifi_flow and a nifi_flow_path
    # Topology entities are derived from the flow graph itself (not from provenance), so the
    # reporting task publishes them on its first cycle regardless of whether any data has moved.
    # Their qualifiedNames embed component UUIDs a scenario can't predict, so we match on the
    # deterministic `name` attribute (root group name or comma-joined processor names).
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

  @ENABLE_KAFKA
  Scenario: PublishKafka sends to a topic and Atlas gets a kafka_topic output entity
    # Depends on the send()/receive() provenance calls added to PublishKafka/ConsumeKafka in this
    # same change set - without them the C++ atlas extractor's URI path never sees a transit URI
    # and no kafka_topic entity is emitted for either direction.
    Given an Atlas server is available
    And a Kafka server is set up
    # Non-zero File Size: PublishKafka's default `Fail Empty Flow Files=true` routes empty
    # flow files to failure without a Kafka send, so an empty payload would produce no SEND
    # provenance event and the extractor would never emit a kafka_topic entity.
    And a GenerateFlowFile processor with the "File Size" property set to "10B"
    And a PublishKafka processor
    And these processor properties are set
      | processor name | property name    | property value                   |
      | PublishKafka   | Client Name      | atlas-publish-client             |
      | PublishKafka   | Known Brokers    | kafka-server-${scenario_id}:9092 |
      | PublishKafka   | Topic Name       | atlas-publish-topic              |
      | PublishKafka   | Message Timeout  | 10 sec                           |
    And the "success" relationship of the GenerateFlowFile processor is connected to the PublishKafka
    And PublishKafka's success relationship is auto-terminated
    And PublishKafka's failure relationship is auto-terminated
    And MiNiFi configuration "nifi.provenance.repository.class.name" is set to "ProvenanceRepository"
    And a ReportLineageToAtlas reporting task with the name "atlas-rt"
    And the "atlas-rt" reporting task is configured for the Atlas server
    And the scheduling period of the "atlas-rt" reporting task is set to "10 sec"
    When the Kafka server is started
    And all instances start up
    Then a "kafka_topic" entity with qualified name "atlas-publish-topic@${scenario_id}" exists in Atlas within 120 seconds
    # KafkaTopicExtractor sets topic and name to the topic. (uri = the raw kafka:// transit URI,
    # which embeds the broker string - left unasserted since its exact shape is broker-dependent.)
    And the "kafka_topic" entity with qualified name "atlas-publish-topic@${scenario_id}" has attribute "topic" equal to "atlas-publish-topic"
    And the "kafka_topic" entity with qualified name "atlas-publish-topic@${scenario_id}" has attribute "name" equal to "atlas-publish-topic"
    # Publish is a SEND, so the topic lands as an output on the "GenerateFlowFile, PublishKafka" path.
    And the "nifi_flow_path" entity named "GenerateFlowFile, PublishKafka" has an output of type "kafka_topic" with qualified name "atlas-publish-topic@${scenario_id}" within 60 seconds

  @ENABLE_KAFKA
  Scenario: ConsumeKafka receives from a topic and Atlas gets a kafka_topic input entity
    Given an Atlas server is available
    And a Kafka server is set up
    And a ConsumeKafka processor
    And these processor properties are set
      | processor name | property name  | property value                   |
      | ConsumeKafka   | Kafka Brokers  | kafka-server-${scenario_id}:9092 |
      | ConsumeKafka   | Topic Names    | atlas-consume-topic              |
      | ConsumeKafka   | Group ID       | atlas-consume-group              |
      | ConsumeKafka   | Offset Reset   | earliest                         |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the ConsumeKafka processor is connected to the PutFile
    And PutFile's success relationship is auto-terminated
    And PutFile's failure relationship is auto-terminated
    And MiNiFi configuration "nifi.provenance.repository.class.name" is set to "ProvenanceRepository"
    And a ReportLineageToAtlas reporting task with the name "atlas-rt"
    And the "atlas-rt" reporting task is configured for the Atlas server
    And the scheduling period of the "atlas-rt" reporting task is set to "10 sec"
    When the Kafka server is started
    And the topic "atlas-consume-topic" is initialized on the kafka broker
    And a message with content "kafka-payload" is published to the "atlas-consume-topic" topic
    And all instances start up
    Then a "kafka_topic" entity with qualified name "atlas-consume-topic@${scenario_id}" exists in Atlas within 120 seconds
    And the "kafka_topic" entity with qualified name "atlas-consume-topic@${scenario_id}" has attribute "topic" equal to "atlas-consume-topic"
    And the "kafka_topic" entity with qualified name "atlas-consume-topic@${scenario_id}" has attribute "name" equal to "atlas-consume-topic"
    # Consume is a RECEIVE, so the topic lands as an input on the "ConsumeKafka, PutFile" path.
    And the "nifi_flow_path" entity named "ConsumeKafka, PutFile" has an input of type "kafka_topic" with qualified name "atlas-consume-topic@${scenario_id}" within 60 seconds

  Scenario: MiNiFi's RPG-driven Site-to-Site produces a correlatable nifi_input_port
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
