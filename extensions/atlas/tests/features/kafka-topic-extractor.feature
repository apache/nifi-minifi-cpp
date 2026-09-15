@ENABLE_ATLAS
@ENABLE_KAFKA
Feature: KafkaTopicExtractor emits kafka_topic entities matching NiFi's baseline

  # Depends on the send()/receive() provenance calls added to PublishKafka/ConsumeKafka in this
  # same change set - without them the C++ atlas extractor's URI path never sees a transit URI
  # and no kafka_topic entity is emitted for either direction.

  Scenario: PublishKafka sends to a topic and Atlas gets a kafka_topic output entity
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
