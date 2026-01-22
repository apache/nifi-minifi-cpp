<!--
Licensed to the Apache Software Foundation (ASF) under one or more
contributor license agreements.  See the NOTICE file distributed with
this work for additional information regarding copyright ownership.
The ASF licenses this file to You under the Apache License, Version 2.0
(the "License"); you may not use this file except in compliance with
the License.  You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

## Table of Contents

- [AppendHostInfo](#AppendHostInfo)
- [ApplyTemplate](#ApplyTemplate)
- [AttributeRollingWindow](#AttributeRollingWindow)
- [AttributesToJSON](#AttributesToJSON)
- [BinFiles](#BinFiles)
- [CaptureRTSPFrame](#CaptureRTSPFrame)
- [CollectKubernetesPodMetrics](#CollectKubernetesPodMetrics)
- [CompressContent](#CompressContent)
- [ConsumeJournald](#ConsumeJournald)
- [ConsumeKafka](#ConsumeKafka)
- [ConsumeMQTT](#ConsumeMQTT)
- [ConsumeWindowsEventLog](#ConsumeWindowsEventLog)
- [ConvertRecord](#ConvertRecord)
- [DefragmentText](#DefragmentText)
- [DeleteAzureBlobStorage](#DeleteAzureBlobStorage)
- [DeleteAzureDataLakeStorage](#DeleteAzureDataLakeStorage)
- [DeleteGCSObject](#DeleteGCSObject)
- [DeleteS3Object](#DeleteS3Object)
- [EvaluateJsonPath](#EvaluateJsonPath)
- [ExecuteProcess](#ExecuteProcess)
- [ExecuteScript](#ExecuteScript)
- [ExecuteSQL](#ExecuteSQL)
- [ExtractText](#ExtractText)
- [FetchAzureBlobStorage](#FetchAzureBlobStorage)
- [FetchAzureDataLakeStorage](#FetchAzureDataLakeStorage)
- [FetchFile](#FetchFile)
- [FetchGCSObject](#FetchGCSObject)
- [FetchModbusTcp](#FetchModbusTcp)
- [FetchOPCProcessor](#FetchOPCProcessor)
- [FetchS3Object](#FetchS3Object)
- [FetchSFTP](#FetchSFTP)
- [FetchSmb](#FetchSmb)
- [FocusArchiveEntry](#FocusArchiveEntry)
- [GenerateFlowFile](#GenerateFlowFile)
- [GetCouchbaseKey](#GetCouchbaseKey)
- [GetFile](#GetFile)
- [GetTCP](#GetTCP)
- [HashContent](#HashContent)
- [InvokeHTTP](#InvokeHTTP)
- [JoltTransformJSON](#JoltTransformJSON)
- [ListAzureBlobStorage](#ListAzureBlobStorage)
- [ListAzureDataLakeStorage](#ListAzureDataLakeStorage)
- [ListenHTTP](#ListenHTTP)
- [ListenSyslog](#ListenSyslog)
- [ListenTCP](#ListenTCP)
- [ListenUDP](#ListenUDP)
- [ListFile](#ListFile)
- [ListGCSBucket](#ListGCSBucket)
- [ListS3](#ListS3)
- [ListSFTP](#ListSFTP)
- [ListSmb](#ListSmb)
- [RunLlamaCppInference](#RunLlamaCppInference)
- [LogAttribute](#LogAttribute)
- [ManipulateArchive](#ManipulateArchive)
- [MergeContent](#MergeContent)
- [MotionDetector](#MotionDetector)
- [PerformanceDataMonitor](#PerformanceDataMonitor)
- [PostElasticsearch](#PostElasticsearch)
- [ProcFsMonitor](#ProcFsMonitor)
- [PublishKafka](#PublishKafka)
- [PublishMQTT](#PublishMQTT)
- [PushGrafanaLokiGrpc](#PushGrafanaLokiGrpc)
- [PushGrafanaLokiREST](#PushGrafanaLokiREST)
- [PutAzureBlobStorage](#PutAzureBlobStorage)
- [PutAzureDataLakeStorage](#PutAzureDataLakeStorage)
- [PutCouchbaseKey](#PutCouchbaseKey)
- [PutFile](#PutFile)
- [PutGCSObject](#PutGCSObject)
- [PutKinesisStream](#PutKinesisStream)
- [PutOPCProcessor](#PutOPCProcessor)
- [PutS3Object](#PutS3Object)
- [PutSFTP](#PutSFTP)
- [PutSmb](#PutSmb)
- [PutSplunkHTTP](#PutSplunkHTTP)
- [PutSQL](#PutSQL)
- [PutTCP](#PutTCP)
- [PutUDP](#PutUDP)
- [QueryDatabaseTable](#QueryDatabaseTable)
- [QuerySplunkIndexingStatus](#QuerySplunkIndexingStatus)
- [ReplaceText](#ReplaceText)
- [RetryFlowFile](#RetryFlowFile)
- [RouteOnAttribute](#RouteOnAttribute)
- [RouteText](#RouteText)
- [SegmentContent](#SegmentContent)
- [SplitContent](#SplitContent)
- [SplitJson](#SplitJson)
- [SplitRecord](#SplitRecord)
- [SplitText](#SplitText)
- [TailEventLog](#TailEventLog)
- [TailFile](#TailFile)
- [UnfocusArchiveEntry](#UnfocusArchiveEntry)
- [UpdateAttribute](#UpdateAttribute)


## AppendHostInfo

### Description

Appends host information such as IP address and hostname as an attribute to incoming flowfiles.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                     | Default Value   | Allowable Values                 | Description                                                                            |
|--------------------------|-----------------|----------------------------------|----------------------------------------------------------------------------------------|
| Network Interface Filter |                 |                                  | A regular expression to filter ip addresses based on the name of the network interface |
| **Hostname Attribute**   | source.hostname |                                  | Flowfile attribute used to record the agent's hostname                                 |
| **IP Attribute**         | source.ipv4     |                                  | Flowfile attribute used to record the agent's IP addresses in a comma separated list   |
| Refresh Policy           | On schedule     | On schedule<br/>On every trigger | When to recalculate the host info                                                      |

### Relationships

| Name    | Description                            |
|---------|----------------------------------------|
| success | success operational on the flow record |


## ApplyTemplate

### Description

Applies the mustache template specified by the "Template" property and writes the output to the flow file content. FlowFile attributes are used as template parameters.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name     | Default Value | Allowable Values | Description                              |
|----------|---------------|------------------|------------------------------------------|
| Template |               |                  | Path to the input mustache template file |

### Relationships

| Name    | Description                            |
|---------|----------------------------------------|
| success | success operational on the flow record |


## AttributeRollingWindow

### Description

Track a Rolling Window based on evaluating an Expression Language expression on each FlowFile. Each FlowFile will be emitted with the count of FlowFiles and total aggregate value of values processed in the current window.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                      | Default Value   | Allowable Values | Description                                                                                                                                                                                      |
|---------------------------|-----------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Value to track**        |                 |                  | The expression on which to evaluate each FlowFile. The result of the expression will be added to the rolling window value.<br/>**Supports Expression Language: true**                            |
| Time window               |                 |                  | The amount of time for a rolling window. The format of the value is expected to be a count followed by a time unit. For example 5 millis, 10 secs, 1 min, 3 hours, 2 days, etc.                  |
| **Window length**         | 0               |                  | The window length in number of values. Takes precedence over 'Time window'. If set to zero, the 'Time window' property is used instead.                                                          |
| **Attribute name prefix** | rolling.window. |                  | The prefix to add to the generated attribute names. For example, if this is set to 'rolling.window.', then the full attribute names will be 'rolling.window.value', 'rolling.window.count', etc. |

### Relationships

| Name    | Description                                                                    |
|---------|--------------------------------------------------------------------------------|
| success | All FlowFiles that are successfully processed are routed to this relationship. |
| failure | When a FlowFile fails, it is routed here.                                      |

### Output Attributes

| Attribute        | Relationship | Description                                            |
|------------------|--------------|--------------------------------------------------------|
| <prefix>count    | success      | Number of the values in the rolling window             |
| <prefix>value    | success      | Sum of the values in the rolling window                |
| <prefix>mean     | success      | Mean of the values in the rolling window               |
| <prefix>median   | success      | Median of the values in the rolling window             |
| <prefix>variance | success      | Variance of the values in the rolling window           |
| <prefix>stddev   | success      | Standard deviation of the values in the rolling window |
| <prefix>min      | success      | Smallest value in the rolling window                   |
| <prefix>max      | success      | Largest value in the rolling window                    |


## AttributesToJSON

### Description

Generates a JSON representation of the input FlowFile Attributes. The resulting JSON can be written to either a new Attribute 'JSONAttributes' or written to the FlowFile as content.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                          | Default Value      | Allowable Values                        | Description                                                                                                                                                                                                                                                                                                                       |
|-------------------------------|--------------------|-----------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Attributes List               |                    |                                         | Comma separated list of attributes to be included in the resulting JSON. If this value is left empty then all existing Attributes will be included. This list of attributes is case sensitive. If an attribute specified in the list is not found it will be be emitted to the resulting JSON with an empty string or NULL value. |
| Attributes Regular Expression |                    |                                         | Regular expression that will be evaluated against the flow file attributes to select the matching attributes. Both the matching attributes and the selected attributes from the Attributes List property will be written in the resulting JSON.                                                                                   |
| **Destination**               | flowfile-attribute | flowfile-attribute<br/>flowfile-content | Control if JSON value is written as a new flowfile attribute 'JSONAttributes' or written in the flowfile content. Writing to flowfile content will overwrite any existing flowfile content.                                                                                                                                       |
| **Include Core Attributes**   | true               | true<br/>false                          | Determines if the FlowFile core attributes which are contained in every FlowFile should be included in the final JSON value generated.                                                                                                                                                                                            |
| **Null Value**                | false              | true<br/>false                          | If true a non existing selected attribute will be NULL in the resulting JSON. If false an empty string will be placed in the JSON.                                                                                                                                                                                                |

### Relationships

| Name    | Description                                  |
|---------|----------------------------------------------|
| success | All FlowFiles received are routed to success |


## BinFiles

### Description

Bins flow files into buckets based on the number of entries or size of entries

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                      | Default Value | Allowable Values | Description                                                                                                |
|---------------------------|---------------|------------------|------------------------------------------------------------------------------------------------------------|
| Minimum Group Size        | 0             |                  | The minimum size of for the bundle                                                                         |
| Maximum Group Size        |               |                  | The maximum size for the bundle. If not specified, there is no maximum.                                    |
| Minimum Number of Entries | 1             |                  | The minimum number of files to include in a bundle                                                         |
| Maximum Number of Entries |               |                  | The maximum number of files to include in a bundle. If not specified, there is no maximum.                 |
| Maximum number of Bins    | 100           |                  | Specifies the maximum number of bins that can be held in memory at any one time                            |
| Max Bin Age               |               |                  | The maximum age of a Bin that will trigger a Bin to be complete. Expected format is <duration> <time unit> |
| Batch Size                | 1             |                  | Maximum number of FlowFiles processed in a single session                                                  |

### Relationships

| Name     | Description                                                                                                                  |
|----------|------------------------------------------------------------------------------------------------------------------------------|
| failure  | If the bundle cannot be created, all FlowFiles that would have been used to create the bundle will be transferred to failure |
| original | The FlowFiles that were used to create the bundle                                                                            |


## CaptureRTSPFrame

### Description

Captures a frame from the RTSP stream at specified intervals.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name           | Default Value | Allowable Values | Description                                                                            |
|----------------|---------------|------------------|----------------------------------------------------------------------------------------|
| RTSP Username  |               |                  | The username for connecting to the RTSP stream                                         |
| RTSP Password  |               |                  | Password used to connect to the RTSP stream<br/>**Sensitive Property: true**           |
| RTSP Hostname  |               |                  | Hostname of the RTSP stream we are trying to connect to                                |
| RTSP URI       |               |                  | URI that should be appended to the RTSP stream hostname                                |
| RTSP Port      |               |                  | Port that should be connected to to receive RTSP Frames                                |
| Image Encoding | .jpg          |                  | The encoding that should be applied the the frame images captured from the RTSP stream |

### Relationships

| Name    | Description                      |
|---------|----------------------------------|
| success | Successful capture of RTSP frame |
| failure | Failures to capture RTSP frame   |


## CollectKubernetesPodMetrics

### Description

A processor which collects pod metrics when MiNiFi is run inside Kubernetes.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                              | Default Value | Allowable Values | Description                                                |
|-----------------------------------|---------------|------------------|------------------------------------------------------------|
| **Kubernetes Controller Service** |               |                  | Controller service which provides Kubernetes functionality |

### Relationships

| Name    | Description                                    |
|---------|------------------------------------------------|
| success | All flow files produced are routed to Success. |


## CompressContent

### Description

Compresses or decompresses the contents of FlowFiles using a user-specified compression algorithm and updates the mime.type attribute as appropriate

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                  | Default Value           | Allowable Values                                                 | Description                                                                                                                                                                                                                                                                                                                                                                                                                    |
|-----------------------|-------------------------|------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Mode**              | compress                | compress<br/>decompress                                          | Indicates whether the processor should compress content or decompress content.                                                                                                                                                                                                                                                                                                                                                 |
| **Compression Level** | 1                       |                                                                  | The compression level to use; this is valid only when using GZIP compression.                                                                                                                                                                                                                                                                                                                                                  |
| Compression Format    | use mime.type attribute | gzip<br/>lzma<br/>xz-lzma2<br/>bzip2<br/>use mime.type attribute | The compression format to use.                                                                                                                                                                                                                                                                                                                                                                                                 |
| Update Filename       | false                   | true<br/>false                                                   | Determines if filename extension need to be updated                                                                                                                                                                                                                                                                                                                                                                            |
| Encapsulate in TAR    | true                    | true<br/>false                                                   | If true, on compression the FlowFile is added to a TAR archive and then compressed, and on decompression a compressed, TAR-encapsulated FlowFile is expected.<br/>If false, on compression the content of the FlowFile simply gets compressed, and on decompression a simple compressed content is expected.<br/>true is the behaviour compatible with older MiNiFi C++ versions, false is the behaviour compatible with NiFi. |
| Batch Size            | 1                       |                                                                  | Maximum number of FlowFiles processed in a single session                                                                                                                                                                                                                                                                                                                                                                      |

### Relationships

| Name    | Description                                                                                                   |
|---------|---------------------------------------------------------------------------------------------------------------|
| success | FlowFiles will be transferred to the success relationship after successfully being compressed or decompressed |
| failure | FlowFiles will be transferred to the failure relationship if they fail to compress/decompress                 |


## ConsumeJournald

### Description

Consume systemd-journald journal messages. Creates one flow file per message. Fields are mapped to attributes. Realtime timestamp is mapped to the 'timestamp' attribute. Available on Linux only.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                     | Default Value | Allowable Values         | Description                                                                                                                                                                                                                                    |
|--------------------------|---------------|--------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Batch Size**           | 1000          |                          | The maximum number of entries processed in a single execution.                                                                                                                                                                                 |
| **Payload Format**       | Syslog        | Raw<br/>Syslog           | Configures flow file content formatting. Raw: only the message. Syslog: similar to syslog or journalctl output.                                                                                                                                |
| **Include Timestamp**    | true          | true<br/>false           | Include message timestamp in the 'timestamp' attribute.                                                                                                                                                                                        |
| **Journal Type**         | System        | User<br/>System<br/>Both | Type of journal to consume.                                                                                                                                                                                                                    |
| **Process Old Messages** | false         | true<br/>false           | Process events created before the first usage (schedule) of the processor instance.                                                                                                                                                            |
| **Timestamp Format**     | %x %X %Z      |                          | Format string to use when creating the timestamp attribute or writing messages in the syslog format. ISO/ISO 8601/ISO8601 are equivalent to "%FT%T%Ez". See https://howardhinnant.github.io/date/date.html#to_stream_formatting for all flags. |

### Relationships

| Name    | Description                             |
|---------|-----------------------------------------|
| success | Successfully consumed journal messages. |


## ConsumeKafka

### Description

Consumes messages from Apache Kafka and transform them into MiNiFi FlowFiles. The application should make sure that the processor is triggered at regular intervals, even if no messages are expected, to serve any queued callbacks waiting to be called. Rebalancing can also only happen on trigger.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                         | Default Value  | Allowable Values                                     | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
|------------------------------|----------------|------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| SSL Context Service          |                |                                                      | SSL Context Service Name                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| **Security Protocol**        | plaintext      | plaintext<br/>ssl<br/>sasl_plaintext<br/>sasl_ssl    | Protocol used to communicate with brokers. Corresponds to Kafka's 'security.protocol' property.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| Kerberos Service Name        |                |                                                      | Kerberos Service Name                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| Kerberos Principal           |                |                                                      | Kerberos Principal                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| Kerberos Keytab Path         |                |                                                      | The path to the location on the local filesystem where the kerberos keytab is located. Read permission on the file is required.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| **SASL Mechanism**           | GSSAPI         | GSSAPI<br/>PLAIN                                     | The SASL mechanism to use for authentication. Corresponds to Kafka's 'sasl.mechanism' property.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| Username                     |                |                                                      | The username when the SASL Mechanism is sasl_plaintext                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| Password                     |                |                                                      | The password for the given username when the SASL Mechanism is sasl_plaintext<br/>**Sensitive Property: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| **Kafka Brokers**            | localhost:9092 |                                                      | A comma-separated list of known Kafka Brokers in the format <host>:<port>.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| **Topic Names**              |                |                                                      | The name of the Kafka Topic(s) to pull from. Multiple topic names are supported as a comma separated list.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| **Topic Name Format**        | Names          | Names<br/>Patterns                                   | Specifies whether the Topic(s) provided are a comma separated list of names or a single regular expression. Using regular expressions does not automatically discover Kafka topics created after the processor started.                                                                                                                                                                                                                                                                                                                                                                                                 |
| **Honor Transactions**       | true           | true<br/>false                                       | Specifies whether or not MiNiFi should honor transactional guarantees when communicating with Kafka. If false, the Processor will use an "isolation level" of read_uncomitted. This means that messages will be received as soon as they are written to Kafka but will be pulled, even if the producer cancels the transactions. If this value is true, MiNiFi will not receive any messages for which the producer's transaction was canceled, but this can result in some latency since the consumer must wait for the producer to finish its entire transaction instead of pulling as the messages become available. |
| **Group ID**                 |                |                                                      | A Group ID is used to identify consumers that are within the same consumer group. Corresponds to Kafka's 'group.id' property.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| **Offset Reset**             | latest         | earliest<br/>latest<br/>none                         | Allows you to manage the condition when there is no initial offset in Kafka or if the current offset does not exist any more on the server (e.g. because that data has been deleted). Corresponds to Kafka's 'auto.offset.reset' property.                                                                                                                                                                                                                                                                                                                                                                              |
| **Key Attribute Encoding**   | UTF-8          | UTF-8<br/>Hex                                        | FlowFiles that are emitted have an attribute named 'kafka.key'. This property dictates how the value of the attribute should be encoded.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| Message Demarcator           |                |                                                      | Since KafkaConsumer receives messages in batches, you have an option to output FlowFiles which contains all Kafka messages in a single batch for a given topic and partition and this property allows you to provide a string (interpreted as UTF-8) to use for demarcating apart multiple Kafka messages. This is an optional property and if not provided each Kafka message received will result in a single FlowFile which time it is triggered. <br/>**Supports Expression Language: true**                                                                                                                        |
| Message Header Encoding      | UTF-8          | UTF-8<br/>Hex                                        | Any message header that is found on a Kafka message will be added to the outbound FlowFile as an attribute. This property indicates the Character Encoding to use for deserializing the headers.                                                                                                                                                                                                                                                                                                                                                                                                                        |
| Headers To Add As Attributes |                |                                                      | A comma separated list to match against all message headers. Any message header whose name matches an item from the list will be added to the FlowFile as an Attribute. If not specified, no Header values will be added as FlowFile attributes. The behaviour on when multiple headers of the same name are present is set using the Duplicate Header Handling attribute.                                                                                                                                                                                                                                              |
| Duplicate Header Handling    | Keep Latest    | Keep First<br/>Keep Latest<br/>Comma-separated Merge | For headers to be added as attributes, this option specifies how to handle cases where multiple headers are present with the same key. For example in case of receiving these two headers: "Accept: text/html" and "Accept: application/xml" and we want to attach the value of "Accept" as a FlowFile attribute:<br/> - "Keep First" attaches: "Accept -> text/html"<br/> - "Keep Latest" attaches: "Accept -> application/xml"<br/> - "Comma-separated Merge" attaches: "Accept -> text/html, application/xml"<br/>                                                                                                   |
| Max Poll Records             | 10000          |                                                      | Specifies the maximum number of records Kafka should return when polling each time the processor is triggered.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| **Max Poll Time**            | 4 seconds      |                                                      | Specifies the maximum amount of time the consumer can use for polling data from the brokers. Polling is a blocking operation, so the upper limit of this value is specified in 4 seconds.                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Session Timeout              | 60 seconds     |                                                      | Client group session and failure detection timeout. The consumer sends periodic heartbeats to indicate its liveness to the broker. If no hearts are received by the broker for a group member within the session timeout, the broker will remove the consumer from the group and trigger a rebalance. The allowed range is configured with the broker configuration properties group.min.session.timeout.ms and group.max.session.timeout.ms.                                                                                                                                                                           |

### Relationships

| Name    | Description                                                                                                                     |
|---------|---------------------------------------------------------------------------------------------------------------------------------|
| success | Incoming Kafka messages as flowfiles. Depending on the demarcation strategy, this can be one or multiple flowfiles per message. |


## ConsumeMQTT

### Description

This Processor gets the contents of a FlowFile from a MQTT broker for a specified topic. The the payload of the MQTT message becomes content of a FlowFile. If Record Reader and Record Writer are set, then the MQTT message specific attributes are not set in the flow file, because different attributes can be set for different records. In this case if Add Attributes As Fields is set to true, the attributes will be added to each record as fields.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                        | Default Value | Allowable Values                     | Description                                                                                                                                               |
|-----------------------------|---------------|--------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Broker URI**              |               |                                      | The URI to use to connect to the MQTT broker                                                                                                              |
| Client ID                   |               |                                      | MQTT client ID to use. If not set, a UUID will be generated.                                                                                              |
| **MQTT Version**            | 3.x AUTO      | 3.x AUTO<br/>3.1.0<br/>3.1.1<br/>5.0 | The MQTT specification version when connecting to the broker.                                                                                             |
| **Topic**                   |               |                                      | The topic to subscribe to.                                                                                                                                |
| Clean Session               | true          | true<br/>false                       | Whether to start afresh rather than remembering previous subscriptions. If true, then make broker forget subscriptions after disconnected. MQTT 3.x only. |
| Clean Start                 | true          | true<br/>false                       | Whether to start afresh rather than remembering previous subscriptions. MQTT 5.x only.                                                                    |
| Session Expiry Interval     | 0 s           |                                      | Time to delete session on broker after client is disconnected. MQTT 5.x only.                                                                             |
| Queue Max Message           | 1000          |                                      | Maximum number of messages allowed on the received MQTT queue                                                                                             |
| Attribute From Content Type |               |                                      | Name of FlowFile attribute to be filled from content type of received message. MQTT 5.x only.                                                             |
| Topic Alias Maximum         | 0             |                                      | Maximum number of topic aliases to use. If set to 0, then topic aliases cannot be used. MQTT 5.x only.                                                    |
| Receive Maximum             | 65535         |                                      | Maximum number of unacknowledged messages allowed. MQTT 5.x only.                                                                                         |
| Record Reader               |               |                                      | The Record Reader to use for parsing received MQTT Messages into Records.                                                                                 |
| Record Writer               |               |                                      | The Record Writer to use for serializing Records before writing them to a FlowFile.                                                                       |
| Add Attributes As Fields    | true          | true<br/>false                       | If setting this property to true, default fields are going to be added in each record: _topic, _qos, _isDuplicate, _isRetained.                           |
| **Quality of Service**      | 0             | 0<br/>1<br/>2                        | The Quality of Service (QoS) of messages.                                                                                                                 |
| Connection Timeout          | 10 sec        |                                      | Maximum time interval the client will wait for the network connection to the MQTT broker                                                                  |
| Keep Alive Interval         | 60 sec        |                                      | Defines the maximum time interval between messages sent or received                                                                                       |
| Last Will Topic             |               |                                      | The topic to send the client's Last Will to. If the Last Will topic is not set then a Last Will will not be sent                                          |
| Last Will Message           |               |                                      | The message to send as the client's Last Will. If the Last Will Message is empty, Last Will will be deleted from the broker                               |
| **Last Will QoS**           | 0             | 0<br/>1<br/>2                        | The Quality of Service (QoS) to send the last will with.                                                                                                  |
| Last Will Retain            | false         | true<br/>false                       | Whether to retain the client's Last Will                                                                                                                  |
| Last Will Content Type      |               |                                      | Content type of the client's Last Will. MQTT 5.x only.                                                                                                    |
| Username                    |               |                                      | Username to use when connecting to the broker                                                                                                             |
| Password                    |               |                                      | Password to use when connecting to the broker<br/>**Sensitive Property: true**                                                                            |
| Security Protocol           |               |                                      | Protocol used to communicate with brokers                                                                                                                 |
| Security CA                 |               |                                      | File or directory path to CA certificate(s) for verifying the broker's key                                                                                |
| Security Cert               |               |                                      | Path to client's public key (PEM) used for authentication                                                                                                 |
| Security Private Key        |               |                                      | Path to client's private key (PEM) used for authentication                                                                                                |
| Security Pass Phrase        |               |                                      | Private key passphrase<br/>**Sensitive Property: true**                                                                                                   |

### Relationships

| Name    | Description                                                                                  |
|---------|----------------------------------------------------------------------------------------------|
| success | FlowFiles that are sent successfully to the destination are transferred to this relationship |

### Output Attributes

| Attribute            | Relationship | Description                                                                                                                           |
|----------------------|--------------|---------------------------------------------------------------------------------------------------------------------------------------|
| mqtt.broker          |              | URI of the sending broker                                                                                                             |
| mqtt.topic           |              | Topic of the message                                                                                                                  |
| mqtt.topic.segment.n |              | The nth topic segment of the message                                                                                                  |
| mqtt.qos             |              | The quality of service for this message.                                                                                              |
| mqtt.isDuplicate     |              | Whether or not this message might be a duplicate of one which has already been received.                                              |
| mqtt.isRetained      |              | Whether or not this message was from a current publisher, or was "retained" by the server as the last message published on the topic. |
| record.count         |              | The number of records received                                                                                                        |


## ConsumeWindowsEventLog

### Description

Registers a Windows Event Log Subscribe Callback to receive FlowFiles from Events on Windows. These can be filtered via channel and XPath.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                               | Default Value                                                                                                                                                                                                               | Allowable Values             | Description                                                                                                                                                                                                                                                                                                                                                           |
|------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Channel**                        | System                                                                                                                                                                                                                      |                              | The Windows Event Log Channel to listen to. In order to process logs from a log file use the format 'SavedLog:\<file path\>'.<br/>**Supports Expression Language: true**                                                                                                                                                                                              |
| **Query**                          | *                                                                                                                                                                                                                           |                              | XPath Query to filter events. (See https://msdn.microsoft.com/en-us/library/windows/desktop/dd996910(v=vs.85).aspx for examples.)<br/>**Supports Expression Language: true**                                                                                                                                                                                          |
| **Max Buffer Size**                | 1 MB                                                                                                                                                                                                                        |                              | The individual Event Log XMLs are rendered to a buffer. This specifies the maximum size in bytes that the buffer will be allowed to grow to. (Limiting the maximum size of an individual Event XML.)                                                                                                                                                                  |
| **Inactive Duration To Reconnect** | 10 min                                                                                                                                                                                                                      |                              | If no new event logs are processed for the specified time period,  this processor will try reconnecting to recover from a state where any further messages cannot be consumed. Such situation can happen if Windows Event Log service is restarted, or ERROR_EVT_QUERY_RESULT_STALE (15011) is returned. Setting no duration, e.g. '0 ms' disables auto-reconnection. |
| Identifier Match Regex             | .*Sid                                                                                                                                                                                                                       |                              | Regular Expression to match Subject Identifier Fields. These will be placed into the attributes of the FlowFile                                                                                                                                                                                                                                                       |
| Apply Identifier Function          | true                                                                                                                                                                                                                        | true<br/>false               | If true it will resolve SIDs matched in the 'Identifier Match Regex' to the DOMAIN\USERNAME associated with that ID                                                                                                                                                                                                                                                   |
| Resolve Metadata in Attributes     | true                                                                                                                                                                                                                        | true<br/>false               | If true, any metadata that is resolved ( such as IDs or keyword metadata ) will be placed into attributes, otherwise it will be replaced in the XML or text output                                                                                                                                                                                                    |
| Event Header Delimiter             |                                                                                                                                                                                                                             |                              | If set, the chosen delimiter will be used in the Event output header. Otherwise, a colon followed by spaces will be used.                                                                                                                                                                                                                                             |
| Event Header                       | LOG_NAME=Log Name, SOURCE = Source, TIME_CREATED = Date,EVENT_RECORDID=Record ID,EVENTID = Event ID,TASK_CATEGORY = Task Category,LEVEL = Level,KEYWORDS = Keywords,USER = User,COMPUTER = Computer, EVENT_TYPE = EventType |                              | Comma seperated list of key/value pairs with the following keys LOG_NAME, SOURCE, TIME_CREATED,EVENT_RECORDID,EVENTID,TASK_CATEGORY,LEVEL,KEYWORDS,USER,COMPUTER, and EVENT_TYPE. Eliminating fields will remove them from the header.                                                                                                                                |
| **Output Format**                  | XML                                                                                                                                                                                                                         | XML<br/>Plaintext<br/>JSON   | The format of the output flow files.                                                                                                                                                                                                                                                                                                                                  |
| **JSON Format**                    | Simple                                                                                                                                                                                                                      | Raw<br/>Simple<br/>Flattened | Set the json format type. Only applicable if Output Format is set to 'JSON'                                                                                                                                                                                                                                                                                           |
| Batch Commit Size                  | 1000                                                                                                                                                                                                                        |                              | Maximum number of Events to consume and create to Flow Files from before committing.                                                                                                                                                                                                                                                                                  |
| State Directory                    | CWELState                                                                                                                                                                                                                   |                              | DEPRECATED. Only use it for state migration from the state file, supplying the legacy state directory.                                                                                                                                                                                                                                                                |
| **Process Old Events**             | false                                                                                                                                                                                                                       | true<br/>false               | This property defines if old events (which are created before first time server is started) should be processed.                                                                                                                                                                                                                                                      |
| Cache SID Lookups                  | true                                                                                                                                                                                                                        | true<br/>false               | Determines whether SID to name lookups are cached in memory                                                                                                                                                                                                                                                                                                           |

### Relationships

| Name    | Description                                    |
|---------|------------------------------------------------|
| success | Relationship for successfully consumed events. |


## ConvertRecord

### Description

Converts records from one data format to another using configured Record Reader and Record Writer Controller Services.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                              | Default Value | Allowable Values | Description                                                                                                                                                                   |
|-----------------------------------|---------------|------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Record Reader**                 |               |                  | Specifies the Controller Service to use for reading incoming data                                                                                                             |
| **Record Writer**                 |               |                  | Specifies the Controller Service to use for writing out the records                                                                                                           |
| **Include Zero Record FlowFiles** | true          | true<br/>false   | When converting an incoming FlowFile, if the conversion results in no data, this property specifies whether or not a FlowFile will be sent to the corresponding relationship. |

### Relationships

| Name    | Description                                                                                                                                                      |
|---------|------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| failure | If a FlowFile cannot be transformed from the configured input format to the configured output format, the unchanged FlowFile will be routed to this relationship |
| success | FlowFiles that are successfully transformed will be routed to this relationship                                                                                  |

### Output Attributes

| Attribute            | Relationship | Description                                                                               |
|----------------------|--------------|-------------------------------------------------------------------------------------------|
| record.count         | success      | The number of records in the FlowFile                                                     |
| record.error.message | failure      | This attribute provides on failure the error message encountered by the Reader or Writer. |


## DefragmentText

### Description

DefragmentText splits and merges incoming flowfiles so cohesive messages are not split between them. It can handle multiple inputs differentiated by the absolute.path flow file attribute.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                 | Default Value    | Allowable Values                    | Description                                                                                                                                                                                                        |
|----------------------|------------------|-------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Pattern**          |                  |                                     | A regular expression to match at the start or end of messages.                                                                                                                                                     |
| **Pattern Location** | Start of Message | End of Message<br/>Start of Message | Whether the pattern is located at the start or at the end of the messages.                                                                                                                                         |
| Max Buffer Age       | 10 min           |                                     | The maximum age of the buffer after which it will be transferred to success when matching Start of Message patterns or to failure when matching End of Message patterns. Expected format is <duration> <time unit> |
| Max Buffer Size      |                  |                                     | The maximum buffer size, if the buffer exceeds this, it will be transferred to failure. Expected format is <size> <data unit>                                                                                      |

### Relationships

| Name    | Description                                        |
|---------|----------------------------------------------------|
| success | Flowfiles that have been successfully defragmented |
| failure | Flowfiles that failed the defragmentation process  |


## DeleteAzureBlobStorage

### Description

Deletes the provided blob from Azure Storage

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                   | Default Value   | Allowable Values                                                                  | Description                                                                                                                                                                                                                                                                                                                                                                                                    |
|----------------------------------------|-----------------|-----------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Azure Storage Credentials Service      |                 |                                                                                   | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                                                                                                                                                                                                     |
| Proxy Configuration Service            |                 |                                                                                   | Specifies the Proxy Configuration Controller Service to proxy network requests.                                                                                                                                                                                                                                                                                                                                |
| **Container Name**                     |                 |                                                                                   | Name of the Azure Storage container. In case of PutAzureBlobStorage processor, container can be created if it does not exist.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                       |
| Storage Account Name                   |                 |                                                                                   | The storage account name.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                           |
| Storage Account Key                    |                 |                                                                                   | The storage account key. This is an admin-like password providing access to every container in this account. It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies if Credential Configuration Strategy is set to From Properties. If set, SAS Token must be empty.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**        |
| SAS Token                              |                 |                                                                                   | Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Credential Configuration Strategy is set to From Properties. If set, Storage Account Key must be empty.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                         |
| Common Storage Account Endpoint Suffix |                 |                                                                                   | Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a different suffix in certain circumstances (like Azure Stack or non-public Azure regions).<br/>**Supports Expression Language: true**                                                                                                                                                                    |
| Connection String                      |                 |                                                                                   | Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Credential Configuration Strategy is set to From Properties.<br/>**Supports Expression Language: true**                                                                                                                                                                                      |
| **Credential Configuration Strategy**  | From Properties | From Properties<br/>Default Credential<br/>Managed Identity<br/>Workload Identity | The strategy to use for credential configuration. If set to From Properties, the credentials are parsed from the SAS Token, Storage Account Key, and Connection String properties. In other cases, the selected Azure identity source is used.                                                                                                                                                                 |
| Managed Identity Client ID             |                 |                                                                                   | Client ID of the managed identity. The property is required when User Assigned Managed Identity is used for authentication and multiple user-assigned identities are added to the resource. It must be empty in case of System Assigned Managed Identity and can also be left empty if only one user-assigned identity is present.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true** |
| Blob                                   |                 |                                                                                   | The filename of the blob. If left empty the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                             |
| **Delete Snapshots Option**            | None            | None<br/>Include Snapshots<br/>Delete Snapshots Only                              | Specifies the snapshot deletion options to be used when deleting a blob. None: Deletes the blob only. Include Snapshots: Delete the blob and its snapshots. Delete Snapshots Only: Delete only the blob's snapshots.                                                                                                                                                                                           |

### Relationships

| Name    | Description                                                             |
|---------|-------------------------------------------------------------------------|
| success | All successfully processed FlowFiles are routed to this relationship    |
| failure | Unsuccessful operations will be transferred to the failure relationship |


## DeleteAzureDataLakeStorage

### Description

Deletes the provided file from Azure Data Lake Storage

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                              | Default Value | Allowable Values | Description                                                                                                                                                                                                                             |
|-----------------------------------|---------------|------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Azure Storage Credentials Service |               |                  | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                              |
| Proxy Configuration Service       |               |                  | Specifies the Proxy Configuration Controller Service to proxy network requests.                                                                                                                                                         |
| **Filesystem Name**               |               |                  | Name of the Azure Storage File System. It is assumed to be already existing.<br/>**Supports Expression Language: true**                                                                                                                 |
| Directory Name                    |               |                  | Name of the Azure Storage Directory. The Directory Name cannot contain a leading '/'. If left empty it designates the root directory. The directory will be created if not already existing.<br/>**Supports Expression Language: true** |
| File Name                         |               |                  | The filename in Azure Storage. If left empty the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                 |

### Relationships

| Name    | Description                                                                                   |
|---------|-----------------------------------------------------------------------------------------------|
| success | If file deletion from Azure storage succeeds the flowfile is transferred to this relationship |
| failure | If file deletion from Azure storage fails the flowfile is transferred to this relationship    |


## DeleteGCSObject

### Description

Deletes an object from a Google Cloud Bucket.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                 | Default Value | Allowable Values | Description                                                                                                                                      |
|--------------------------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------|
| **GCP Credentials Provider Service** |               |                  | The Controller Service used to obtain Google Cloud Platform credentials. Should be the name of a GCPCredentialsControllerService.                |
| **Number of retries**                | 6             |                  | How many retry attempts should be made before routing to the failure relationship.                                                               |
| Endpoint Override URL                |               |                  | Overrides the default Google Cloud Storage endpoints<br/>**Supports Expression Language: true**                                                  |
| Bucket                               | ${gcs.bucket} |                  | Bucket of the object.<br/>**Supports Expression Language: true**                                                                                 |
| Key                                  | ${filename}   |                  | Name of the object.<br/>**Supports Expression Language: true**                                                                                   |
| Server Side Encryption Key           |               |                  | The AES256 Encryption Key (encoded in base64) for server-side decryption of the object.<br/>**Supports Expression Language: true**               |
| Object Generation                    |               |                  | The generation of the Object to download. If left empty, then it will download the latest generation.<br/>**Supports Expression Language: true** |

### Relationships

| Name    | Description                                                                                  |
|---------|----------------------------------------------------------------------------------------------|
| success | FlowFiles are routed to this relationship after a successful Google Cloud Storage operation. |
| failure | FlowFiles are routed to this relationship if the Google Cloud Storage operation fails.       |

### Output Attributes

| Attribute          | Relationship | Description                                             |
|--------------------|--------------|---------------------------------------------------------|
| gcs.status.message | failure      | The status message received from google cloud.          |
| gcs.error.reason   | failure      | The description of the error occurred during operation. |
| gcs.error.domain   | failure      | The domain of the error occurred during operation.      |


## DeleteS3Object

### Description

Deletes FlowFiles on an Amazon S3 Bucket. If attempting to delete a file that does not exist, FlowFile is routed to success.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                             | Default Value | Allowable Values                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         | Description                                                                                                                                                                                                                                                                                                 |
|----------------------------------|---------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Access Key                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | AWS account access key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                           |
| Secret Key                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | AWS account secret key<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                          |
| Credentials File                 |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey                                                                                                                                                                                 |
| AWS Credentials Provider service |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.                                                                                                                                                                                                         |
| **Region**                       | us-west-2     | af-south-1<br/>ap-east-1<br/>ap-northeast-1<br/>ap-northeast-2<br/>ap-northeast-3<br/>ap-south-1<br/>ap-south-2<br/>ap-southeast-1<br/>ap-southeast-2<br/>ap-southeast-3<br/>ap-southeast-4<br/>ap-southeast-5<br/>ap-southeast-7<br/>ca-central-1<br/>ca-west-1<br/>cn-north-1<br/>cn-northwest-1<br/>eu-central-1<br/>eu-central-2<br/>eu-isoe-west-1<br/>eu-north-1<br/>eu-south-1<br/>eu-south-2<br/>eu-west-1<br/>eu-west-2<br/>eu-west-3<br/>il-central-1<br/>me-central-1<br/>me-south-1<br/>mx-central-1<br/>sa-east-1<br/>us-east-1<br/>us-east-2<br/>us-gov-east-1<br/>us-gov-west-1<br/>us-iso-east-1<br/>us-iso-west-1<br/>us-isob-east-1<br/>us-isof-east-1<br/>us-isof-south-1<br/>us-west-1<br/>us-west-2 | AWS Region                                                                                                                                                                                                                                                                                                  |
| **Communications Timeout**       | 30 sec        |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Sets the timeout of the communication between the AWS server and the client                                                                                                                                                                                                                                 |
| Endpoint Override URL            |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Endpoint URL to use instead of the AWS default including scheme, host, port, and path. The AWS libraries select an endpoint URL based on the AWS region, but this property overrides the selected endpoint URL, allowing use with other S3-compatible endpoints.<br/>**Supports Expression Language: true** |
| Proxy Host                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Proxy host name or IP<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                            |
| Proxy Port                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The port number of the proxy host<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                |
| Proxy Username                   |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Username to set when authenticating against proxy<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
| Proxy Password                   |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Password to set when authenticating against proxy<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                               |
| Proxy Configuration Service      |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Specifies the Proxy Configuration Controller Service to proxy network requests. When used, this will override any values specified for Proxy Host, Proxy Port, Proxy Username, and Proxy Password properties.                                                                                               |
| **Use Default Credentials**      | false         | true<br/>false                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc.                                                                                                                                                                 |
| **Bucket**                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The S3 bucket<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                    |
| Object Key                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The key of the S3 object. If none is given the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                                                                                       |
| Version                          |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The Version of the Object to delete<br/>**Supports Expression Language: true**                                                                                                                                                                                                                              |

### Relationships

| Name    | Description                                  |
|---------|----------------------------------------------|
| success | FlowFiles are routed to success relationship |
| failure | FlowFiles are routed to failure relationship |


## EvaluateJsonPath

### Description

Evaluates one or more JsonPath expressions against the content of a FlowFile. The results of those expressions are assigned to FlowFile Attributes or are written to the content of the FlowFile itself, depending on configuration of the Processor. JsonPaths are entered by adding user-defined properties; the name of the property maps to the Attribute Name into which the result will be placed (if the Destination is flowfile-attribute; otherwise, the property name is ignored). The value of the property must be a valid JsonPath expression. A Return Type of 'auto-detect' will make a determination based off the configured destination. When 'Destination' is set to 'flowfile-attribute,' a return type of 'scalar' will be used. When 'Destination' is set to 'flowfile-content,' a return type of 'JSON' will be used.If the JsonPath evaluates to a JSON array or JSON object and the Return Type is set to 'scalar' the FlowFile will be unmodified and will be routed to failure. A Return Type of JSON can return scalar values if the provided JsonPath evaluates to the specified value and will be routed as a match.If Destination is 'flowfile-content' and the JsonPath does not evaluate to a defined path, the FlowFile will be routed to 'unmatched' without having its contents modified. If Destination is 'flowfile-attribute' and the expression matches nothing, attributes will be created with empty strings as the value unless 'Path Not Found Behaviour' is set to 'skip', and the FlowFile will always be routed to 'matched.'

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                          | Default Value      | Allowable Values                        | Description                                                                                                                                                                                                                                                              |
|-------------------------------|--------------------|-----------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Destination**               | flowfile-attribute | flowfile-content<br/>flowfile-attribute | Indicates whether the results of the JsonPath evaluation are written to the FlowFile content or a FlowFile attribute.                                                                                                                                                    |
| **Null Value Representation** | empty string       | empty string<br/>the string 'null'      | Indicates the desired representation of JSON Path expressions resulting in a null value.                                                                                                                                                                                 |
| **Path Not Found Behavior**   | ignore             | warn<br/>ignore<br/>skip                | Indicates how to handle missing JSON path expressions when destination is set to 'flowfile-attribute'. Selecting 'warn' will generate a warning when a JSON path expression is not found. Selecting 'skip' will omit attributes for any unmatched JSON path expressions. |
| **Return Type**               | auto-detect        | auto-detect<br/>json<br/>scalar         | Indicates the desired return type of the JSON Path expressions. Selecting 'auto-detect' will set the return type to 'json' for a Destination of 'flowfile-content', and 'scalar' for a Destination of 'flowfile-attribute'.                                              |

### Dynamic Properties

| Name              | Value                           | Description                                                                                                                                                                                                                                                                                                                                                                                             |
|-------------------|---------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Evaluation Result | JsonPath expression to evaluate | Dynamic property values are evaluated as JsonPaths. In case of 'flowfile-content' destination, only one dynamic property with JsonPath may be specified, in this case the name of the property is ignored. In case of 'flowfile-attribute' destination, the result of the JsonPath evaluation is written to the attribute matching the dynamic property name<br/>**Supports Expression Language: true** |

### Relationships

| Name      | Description                                                                                                                                                          |
|-----------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| failure   | FlowFiles are routed to this relationship when the JsonPath cannot be evaluated against the content of the FlowFile; for instance, if the FlowFile is not valid JSON |
| matched   | FlowFiles are routed to this relationship when the JsonPath is successfully evaluated and the FlowFile is modified as a result                                       |
| unmatched | FlowFiles are routed to this relationship when the JsonPath does not match the content of the FlowFile and the Destination is set to flowfile-content                |


## ExecuteProcess

### Description

Runs an operating system command specified by the user and writes the output of that command to a FlowFile. If the command is expected to be long-running, the Processor can output the partial data on a specified interval. When this option is used, the output is expected to be in textual format, as it typically does not make sense to split binary data on arbitrary time-based intervals. This processor is not available on Windows systems.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                  | Default Value | Allowable Values | Description                                                                                                                      |
|-----------------------|---------------|------------------|----------------------------------------------------------------------------------------------------------------------------------|
| Command               |               |                  | Specifies the command to be executed; if just the name of an executable is provided, it must be in the user's environment PATH.  |
| Command Arguments     |               |                  | The arguments to supply to the executable delimited by white space. White space can be escaped by enclosing it in double-quotes. |
| Working Directory     |               |                  | The directory to use as the current working directory when executing the command                                                 |
| Batch Duration        | 0 sec         |                  | If the process is expected to be long-running and produce textual output, a batch duration can be specified.                     |
| Redirect Error Stream | false         | true<br/>false   | If true will redirect any error stream output of the process to the output stream.                                               |

### Relationships

| Name    | Description                                            |
|---------|--------------------------------------------------------|
| success | All created FlowFiles are routed to this relationship. |


## ExecuteScript

### Description

Executes a script given the flow file and a process session. The script is responsible for handling the incoming flow file (transfer to SUCCESS or remove, e.g.) as well as any flow files created by the script. If the handling is incomplete or incorrect, the session will be rolled back.Scripts must define an onTrigger function which accepts NiFi Context and Property objects. For efficiency, scripts are executed once when the processor is run, then the onTrigger method is called for each incoming flowfile. This enables scripts to keep state if they wish, although there will be a script context per concurrent task of the processor. In order to, e.g., compute an arithmetic sum based on incoming flow file information, set the concurrent tasks to 1.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name              | Default Value | Allowable Values | Description                                                                                            |
|-------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------|
| **Script Engine** | python        | lua<br/>python   | The engine to execute scripts (python, lua)                                                            |
| Script File       |               |                  | Path to script file to execute. Only one of Script File or Script Body may be used                     |
| Script Body       |               |                  | Body of script to execute. Only one of Script File or Script Body may be used                          |
| Module Directory  |               |                  | Comma-separated list of paths to files and/or directories which contain modules required by the script |

### Relationships

| Name     | Description        |
|----------|--------------------|
| success  | Script successes   |
| failure  | Script failures    |
| original | Original flow file |


## ExecuteSQL

### Description

Execute provided SQL query. Query result rows will be outputted as new flow files with attribute keys equal to result column names and values equal to result values. There will be one output FlowFile per result row. This processor can be scheduled to run using the standard timer-based scheduling methods, or it can be triggered by an incoming FlowFile. If it is triggered by an incoming FlowFile, then attributes of that FlowFile will be available when evaluating the query.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value | Allowable Values     | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
|----------------------------|---------------|----------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **DB Controller Service**  |               |                      | Database Controller Service.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| **Output Format**          | JSON-Pretty   | JSON<br/>JSON-Pretty | Set the output format type.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| **Max Rows Per Flow File** | 0             |                      | The maximum number of result rows that will be included in a single FlowFile. This will allow you to break up very large result sets into multiple FlowFiles. If the value specified is zero, then all rows are returned in a single FlowFile.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                         |
| SQL select query           |               |                      | The SQL select query to execute. The query can be empty, a constant value, or built from attributes using Expression Language. If this property is specified, it will be used regardless of the content of incoming flowfiles. If this property is empty, the content of the incoming flow file is expected to contain a valid SQL select query, to be issued by the processor to the database. Note that Expression Language is not evaluated for flow file contents.<br/>**Supports Expression Language: true** |

### Relationships

| Name    | Description                                              |
|---------|----------------------------------------------------------|
| success | Successfully created FlowFile from SQL query result set. |
| failure | Flow files containing malformed sql statements           |


## ExtractText

### Description

Extracts the content of a FlowFile and places it into an attribute.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                             | Default Value | Allowable Values | Description                                                                                                                                                                                            |
|----------------------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Attribute                        |               |                  | Attribute to set from content                                                                                                                                                                          |
| Size Limit                       | 2097152       |                  | Maximum number of bytes to read into the attribute. 0 for no limit. Default is 2MB.                                                                                                                    |
| Regex Mode                       | false         | true<br/>false   | Set this to extract parts of flowfile content using regular experssions in dynamic properties                                                                                                          |
| Include Capture Group 0          | true          | true<br/>false   | Indicates that Capture Group 0 should be included as an attribute. Capture Group 0 represents the entirety of the regular expression match, is typically not used, and could have considerable length. |
| Enable Case-insensitive Matching | false         | true<br/>false   | Indicates that two characters match even if they are in a different case.                                                                                                                              |
| Maximum Capture Group Length     | 1024          |                  | Specifies the maximum number of characters a given capture group value can have. Any characters beyond the max will be truncated.                                                                      |
| Enable repeating capture group   | false         | true<br/>false   | f set to true, every string matching the capture groups will be extracted. Otherwise, if the Regular Expression matches more than once, only the first match will be extracted.                        |

### Relationships

| Name    | Description                            |
|---------|----------------------------------------|
| success | success operational on the flow record |


## FetchAzureBlobStorage

### Description

Retrieves contents of an Azure Storage Blob, writing the contents to the content of the FlowFile

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                   | Default Value   | Allowable Values                                                                  | Description                                                                                                                                                                                                                                                                                                                                                                                                    |
|----------------------------------------|-----------------|-----------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Azure Storage Credentials Service      |                 |                                                                                   | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                                                                                                                                                                                                     |
| Proxy Configuration Service            |                 |                                                                                   | Specifies the Proxy Configuration Controller Service to proxy network requests.                                                                                                                                                                                                                                                                                                                                |
| **Container Name**                     |                 |                                                                                   | Name of the Azure Storage container. In case of PutAzureBlobStorage processor, container can be created if it does not exist.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                       |
| Storage Account Name                   |                 |                                                                                   | The storage account name.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                           |
| Storage Account Key                    |                 |                                                                                   | The storage account key. This is an admin-like password providing access to every container in this account. It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies if Credential Configuration Strategy is set to From Properties. If set, SAS Token must be empty.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**        |
| SAS Token                              |                 |                                                                                   | Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Credential Configuration Strategy is set to From Properties. If set, Storage Account Key must be empty.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                         |
| Common Storage Account Endpoint Suffix |                 |                                                                                   | Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a different suffix in certain circumstances (like Azure Stack or non-public Azure regions).<br/>**Supports Expression Language: true**                                                                                                                                                                    |
| Connection String                      |                 |                                                                                   | Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Credential Configuration Strategy is set to From Properties.<br/>**Supports Expression Language: true**                                                                                                                                                                                      |
| **Credential Configuration Strategy**  | From Properties | From Properties<br/>Default Credential<br/>Managed Identity<br/>Workload Identity | The strategy to use for credential configuration. If set to From Properties, the credentials are parsed from the SAS Token, Storage Account Key, and Connection String properties. In other cases, the selected Azure identity source is used.                                                                                                                                                                 |
| Managed Identity Client ID             |                 |                                                                                   | Client ID of the managed identity. The property is required when User Assigned Managed Identity is used for authentication and multiple user-assigned identities are added to the resource. It must be empty in case of System Assigned Managed Identity and can also be left empty if only one user-assigned identity is present.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true** |
| Blob                                   |                 |                                                                                   | The filename of the blob. If left empty the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                             |
| Range Start                            |                 |                                                                                   | The byte position at which to start reading from the blob. An empty value or a value of zero will start reading at the beginning of the blob.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                       |
| Range Length                           |                 |                                                                                   | The number of bytes to download from the blob, starting from the Range Start. An empty value or a value that extends beyond the end of the blob will read to the end of the blob.<br/>**Supports Expression Language: true**                                                                                                                                                                                   |

### Relationships

| Name    | Description                                                             |
|---------|-------------------------------------------------------------------------|
| success | All successfully processed FlowFiles are routed to this relationship    |
| failure | Unsuccessful operations will be transferred to the failure relationship |


## FetchAzureDataLakeStorage

### Description

Fetch the provided file from Azure Data Lake Storage Gen 2

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                              | Default Value | Allowable Values | Description                                                                                                                                                                                                                             |
|-----------------------------------|---------------|------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Azure Storage Credentials Service |               |                  | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                              |
| Proxy Configuration Service       |               |                  | Specifies the Proxy Configuration Controller Service to proxy network requests.                                                                                                                                                         |
| **Filesystem Name**               |               |                  | Name of the Azure Storage File System. It is assumed to be already existing.<br/>**Supports Expression Language: true**                                                                                                                 |
| Directory Name                    |               |                  | Name of the Azure Storage Directory. The Directory Name cannot contain a leading '/'. If left empty it designates the root directory. The directory will be created if not already existing.<br/>**Supports Expression Language: true** |
| File Name                         |               |                  | The filename in Azure Storage. If left empty the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                 |
| Range Start                       |               |                  | The byte position at which to start reading from the object. An empty value or a value of zero will start reading at the beginning of the object.<br/>**Supports Expression Language: true**                                            |
| Range Length                      |               |                  | The number of bytes to download from the object, starting from the Range Start. An empty value or a value that extends beyond the end of the object will read to the end of the object.<br/>**Supports Expression Language: true**      |
| Number of Retries                 | 0             |                  | The number of automatic retries to perform if the download fails.<br/>**Supports Expression Language: true**                                                                                                                            |

### Relationships

| Name    | Description                                                                                       |
|---------|---------------------------------------------------------------------------------------------------|
| success | Files that have been successfully fetched from Azure storage are transferred to this relationship |
| failure | In case of fetch failure flowfiles are transferred to this relationship                           |


## FetchFile

### Description

Reads the contents of a file from disk and streams it into the contents of an incoming FlowFile. Once this is done, the file is optionally moved elsewhere or deleted to help keep the file system organized.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                 | Default Value | Allowable Values                                                 | Description                                                                                                                                                                                                                                                              |
|--------------------------------------|---------------|------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| File to Fetch                        |               |                                                                  | The fully-qualified filename of the file to fetch from the file system. If not defined the default ${absolute.path}/${filename} path is used.<br/>**Supports Expression Language: true**                                                                                 |
| **Completion Strategy**              | None          | None<br/>Move File<br/>Delete File                               | Specifies what to do with the original file on the file system once it has been pulled into MiNiFi                                                                                                                                                                       |
| Move Destination Directory           |               |                                                                  | The directory to move the original file to once it has been fetched from the file system. This property is ignored unless the Completion Strategy is set to "Move File". If the directory does not exist, it will be created.<br/>**Supports Expression Language: true** |
| **Move Conflict Strategy**           | Rename        | Rename<br/>Replace File<br/>Keep Existing<br/>Fail               | If Completion Strategy is set to Move File and a file already exists in the destination directory with the same name, this property specifies how that naming conflict should be resolved                                                                                |
| **Log level when file not found**    | ERROR         | TRACE<br/>DEBUG<br/>INFO<br/>WARN<br/>ERROR<br/>CRITICAL<br/>OFF | Log level to use in case the file does not exist when the processor is triggered                                                                                                                                                                                         |
| **Log level when permission denied** | ERROR         | TRACE<br/>DEBUG<br/>INFO<br/>WARN<br/>ERROR<br/>CRITICAL<br/>OFF | Log level to use in case agent does not have sufficient permissions to read the file                                                                                                                                                                                     |

### Relationships

| Name              | Description                                                                                                                                                                       |
|-------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| success           | Any FlowFile that is successfully fetched from the file system will be transferred to this Relationship.                                                                          |
| not.found         | Any FlowFile that could not be fetched from the file system because the file could not be found will be transferred to this Relationship.                                         |
| permission.denied | Any FlowFile that could not be fetched from the file system due to the user running MiNiFi not having sufficient permissions will be transferred to this Relationship.            |
| failure           | Any FlowFile that could not be fetched from the file system for any reason other than insufficient permissions or the file not existing will be transferred to this Relationship. |


## FetchGCSObject

### Description

Fetches a file from a Google Cloud Bucket. Designed to be used in tandem with ListGCSBucket.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                 | Default Value | Allowable Values | Description                                                                                                                                                         |
|--------------------------------------|---------------|------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **GCP Credentials Provider Service** |               |                  | The Controller Service used to obtain Google Cloud Platform credentials. Should be the name of a GCPCredentialsControllerService.                                   |
| **Number of retries**                | 6             |                  | How many retry attempts should be made before routing to the failure relationship.                                                                                  |
| Endpoint Override URL                |               |                  | Overrides the default Google Cloud Storage endpoints<br/>**Supports Expression Language: true**                                                                     |
| Bucket                               | ${gcs.bucket} |                  | Bucket of the object.<br/>**Supports Expression Language: true**                                                                                                    |
| Key                                  | ${filename}   |                  | Name of the object.<br/>**Supports Expression Language: true**                                                                                                      |
| Server Side Encryption Key           |               |                  | The AES256 Encryption Key (encoded in base64) for server-side decryption of the object.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true** |
| Object Generation                    |               |                  | The generation of the Object to download. If left empty, then it will download the latest generation.<br/>**Supports Expression Language: true**                    |

### Relationships

| Name    | Description                                                                                  |
|---------|----------------------------------------------------------------------------------------------|
| success | FlowFiles are routed to this relationship after a successful Google Cloud Storage operation. |
| failure | FlowFiles are routed to this relationship if the Google Cloud Storage operation fails.       |

### Output Attributes

| Attribute          | Relationship | Description                                             |
|--------------------|--------------|---------------------------------------------------------|
| gcs.status.message | failure      | The status message received from google cloud.          |
| gcs.error.reason   | failure      | The description of the error occurred during operation. |
| gcs.error.domain   | failure      | The domain of the error occurred during operation.      |


## FetchModbusTcp

### Description

Processor able to read data from industrial PLCs using Modbus TCP/IP

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                           | Default Value | Allowable Values | Description                                                                                                                                           |
|--------------------------------|---------------|------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Hostname**                   |               |                  | The ip address or hostname of the destination.<br/>**Supports Expression Language: true**                                                             |
| **Port**                       | 502           |                  | The port or service on the destination.<br/>**Supports Expression Language: true**                                                                    |
| **Unit Identifier**            | 1             |                  | Unit identifier<br/>**Supports Expression Language: true**                                                                                            |
| **Idle Connection Expiration** | 15 seconds    |                  | The amount of time a connection should be held open without being used before closing the connection. A value of 0 seconds will disable this feature. |
| **Connection Per FlowFile**    | false         | true<br/>false   | Specifies whether to send each FlowFile's content on an individual connection.                                                                        |
| **Timeout**                    | 15 seconds    |                  | The timeout for connecting to and communicating with the destination.                                                                                 |
| SSL Context Service            |               |                  | The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be sent over a secure connection.             |
| **Record Set Writer**          |               |                  | Specifies the Controller Service to use for writing results to a FlowFile.                                                                            |

### Relationships

| Name    | Description                  |
|---------|------------------------------|
| success | Successfully processed       |
| failure | An error occurred processing |


## FetchOPCProcessor

### Description

Fetches OPC-UA node

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                            | Default Value | Allowable Values         | Description                                                                                                                                                                                                                                                                                                        |
|---------------------------------|---------------|--------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **OPC server endpoint**         |               |                          | Specifies the address, port and relative path of an OPC endpoint                                                                                                                                                                                                                                                   |
| Application URI                 |               |                          | Application URI of the client in the format 'urn:unconfigured:application'. Mandatory, if using Secure Channel and must match the URI included in the certificate's Subject Alternative Names.                                                                                                                     |
| Username                        |               |                          | Username to log in with.                                                                                                                                                                                                                                                                                           |
| Password                        |               |                          | Password to log in with.<br/>**Sensitive Property: true**                                                                                                                                                                                                                                                          |
| Certificate path                |               |                          | Path to the DER-encoded cert file                                                                                                                                                                                                                                                                                  |
| Key path                        |               |                          | Path to the DER-encoded key file                                                                                                                                                                                                                                                                                   |
| Trusted server certificate path |               |                          | Comma separated list of paths to the DER-encoded trusted server certificates                                                                                                                                                                                                                                       |
| Path reference types            |               |                          | Specify the reference types between nodes in the path if Path Node ID type is used. If not provided, all reference types are assumed to be Organizes. The format is 'referenceType1/referenceType2/.../referenceTypeN' and the supported reference types are Organizes, HasComponent, HasProperty, and HasSubtype. |
| **Node ID type**                |               | Path<br/>Int<br/>String  | Specifies the type of the provided node ID                                                                                                                                                                                                                                                                         |
| **Node ID**                     |               |                          | Specifies the ID of the root node to traverse. In case of a Path Node ID Type, the path should be provided in the format of 'path/to/node'.                                                                                                                                                                        |
| **Namespace index**             | 0             |                          | The index of the namespace.                                                                                                                                                                                                                                                                                        |
| **Max depth**                   | 0             |                          | Specifiec the max depth of browsing. 0 means unlimited.                                                                                                                                                                                                                                                            |
| **Lazy mode**                   | Off           | On<br/>New Value<br/>Off | Only creates flowfiles from nodes with new timestamp from the server. If set to 'New Value', it will only create flowfiles if the value of the node data has changed since the last fetch, the timestamp is ignored.                                                                                               |

### Relationships

| Name    | Description                                                              |
|---------|--------------------------------------------------------------------------|
| success | Successfully retrieved OPC-UA nodes                                      |
| failure | Retrieved OPC-UA nodes where value cannot be extracted (only if enabled) |

### Output Attributes

| Attribute       | Relationship | Description                                                                            |
|-----------------|--------------|----------------------------------------------------------------------------------------|
| NodeID          | success      | ID of the node.                                                                        |
| NodeID type     | success      | Type of the node ID.                                                                   |
| Browsename      | success      | The browse name of the node.                                                           |
| Full path       | success      | The full path of the node.                                                             |
| Sourcetimestamp | success      | The timestamp of when the node was created in the server as 'MM-dd-yyyy HH:mm:ss.mmm'. |
| Typename        | success      | The type name of the node data.                                                        |
| Datasize        | success      | The size of the node data.                                                             |


## FetchS3Object

### Description

This Processor retrieves the contents of an S3 Object and writes it to the content of a FlowFile.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                             | Default Value | Allowable Values                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         | Description                                                                                                                                                                                                                                                                                                 |
|----------------------------------|---------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Access Key                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | AWS account access key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                           |
| Secret Key                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | AWS account secret key<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                          |
| Credentials File                 |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey                                                                                                                                                                                 |
| AWS Credentials Provider service |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.                                                                                                                                                                                                         |
| **Region**                       | us-west-2     | af-south-1<br/>ap-east-1<br/>ap-northeast-1<br/>ap-northeast-2<br/>ap-northeast-3<br/>ap-south-1<br/>ap-south-2<br/>ap-southeast-1<br/>ap-southeast-2<br/>ap-southeast-3<br/>ap-southeast-4<br/>ap-southeast-5<br/>ap-southeast-7<br/>ca-central-1<br/>ca-west-1<br/>cn-north-1<br/>cn-northwest-1<br/>eu-central-1<br/>eu-central-2<br/>eu-isoe-west-1<br/>eu-north-1<br/>eu-south-1<br/>eu-south-2<br/>eu-west-1<br/>eu-west-2<br/>eu-west-3<br/>il-central-1<br/>me-central-1<br/>me-south-1<br/>mx-central-1<br/>sa-east-1<br/>us-east-1<br/>us-east-2<br/>us-gov-east-1<br/>us-gov-west-1<br/>us-iso-east-1<br/>us-iso-west-1<br/>us-isob-east-1<br/>us-isof-east-1<br/>us-isof-south-1<br/>us-west-1<br/>us-west-2 | AWS Region                                                                                                                                                                                                                                                                                                  |
| **Communications Timeout**       | 30 sec        |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Sets the timeout of the communication between the AWS server and the client                                                                                                                                                                                                                                 |
| Endpoint Override URL            |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Endpoint URL to use instead of the AWS default including scheme, host, port, and path. The AWS libraries select an endpoint URL based on the AWS region, but this property overrides the selected endpoint URL, allowing use with other S3-compatible endpoints.<br/>**Supports Expression Language: true** |
| Proxy Host                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Proxy host name or IP<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                            |
| Proxy Port                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The port number of the proxy host<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                |
| Proxy Username                   |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Username to set when authenticating against proxy<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
| Proxy Password                   |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Password to set when authenticating against proxy<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                               |
| Proxy Configuration Service      |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Specifies the Proxy Configuration Controller Service to proxy network requests. When used, this will override any values specified for Proxy Host, Proxy Port, Proxy Username, and Proxy Password properties.                                                                                               |
| **Use Default Credentials**      | false         | true<br/>false                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc.                                                                                                                                                                 |
| **Bucket**                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The S3 bucket<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                    |
| Object Key                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The key of the S3 object. If none is given the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                                                                                       |
| Version                          |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The Version of the Object to download<br/>**Supports Expression Language: true**                                                                                                                                                                                                                            |
| **Requester Pays**               | false         | true<br/>false                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | If true, indicates that the requester consents to pay any charges associated with retrieving objects from the S3 bucket. This sets the 'x-amz-request-payer' header to 'requester'.                                                                                                                         |

### Relationships

| Name    | Description                                  |
|---------|----------------------------------------------|
| success | FlowFiles are routed to success relationship |
| failure | FlowFiles are routed to failure relationship |


## FetchSFTP

### Description

Fetches the content of a file from a remote SFTP server and overwrites the contents of an incoming FlowFile with the content of the remote file.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                           | Default Value | Allowable Values                   | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
|--------------------------------|---------------|------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Hostname**                   |               |                                    | The fully qualified hostname or IP address of the remote system<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| **Port**                       |               |                                    | The port that the remote system is listening on for file transfers<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| **Username**                   |               |                                    | Username<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| Password                       |               |                                    | Password for the user account<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| Private Key Path               |               |                                    | The fully qualified path to the Private Key file<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| Private Key Passphrase         |               |                                    | Password for the private key<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| **Strict Host Key Checking**   | false         | true<br/>false                     | Indicates whether or not strict enforcement of hosts keys should be applied                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| Host Key File                  |               |                                    | If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| **Connection Timeout**         | 30 sec        |                                    | Amount of time to wait before timing out while creating a connection                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| **Data Timeout**               | 30 sec        |                                    | When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| **Send Keep Alive On Timeout** | true          | true<br/>false                     | Indicates whether or not to send a single Keep Alive message when SSH socket times out                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| Proxy Type                     | DIRECT        | DIRECT<br/>HTTP<br/>SOCKS          | Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. Supported proxies: HTTP + AuthN, SOCKS + AuthN                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| Proxy Host                     |               |                                    | The fully qualified hostname or IP address of the proxy server<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| Proxy Port                     |               |                                    | The port of the proxy server<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| Http Proxy Username            |               |                                    | Http Proxy Username<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| Http Proxy Password            |               |                                    | Http Proxy Password<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| **Remote File**                |               |                                    | The fully qualified filename on the remote system<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| **Completion Strategy**        | None          | None<br/>Move File<br/>Delete File | Specifies what to do with the original file on the server once it has been pulled into NiFi. If the Completion Strategy fails, a warning will be logged but the data will still be transferred.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Move Destination Directory     |               |                                    | The directory on the remote server to move the original file to once it has been ingested into NiFi. This property is ignored unless the Completion Strategy is set to 'Move File'. The specified directory must already exist on the remote system if 'Create Directory' is disabled, or the rename will fail.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| **Create Directory**           | false         | true<br/>false                     | Specifies whether or not the remote directory should be created if it does not exist.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| Disable Directory Listing      | false         | true<br/>false                     | Control how 'Move Destination Directory' is created when 'Completion Strategy' is 'Move File' and 'Create Directory' is enabled. If set to 'true', directory listing is not performed prior to create missing directories. By default, this processor executes a directory listing command to see target directory existence before creating missing directories. However, there are situations that you might need to disable the directory listing such as the following. Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, then an error is returned because the directory already exists. |
| **Use Compression**            | false         | true<br/>false                     | Indicates whether or not ZLIB compression should be used when transferring files                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |

### Relationships

| Name              | Description                                                                                                                             |
|-------------------|-----------------------------------------------------------------------------------------------------------------------------------------|
| success           | All FlowFiles that are received are routed to success                                                                                   |
| comms.failure     | Any FlowFile that could not be fetched from the remote server due to a communications failure will be transferred to this Relationship. |
| not.found         | Any FlowFile for which we receive a 'Not Found' message from the remote server will be transferred to this Relationship.                |
| permission.denied | Any FlowFile that could not be fetched from the remote server due to insufficient permissions will be transferred to this Relationship. |


## FetchSmb

### Description

Fetches files from a SMB Share. Designed to be used in tandem with ListSmb.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                  | Default Value | Allowable Values | Description                                                                                                                                                                                          |
|---------------------------------------|---------------|------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **SMB Connection Controller Service** |               |                  | Specifies the SMB connection controller service to use for connecting to the SMB server. If the SMB share is auto-mounted to a drive letter, its recommended to use FetchFile instead.               |
| Input Directory                       |               |                  | The full path of the file to be retrieved from the remote server. If left empty, the path and filename attributes of the incoming flow file will be used.<br/>**Supports Expression Language: true** |

### Relationships

| Name    | Description                                                        |
|---------|--------------------------------------------------------------------|
| success | A flowfile will be routed here for each successfully fetched file. |
| failure | A flowfile will be routed here when failed to fetch its content.   |

### Output Attributes

| Attribute     | Relationship | Description                                                       |
|---------------|--------------|-------------------------------------------------------------------|
| error.code    | failure      | The error code returned by SMB when the fetch of a file fails.    |
| error.message | failure      | The error message returned by SMB when the fetch of a file fails. |


## FocusArchiveEntry

### Description

Allows manipulation of entries within an archive (e.g. TAR) by focusing on one entry within the archive at a time. When an archive entry is focused, that entry is treated as the content of the FlowFile and may be manipulated independently of the rest of the archive. To restore the FlowFile to its original state, use UnfocusArchiveEntry.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description                                                           |
|------|---------------|------------------|-----------------------------------------------------------------------|
| Path |               |                  | The path within the archive to focus ("/" to focus the total archive) |

### Relationships

| Name    | Description                            |
|---------|----------------------------------------|
| success | success operational on the flow record |


## GenerateFlowFile

### Description

This processor creates FlowFiles with random data or custom content. GenerateFlowFile is useful for load testing, configuration, and simulation.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name             | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                                                                                      |
|------------------|---------------|------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| File Size        | 1 kB          |                  | The size of the file that will be used                                                                                                                                                                                                                                                                                           |
| Batch Size       | 1             |                  | The number of FlowFiles to be transferred in each invocation                                                                                                                                                                                                                                                                     |
| Data Format      | Binary        | Text<br/>Binary  | Specifies whether the data should be Text or Binary                                                                                                                                                                                                                                                                              |
| Unique FlowFiles | true          | true<br/>false   | If true, each FlowFile that is generated will be unique. If false, a random value will be generated and all FlowFiles will get the same content but this offers much higher throughput (but see the description of Custom Text for special non-random use cases)                                                                 |
| Custom Text      |               |                  | If Data Format is text and if Unique FlowFiles is false, then this custom text will be used as content of the generated FlowFiles and the File Size will be ignored. Finally, if Expression Language is used, evaluation will be performed only once per batch of generated FlowFiles<br/>**Supports Expression Language: true** |

### Relationships

| Name    | Description                            |
|---------|----------------------------------------|
| success | success operational on the flow record |


## GetCouchbaseKey

### Description

Get a document from Couchbase Server via Key/Value access. The ID of the document to fetch may be supplied by setting the <Document Id> property. NOTE: if the Document Id property is not set, the contents of the FlowFile will be read to determine the Document Id, which means that the contents of the entire FlowFile will be buffered in memory.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                     | Default Value | Allowable Values           | Description                                                                                                                                                                                                                                    |
|------------------------------------------|---------------|----------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Couchbase Cluster Controller Service** |               |                            | A Couchbase Cluster Controller Service which manages connections to a Couchbase cluster.                                                                                                                                                       |
| **Bucket Name**                          | default       |                            | The name of bucket to access.<br/>**Supports Expression Language: true**                                                                                                                                                                       |
| Scope Name                               |               |                            | Scope to use inside the bucket. If not specified, the _default scope is used.<br/>**Supports Expression Language: true**                                                                                                                       |
| Collection Name                          |               |                            | Collection to use inside the bucket scope. If not specified, the _default collection is used.<br/>**Supports Expression Language: true**                                                                                                       |
| **Document Type**                        | Json          | Json<br/>Binary<br/>String | Content type of the retrieved value.                                                                                                                                                                                                           |
| Document Id                              |               |                            | A static, fixed Couchbase document id, or an expression to construct the Couchbase document id.<br/>**Supports Expression Language: true**                                                                                                     |
| Put Value to Attribute                   |               |                            | If set, the retrieved value will be put into an attribute of the FlowFile instead of a the content of the FlowFile. The attribute key to put to is determined by evaluating value of this property.<br/>**Supports Expression Language: true** |

### Relationships

| Name     | Description                                                                                                                                                         |
|----------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| success  | Values retrieved from Couchbase Server are written as outgoing FlowFiles content or put into an attribute of the incoming FlowFile and routed to this relationship. |
| failure  | All FlowFiles failed to fetch from Couchbase Server and not retry-able are routed to this relationship.                                                             |
| retry    | All FlowFiles failed to fetch from Couchbase Server but can be retried are routed to this relationship.                                                             |
| original | The original input FlowFile is routed to this relationship when the value is retrieved from Couchbase Server and routed to 'success'.                               |

### Output Attributes

| Attribute            | Relationship | Description                           |
|----------------------|--------------|---------------------------------------|
| couchbase.bucket     | success      | Bucket where the document was stored. |
| couchbase.doc.id     | success      | Id of the document.                   |
| couchbase.doc.cas    | success      | CAS of the document.                  |
| couchbase.doc.expiry | success      | Expiration of the document.           |


## GetFile

### Description

Creates FlowFiles from files in a directory. MiNiFi will ignore files for which it doesn't have read permissions.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                   | Default Value | Allowable Values | Description                                                                                                                                                |
|------------------------|---------------|------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Input Directory**    |               |                  | The input directory from which to pull files<br/>**Supports Expression Language: true**                                                                    |
| Recurse Subdirectories | true          | true<br/>false   | Indicates whether or not to pull files from subdirectories                                                                                                 |
| Keep Source File       | false         | true<br/>false   | If true, the file is not deleted after it has been copied to the Content Repository                                                                        |
| Minimum File Age       | 0 sec         |                  | The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored |
| Maximum File Age       | 0 sec         |                  | The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored   |
| Minimum File Size      | 0 B           |                  | The minimum size that a file can be in order to be pulled                                                                                                  |
| Maximum File Size      | 0 B           |                  | The maximum size that a file can be in order to be pulled                                                                                                  |
| Ignore Hidden Files    | true          | true<br/>false   | Indicates whether or not hidden files should be ignored                                                                                                    |
| Polling Interval       | 0 sec         |                  | Indicates how long to wait before performing a directory listing                                                                                           |
| Batch Size             | 10            |                  | The maximum number of files to pull in each iteration                                                                                                      |
| File Filter            | .*            |                  | Only files whose names match the given regular expression will be picked up                                                                                |

### Relationships

| Name    | Description                     |
|---------|---------------------------------|
| success | All files are routed to success |


## GetTCP

### Description

Establishes a TCP Server that defines and retrieves one or more byte messages from clients

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                          | Default Value | Allowable Values | Description                                                                                                                                                                                      |
|-------------------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Endpoint List**             |               |                  | A comma delimited list of the endpoints to connect to. The format should be <server_address>:<port>.                                                                                             |
| SSL Context Service           |               |                  | SSL Context Service Name                                                                                                                                                                         |
| Message Delimiter             | \n            |                  | Character that denotes the end of the message.                                                                                                                                                   |
| **Max Size of Message Queue** | 10000         |                  | Maximum number of messages allowed to be buffered before processing them when the processor is triggered. If the buffer is full, the message is ignored. If set to zero the buffer is unlimited. |
| Maximum Message Size          |               |                  | Optional size of the buffer to receive data in.                                                                                                                                                  |
| **Max Batch Size**            | 500           |                  | The maximum number of messages to process at a time.                                                                                                                                             |
| **Timeout**                   | 1s            |                  | The timeout for connecting to and communicating with the destination.<br/>**Supports Expression Language: true**                                                                                 |
| **Reconnection Interval**     | 1 min         |                  | The duration to wait before attempting to reconnect to the endpoints.<br/>**Supports Expression Language: true**                                                                                 |

### Relationships

| Name    | Description                                                                                 |
|---------|---------------------------------------------------------------------------------------------|
| success | All files are routed to success                                                             |
| partial | Indicates an incomplete message as a result of encountering the end of message byte trigger |

### Output Attributes

| Attribute       | Relationship     | Description                                              |
|-----------------|------------------|----------------------------------------------------------|
| source.endpoint | success, partial | The address of the source endpoint the message came from |


## HashContent

### Description

HashContent calculates the checksum of the content of the flowfile and adds it as an attribute. Configuration options exist to select hashing algorithm and set the name of the attribute.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name               | Default Value | Allowable Values | Description                                            |
|--------------------|---------------|------------------|--------------------------------------------------------|
| Hash Attribute     | Checksum      |                  | Attribute to store checksum to                         |
| **Hash Algorithm** | SHA256        |                  | Name of the algorithm used to generate checksum        |
| Fail on empty      | false         | true<br/>false   | Route to failure relationship in case of empty content |

### Relationships

| Name    | Description                            |
|---------|----------------------------------------|
| success | success operational on the flow record |
| failure | failure operational on the flow record |


## InvokeHTTP

### Description

An HTTP client processor which can interact with a configurable HTTP Endpoint. The destination URL and HTTP Method are configurable. FlowFile attributes are converted to HTTP headers and the FlowFile contents are included as the body of the request (if the HTTP Method is PUT, POST or PATCH).

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                            | Default Value            | Allowable Values                                                                     | Description                                                                                                                                                                                                                                                                                                     |
|-------------------------------------------------|--------------------------|--------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| HTTP Method                                     | GET                      | GET<br/>POST<br/>PUT<br/>PATCH<br/>DELETE<br/>CONNECT<br/>HEAD<br/>OPTIONS<br/>TRACE | HTTP request method. Methods other than POST, PUT and PATCH will be sent without a message body.                                                                                                                                                                                                                |
| Remote URL                                      |                          |                                                                                      | Remote URL which will be connected to, including scheme, host, port, path.<br/>**Supports Expression Language: true**                                                                                                                                                                                           |
| Connection Timeout                              | 5 s                      |                                                                                      | Max wait time for connection to remote service                                                                                                                                                                                                                                                                  |
| Read Timeout                                    | 15 s                     |                                                                                      | Max wait time for response from remote service                                                                                                                                                                                                                                                                  |
| Include Date Header                             | true                     | true<br/>false                                                                       | Include an RFC-2616 Date header in the request.                                                                                                                                                                                                                                                                 |
| Follow Redirects                                | true                     | true<br/>false                                                                       | Follow HTTP redirects issued by remote server.                                                                                                                                                                                                                                                                  |
| Attributes to Send                              |                          |                                                                                      | Regular expression that defines which attributes to send as HTTP headers in the request. If not defined, no attributes are sent as headers.                                                                                                                                                                     |
| SSL Context Service                             |                          |                                                                                      | The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.                                                                                                                                                                                                         |
| Proxy Host                                      |                          |                                                                                      | The fully qualified hostname or IP address of the proxy server                                                                                                                                                                                                                                                  |
| Proxy Port                                      |                          |                                                                                      | The port of the proxy server                                                                                                                                                                                                                                                                                    |
| invokehttp-proxy-username                       |                          |                                                                                      | Username to set when authenticating against proxy                                                                                                                                                                                                                                                               |
| invokehttp-proxy-password                       |                          |                                                                                      | Password to set when authenticating against proxy<br/>**Sensitive Property: true**                                                                                                                                                                                                                              |
| Content-type                                    | application/octet-stream |                                                                                      | The Content-Type to specify for when content is being transmitted through a PUT, POST or PATCH. In the case of an empty value after evaluating an expression language expression, Content-Type defaults to                                                                                                      |
| send-message-body                               | true                     | true<br/>false                                                                       | DEPRECATED. Only kept for backwards compatibility, no functionality is included.                                                                                                                                                                                                                                |
| Send Message Body                               | true                     | true<br/>false                                                                       | If true, sends the HTTP message body on POST/PUT/PATCH requests (default). If false, suppresses the message body and content-type header for these requests.                                                                                                                                                    |
| Use Chunked Encoding                            | false                    | true<br/>false                                                                       | When POST'ing, PUT'ing or PATCH'ing content set this property to true in order to not pass the 'Content-length' header and instead send 'Transfer-Encoding' with a value of 'chunked'. This will enable the data transfer mechanism which was introduced in HTTP 1.1 to pass data of unknown lengths in chunks. |
| Disable Peer Verification                       | false                    | true<br/>false                                                                       | DEPRECATED. The value is ignored, peer and host verification are always performed when using SSL/TLS.                                                                                                                                                                                                           |
| Put Response Body in Attribute                  |                          |                                                                                      | If set, the response body received back will be put into an attribute of the original FlowFile instead of a separate FlowFile. The attribute key to put to is determined by evaluating value of this property.                                                                                                  |
| Always Output Response                          | false                    | true<br/>false                                                                       | Will force a response FlowFile to be generated and routed to the 'Response' relationship regardless of what the server status code received is                                                                                                                                                                  |
| Penalize on "No Retry"                          | false                    | true<br/>false                                                                       | Enabling this property will penalize FlowFiles that are routed to the "No Retry" relationship.                                                                                                                                                                                                                  |
| **Invalid HTTP Header Field Handling Strategy** | transform                | fail<br/>transform<br/>drop                                                          | Indicates what should happen when an attribute's name is not a valid HTTP header field name. Options: transform - invalid characters are replaced, fail - flow file is transferred to failure, drop - drops invalid attributes from HTTP message                                                                |
| Upload Speed Limit                              |                          |                                                                                      | Maximum upload speed, e.g. '500 KB/s'. Leave this empty if you want no limit.                                                                                                                                                                                                                                   |
| Download Speed Limit                            |                          |                                                                                      | Maximum download speed,e.g. '500 KB/s'. Leave this empty if you want no limit.                                                                                                                                                                                                                                  |

### Relationships

| Name     | Description                                                                                                                                                                                                      |
|----------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| success  | The original FlowFile will be routed upon success (2xx status codes). It will have new attributes detailing the success of the request.                                                                          |
| response | A Response FlowFile will be routed upon success (2xx status codes). If the 'Always Output Response' property is true then the response will be sent to this relationship regardless of the status code received. |
| retry    | The original FlowFile will be routed on any status code that can be retried (5xx status codes). It will have new attributes detailing the request.                                                               |
| no retry | The original FlowFile will be routed on any status code that should NOT be retried (1xx, 3xx, 4xx status codes). It will have new attributes detailing the request.                                              |
| failure  | The original FlowFile will be routed on any type of connection failure, timeout or general exception. It will have new attributes detailing the request.                                                         |

### Output Attributes

| Attribute                 | Relationship                       | Description                                                    |
|---------------------------|------------------------------------|----------------------------------------------------------------|
| invokehttp.status.code    | success, response, retry, no retry | The status code that is returned                               |
| invokehttp.status.message | success, response, retry, no retry | The status message that is returned                            |
| invokehttp.request.url    | success, response, retry, no retry | The original request URL                                       |
| invokehttp.tx.id          | success, response, retry, no retry | The transaction ID that is returned after reading the response |


## JoltTransformJSON

### Description

Applies a list of Jolt specifications to the flowfile JSON payload. A new FlowFile is created with transformed content and is routed to the 'success' relationship. If the JSON transform fails, the original FlowFile is routed to the 'failure' relationship.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                        | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                                           |
|-----------------------------|---------------|------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Jolt Transformation DSL** | Shift         | Shift            | Specifies the Jolt Transformation that should be used with the provided specification.                                                                                                                                                                                                |
| **Jolt Specification**      |               |                  | Jolt Specification for transformation of JSON data. The value for this property may be the text of a Jolt specification or the path to a file containing a Jolt specification. 'Jolt Specification' must be set, or the value is ignored if the Jolt Sort Transformation is selected. |

### Relationships

| Name    | Description                                                                                                                         |
|---------|-------------------------------------------------------------------------------------------------------------------------------------|
| success | The FlowFile with transformed content will be routed to this relationship                                                           |
| failure | If a FlowFile fails processing for any reason (for example, the FlowFile is not valid JSON), it will be routed to this relationship |


## ListAzureBlobStorage

### Description

Lists blobs in an Azure Storage container. Listing details are attached to an empty FlowFile for use with FetchAzureBlobStorage.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                   | Default Value   | Allowable Values                                                                  | Description                                                                                                                                                                                                                                                                                                                                                                                                    |
|----------------------------------------|-----------------|-----------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Azure Storage Credentials Service      |                 |                                                                                   | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                                                                                                                                                                                                     |
| Proxy Configuration Service            |                 |                                                                                   | Specifies the Proxy Configuration Controller Service to proxy network requests.                                                                                                                                                                                                                                                                                                                                |
| **Container Name**                     |                 |                                                                                   | Name of the Azure Storage container. In case of PutAzureBlobStorage processor, container can be created if it does not exist.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                       |
| Storage Account Name                   |                 |                                                                                   | The storage account name.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                           |
| Storage Account Key                    |                 |                                                                                   | The storage account key. This is an admin-like password providing access to every container in this account. It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies if Credential Configuration Strategy is set to From Properties. If set, SAS Token must be empty.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**        |
| SAS Token                              |                 |                                                                                   | Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Credential Configuration Strategy is set to From Properties. If set, Storage Account Key must be empty.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                         |
| Common Storage Account Endpoint Suffix |                 |                                                                                   | Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a different suffix in certain circumstances (like Azure Stack or non-public Azure regions).<br/>**Supports Expression Language: true**                                                                                                                                                                    |
| Connection String                      |                 |                                                                                   | Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Credential Configuration Strategy is set to From Properties.<br/>**Supports Expression Language: true**                                                                                                                                                                                      |
| **Credential Configuration Strategy**  | From Properties | From Properties<br/>Default Credential<br/>Managed Identity<br/>Workload Identity | The strategy to use for credential configuration. If set to From Properties, the credentials are parsed from the SAS Token, Storage Account Key, and Connection String properties. In other cases, the selected Azure identity source is used.                                                                                                                                                                 |
| Managed Identity Client ID             |                 |                                                                                   | Client ID of the managed identity. The property is required when User Assigned Managed Identity is used for authentication and multiple user-assigned identities are added to the resource. It must be empty in case of System Assigned Managed Identity and can also be left empty if only one user-assigned identity is present.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true** |
| **Listing Strategy**                   | timestamps      | none<br/>timestamps                                                               | Specify how to determine new/updated entities. If 'timestamps' is selected it tracks the latest timestamp of listed entity to determine new/updated entities. If 'none' is selected it lists an entity without any tracking, the same entity will be listed each time on executing this processor.                                                                                                             |
| Prefix                                 |                 |                                                                                   | Search prefix for listing<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                           |

### Relationships

| Name    | Description                                           |
|---------|-------------------------------------------------------|
| success | All FlowFiles that are received are routed to success |


## ListAzureDataLakeStorage

### Description

Lists directory in an Azure Data Lake Storage Gen 2 filesystem

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                              | Default Value | Allowable Values    | Description                                                                                                                                                                                                                                                                                        |
|-----------------------------------|---------------|---------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Azure Storage Credentials Service |               |                     | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                                                                                         |
| Proxy Configuration Service       |               |                     | Specifies the Proxy Configuration Controller Service to proxy network requests.                                                                                                                                                                                                                    |
| **Filesystem Name**               |               |                     | Name of the Azure Storage File System. It is assumed to be already existing.<br/>**Supports Expression Language: true**                                                                                                                                                                            |
| Directory Name                    |               |                     | Name of the Azure Storage Directory. The Directory Name cannot contain a leading '/'. If left empty it designates the root directory. The directory will be created if not already existing.<br/>**Supports Expression Language: true**                                                            |
| **Recurse Subdirectories**        | true          | true<br/>false      | Indicates whether to list files from subdirectories of the directory                                                                                                                                                                                                                               |
| File Filter                       |               |                     | Only files whose names match the given regular expression will be listed                                                                                                                                                                                                                           |
| Path Filter                       |               |                     | When 'Recurse Subdirectories' is true, then only subdirectories whose paths match the given regular expression will be scanned                                                                                                                                                                     |
| Listing Strategy                  | timestamps    | none<br/>timestamps | Specify how to determine new/updated entities. If 'timestamps' is selected it tracks the latest timestamp of listed entity to determine new/updated entities. If 'none' is selected it lists an entity without any tracking, the same entity will be listed each time on executing this processor. |

### Relationships

| Name    | Description                                           |
|---------|-------------------------------------------------------|
| success | All FlowFiles that are received are routed to success |


## ListenHTTP

### Description

Starts an HTTP Server and listens on a given base path to transform incoming requests into FlowFiles. The default URI of the Service will be http://{hostname}:{port}/contentListener. Only HEAD, POST, and GET requests are supported. PUT, and DELETE will result in an error and the HTTP response status code 405. The response body text for all requests, by default, is empty (length of 0). A static response body can be set for a given URI by sending input files to ListenHTTP with the http.type attribute set to response_body. The response body FlowFile filename attribute is appended to the Base Path property (separated by a /) when mapped to incoming requests. The mime.type attribute of the response body FlowFile is used for the Content-type header in responses. Response body content can be cleared by sending an empty (size 0) FlowFile for a given URI mapping.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                          | Default Value   | Allowable Values | Description                                                                                                                                                                                        |
|-----------------------------------------------|-----------------|------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Base Path**                                 | contentListener |                  | Base path for incoming connections                                                                                                                                                                 |
| **Listening Port**                            | 80              |                  | The Port to listen on for incoming connections. 0 means port is going to be selected randomly.                                                                                                     |
| Authorized DN Pattern                         | .*              |                  | A Regular Expression to apply against the Distinguished Name of incoming connections. If the Pattern does not match the DN, the connection will be refused.                                        |
| SSL Certificate                               |                 |                  | File containing PEM-formatted file including TLS/SSL certificate and key                                                                                                                           |
| SSL Certificate Authority                     |                 |                  | File containing trusted PEM-formatted certificates                                                                                                                                                 |
| SSL Verify Peer                               | no              | yes<br/>no       | Whether or not to verify the client's certificate (yes/no)                                                                                                                                         |
| SSL Minimum Version                           | TLS1.2          | TLS1.2           | Minimum TLS/SSL version allowed (TLS1.2)                                                                                                                                                           |
| HTTP Headers to receive as Attributes (Regex) |                 |                  | Specifies the Regular Expression that determines the names of HTTP Headers that should be passed along as FlowFile attributes                                                                      |
| Batch Size                                    | 5               |                  | Maximum number of buffered requests to be processed in a single batch. If set to zero all buffered requests are processed.                                                                         |
| Buffer Size                                   | 5               |                  | Maximum number of HTTP Requests allowed to be buffered before processing them when the processor is triggered. If the buffer full, the request is refused. If set to zero the buffer is unlimited. |

### Relationships

| Name    | Description                     |
|---------|---------------------------------|
| success | All files are routed to success |


## ListenSyslog

### Description

Listens for Syslog messages being sent to a given port over TCP or UDP. Incoming messages are optionally checked against regular expressions for RFC5424 and RFC3164 formatted messages. With parsing enabled the individual parts of the message will be placed as FlowFile attributes and valid messages will be transferred to success relationship, while invalid messages will be transferred to invalid relationship. With parsing disabled all message will be routed to the success relationship, but it will only contain the sender, protocol, and port attributes

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                      | Default Value | Allowable Values           | Description                                                                                                                                                                                                                     |
|---------------------------|---------------|----------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Listening Port**        | 514           |                            | The port for Syslog communication. (Well-known ports (0-1023) require root access)                                                                                                                                              |
| **Protocol**              | UDP           | TCP<br/>UDP                | The protocol for Syslog communication.                                                                                                                                                                                          |
| Max Batch Size            | 500           |                            | The maximum number of Syslog events to process at a time.                                                                                                                                                                       |
| Parse Messages            | false         | true<br/>false             | Indicates if the processor should parse the Syslog messages. If set to false, each outgoing FlowFile will only contain the sender, protocol, and port, and no additional attributes.                                            |
| Max Size of Message Queue | 10000         |                            | Maximum number of Syslog messages allowed to be buffered before processing them when the processor is triggered. If the buffer is full, the message is ignored. If set to zero the buffer is unlimited.                         |
| SSL Context Service       |               |                            | The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be received over a secure connection. This Property is only considered if the <Protocol> Property has a value of "TCP". |
| Client Auth               | NONE          | NONE<br/>WANT<br/>REQUIRED | The client authentication policy to use for the SSL Context. Only used if an SSL Context Service is provided.                                                                                                                   |

### Relationships

| Name    | Description                                                                                                                                                                                   |
|---------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| success | Incoming messages that match the expected format when parsing will be sent to this relationship. When Parse Messages is set to false, all incoming message will be sent to this relationship. |
| invalid | Incoming messages that do not match the expected format when parsing will be sent to this relationship.                                                                                       |

### Output Attributes

| Attribute              | Relationship | Description                                                                                       |
|------------------------|--------------|---------------------------------------------------------------------------------------------------|
| syslog.protocol        |              | The protocol over which the Syslog message was received.                                          |
| syslog.port            |              | The port over which the Syslog message was received.                                              |
| syslog.sender          |              | The hostname of the Syslog server that sent the message.                                          |
| syslog.valid           |              | An indicator of whether this message matched the expected formats. (requirement: parsing enabled) |
| syslog.priority        |              | The priority of the Syslog message. (requirement: parsed RFC5424/RFC3164)                         |
| syslog.severity        |              | The severity of the Syslog message. (requirement: parsed RFC5424/RFC3164)                         |
| syslog.facility        |              | The facility of the Syslog message. (requirement: parsed RFC5424/RFC3164)                         |
| syslog.timestamp       |              | The timestamp of the Syslog message. (requirement: parsed RFC5424/RFC3164)                        |
| syslog.hostname        |              | The hostname of the Syslog message. (requirement: parsed RFC5424/RFC3164)                         |
| syslog.msg             |              | The free-form message of the Syslog message. (requirement: parsed RFC5424/RFC3164)                |
| syslog.version         |              | The version of the Syslog message. (requirement: parsed RFC5424)                                  |
| syslog.app_name        |              | The app name of the Syslog message. (requirement: parsed RFC5424)                                 |
| syslog.proc_id         |              | The proc id of the Syslog message. (requirement: parsed RFC5424)                                  |
| syslog.msg_id          |              | The message id of the Syslog message. (requirement: parsed RFC5424)                               |
| syslog.structured_data |              | The structured data of the Syslog message. (requirement: parsed RFC5424)                          |


## ListenTCP

### Description

Listens for incoming TCP connections and reads data from each connection using a configurable message delimiter. For each message the processor produces a single FlowFile.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                          | Default Value | Allowable Values           | Description                                                                                                                                                                                      |
|-------------------------------|---------------|----------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Listening Port**            |               |                            | The port to listen on for communication.                                                                                                                                                         |
| **Max Batch Size**            | 500           |                            | The maximum number of messages to process at a time.                                                                                                                                             |
| **Max Size of Message Queue** | 10000         |                            | Maximum number of messages allowed to be buffered before processing them when the processor is triggered. If the buffer is full, the message is ignored. If set to zero the buffer is unlimited. |
| SSL Context Service           |               |                            | The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be received over a secure connection.                                                    |
| Client Auth                   | NONE          | NONE<br/>WANT<br/>REQUIRED | The client authentication policy to use for the SSL Context. Only used if an SSL Context Service is provided.                                                                                    |
| **Message Delimiter**         | \n            |                            | The delimiter is used to divide the stream into flowfiles.                                                                                                                                       |
| **Consume Delimiter**         | true          | true<br/>false             | If set to true then the delimiter won't be included at the end of the resulting flowfiles.                                                                                                       |

### Relationships

| Name    | Description                                                        |
|---------|--------------------------------------------------------------------|
| success | Messages received successfully will be sent out this relationship. |

### Output Attributes

| Attribute  | Relationship | Description                                  |
|------------|--------------|----------------------------------------------|
| tcp.port   |              | The sending port the messages were received. |
| tcp.sender |              | The sending host of the messages.            |


## ListenUDP

### Description

Listens for incoming UDP datagrams. For each datagram the processor produces a single FlowFile.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                          | Default Value | Allowable Values | Description                                                                                                                                                                                      |
|-------------------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Listening Port**            |               |                  | The port to listen on for communication.                                                                                                                                                         |
| **Max Batch Size**            | 500           |                  | The maximum number of messages to process at a time.                                                                                                                                             |
| **Max Size of Message Queue** | 10000         |                  | Maximum number of messages allowed to be buffered before processing them when the processor is triggered. If the buffer is full, the message is ignored. If set to zero the buffer is unlimited. |

### Relationships

| Name    | Description                                                        |
|---------|--------------------------------------------------------------------|
| success | Messages received successfully will be sent out this relationship. |

### Output Attributes

| Attribute       | Relationship | Description                                             |
|-----------------|--------------|---------------------------------------------------------|
| udp.port        |              | The listening port on which the messages were received. |
| udp.sender      |              | The sending host of the messages.                       |
| udp.sender.port |              | The sending port of the messages.                       |


## ListFile

### Description

Retrieves a listing of files from the local filesystem. For each file that is listed, creates a FlowFile that represents the file so that it can be fetched in conjunction with FetchFile.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value | Allowable Values | Description                                                                                                                                                |
|----------------------------|---------------|------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Input Directory**        |               |                  | The input directory from which files to pull files                                                                                                         |
| **Recurse Subdirectories** | true          | true<br/>false   | Indicates whether to list files from subdirectories of the directory                                                                                       |
| File Filter                |               |                  | Only files whose names match the given regular expression will be picked up                                                                                |
| Path Filter                |               |                  | When Recurse Subdirectories is true, then only subdirectories whose path matches the given regular expression will be scanned                              |
| **Minimum File Age**       | 0 sec         |                  | The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored |
| Maximum File Age           |               |                  | The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored   |
| **Minimum File Size**      | 0 B           |                  | The minimum size that a file must be in order to be pulled                                                                                                 |
| Maximum File Size          |               |                  | The maximum size that a file can be in order to be pulled                                                                                                  |
| **Ignore Hidden Files**    | true          | true<br/>false   | Indicates whether or not hidden files should be ignored                                                                                                    |

### Relationships

| Name    | Description                                           |
|---------|-------------------------------------------------------|
| success | All FlowFiles that are received are routed to success |

### Output Attributes

| Attribute             | Relationship | Description                                                                                                                                                                                                                                                                                                                                                                                           |
|-----------------------|--------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| filename              | success      | The name of the file that was read from filesystem.                                                                                                                                                                                                                                                                                                                                                   |
| path                  | success      | The path is set to the relative path of the file's directory on filesystem compared to the Input Directory property. For example, if Input Directory is set to /tmp, then files picked up from /tmp will have the path attribute set to "./". If the Recurse Subdirectories property is set to true and a file is picked up from /tmp/abc/1/2/3, then the path attribute will be set to "abc/1/2/3/". |
| absolute.path         | success      | The absolute.path is set to the absolute path of the file's directory on filesystem. For example, if the Input Directory property is set to /tmp, then files picked up from /tmp will have the path attribute set to "/tmp/". If the Recurse Subdirectories property is set to true and a file is picked up from /tmp/abc/1/2/3, then the path attribute will be set to "/tmp/abc/1/2/3/".            |
| file.owner            | success      | The user that owns the file in filesystem                                                                                                                                                                                                                                                                                                                                                             |
| file.group            | success      | The group that owns the file in filesystem                                                                                                                                                                                                                                                                                                                                                            |
| file.size             | success      | The number of bytes in the file in filesystem                                                                                                                                                                                                                                                                                                                                                         |
| file.permissions      | success      | The permissions for the file in filesystem. This is formatted as 3 characters for the owner, 3 for the group, and 3 for other users. For example rw-rw-r--                                                                                                                                                                                                                                            |
| file.lastModifiedTime | success      | The timestamp of when the file in filesystem was last modified as 'yyyy-MM-dd'T'HH:mm:ssZ'                                                                                                                                                                                                                                                                                                            |


## ListGCSBucket

### Description

Retrieves a listing of objects from an GCS bucket. For each object that is listed, creates a FlowFile that represents the object so that it can be fetched in conjunction with FetchGCSObject.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                 | Default Value | Allowable Values | Description                                                                                                                       |
|--------------------------------------|---------------|------------------|-----------------------------------------------------------------------------------------------------------------------------------|
| **GCP Credentials Provider Service** |               |                  | The Controller Service used to obtain Google Cloud Platform credentials. Should be the name of a GCPCredentialsControllerService. |
| **Number of retries**                | 6             |                  | How many retry attempts should be made before routing to the failure relationship.                                                |
| Endpoint Override URL                |               |                  | Overrides the default Google Cloud Storage endpoints<br/>**Supports Expression Language: true**                                   |
| **Bucket**                           |               |                  | Bucket of the object.<br/>**Supports Expression Language: true**                                                                  |
| List all versions                    | false         | true<br/>false   | Set this option to `true` to get all the previous versions separately.                                                            |

### Relationships

| Name    | Description                                                                                  |
|---------|----------------------------------------------------------------------------------------------|
| success | FlowFiles are routed to this relationship after a successful Google Cloud Storage operation. |

### Output Attributes

| Attribute                | Relationship | Description                                                        |
|--------------------------|--------------|--------------------------------------------------------------------|
| gcs.bucket               | success      | Bucket of the object.                                              |
| gcs.key                  | success      | Name of the object.                                                |
| filename                 | success      | Same as gcs.key                                                    |
| gcs.size                 | success      | Size of the object.                                                |
| gcs.crc32c               | success      | The CRC32C checksum of object's data, encoded in base64.           |
| gcs.md5                  | success      | The MD5 hash of the object's data, encoded in base64.              |
| gcs.owner.entity         | success      | The owner entity, in the form "user-emailAddress".                 |
| gcs.owner.entity.id      | success      | The ID for the entity.                                             |
| gcs.content.encoding     | success      | The content encoding of the object.                                |
| gcs.content.language     | success      | The content language of the object.                                |
| gcs.content.disposition  | success      | The data content disposition of the object.                        |
| gcs.media.link           | success      | The media download link to the object.                             |
| gcs.self.link            | success      | The link to this object.                                           |
| gcs.etag                 | success      | The HTTP 1.1 Entity tag for the object.                            |
| gcs.generated.id         | success      | The service-generated ID for the object.                           |
| gcs.generation           | success      | The content generation of this object. Used for object versioning. |
| gcs.metageneration       | success      | The metageneration of the object.                                  |
| gcs.create.time          | success      | Unix timestamp of the object's creation in milliseconds.           |
| gcs.update.time          | success      | Unix timestamp of the object's last modification in milliseconds.  |
| gcs.delete.time          | success      | Unix timestamp of the object's deletion in milliseconds.           |
| gcs.encryption.algorithm | success      | The algorithm used to encrypt the object.                          |
| gcs.encryption.sha256    | success      | The SHA256 hash of the key used to encrypt the object.             |


## ListS3

### Description

This Processor retrieves a listing of objects from an Amazon S3 bucket.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                             | Default Value | Allowable Values                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         | Description                                                                                                                                                                                                                                                                                                 |
|----------------------------------|---------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Access Key                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | AWS account access key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                           |
| Secret Key                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | AWS account secret key<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                          |
| Credentials File                 |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey                                                                                                                                                                                 |
| AWS Credentials Provider service |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.                                                                                                                                                                                                         |
| **Region**                       | us-west-2     | af-south-1<br/>ap-east-1<br/>ap-northeast-1<br/>ap-northeast-2<br/>ap-northeast-3<br/>ap-south-1<br/>ap-south-2<br/>ap-southeast-1<br/>ap-southeast-2<br/>ap-southeast-3<br/>ap-southeast-4<br/>ap-southeast-5<br/>ap-southeast-7<br/>ca-central-1<br/>ca-west-1<br/>cn-north-1<br/>cn-northwest-1<br/>eu-central-1<br/>eu-central-2<br/>eu-isoe-west-1<br/>eu-north-1<br/>eu-south-1<br/>eu-south-2<br/>eu-west-1<br/>eu-west-2<br/>eu-west-3<br/>il-central-1<br/>me-central-1<br/>me-south-1<br/>mx-central-1<br/>sa-east-1<br/>us-east-1<br/>us-east-2<br/>us-gov-east-1<br/>us-gov-west-1<br/>us-iso-east-1<br/>us-iso-west-1<br/>us-isob-east-1<br/>us-isof-east-1<br/>us-isof-south-1<br/>us-west-1<br/>us-west-2 | AWS Region                                                                                                                                                                                                                                                                                                  |
| **Communications Timeout**       | 30 sec        |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Sets the timeout of the communication between the AWS server and the client                                                                                                                                                                                                                                 |
| Endpoint Override URL            |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Endpoint URL to use instead of the AWS default including scheme, host, port, and path. The AWS libraries select an endpoint URL based on the AWS region, but this property overrides the selected endpoint URL, allowing use with other S3-compatible endpoints.<br/>**Supports Expression Language: true** |
| Proxy Host                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Proxy host name or IP<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                            |
| Proxy Port                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The port number of the proxy host<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                |
| Proxy Username                   |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Username to set when authenticating against proxy<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
| Proxy Password                   |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Password to set when authenticating against proxy<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                               |
| Proxy Configuration Service      |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Specifies the Proxy Configuration Controller Service to proxy network requests. When used, this will override any values specified for Proxy Host, Proxy Port, Proxy Username, and Proxy Password properties.                                                                                               |
| **Use Default Credentials**      | false         | true<br/>false                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc.                                                                                                                                                                 |
| **Bucket**                       |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The S3 bucket<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                    |
| Delimiter                        |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The string used to delimit directories within the bucket. Please consult the AWS documentation for the correct use of this field.                                                                                                                                                                           |
| Prefix                           |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The prefix used to filter the object list. In most cases, it should end with a forward slash ('/').                                                                                                                                                                                                         |
| **Use Versions**                 | false         | true<br/>false                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | Specifies whether to use S3 versions, if applicable. If false, only the latest version of each object will be returned.                                                                                                                                                                                     |
| **Minimum Object Age**           | 0 sec         |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The minimum age that an S3 object must be in order to be considered; any object younger than this amount of time (according to last modification date) will be ignored.                                                                                                                                     |
| **Write Object Tags**            | false         | true<br/>false                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | If set to 'true', the tags associated with the S3 object will be written as FlowFile attributes.                                                                                                                                                                                                            |
| **Write User Metadata**          | false         | true<br/>false                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | If set to 'true', the user defined metadata associated with the S3 object will be added to FlowFile attributes/records.                                                                                                                                                                                     |
| **Requester Pays**               | false         | true<br/>false                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | If true, indicates that the requester consents to pay any charges associated with listing the S3 bucket. This sets the 'x-amz-request-payer' header to 'requester'. Note that this setting is only used if Write User Metadata is true.                                                                     |

### Relationships

| Name    | Description                                  |
|---------|----------------------------------------------|
| success | FlowFiles are routed to success relationship |


## ListSFTP

### Description

Performs a listing of the files residing on an SFTP server. For each file that is found on the remote server, a new FlowFile will be created with the filename attribute set to the name of the file on the remote server. This can then be used in conjunction with FetchSFTP in order to fetch those files.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                   | Default Value       | Allowable Values                                     | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
|----------------------------------------|---------------------|------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Hostname**                           |                     |                                                      | The fully qualified hostname or IP address of the remote system<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| **Port**                               |                     |                                                      | The port that the remote system is listening on for file transfers<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| **Username**                           |                     |                                                      | Username<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| Password                               |                     |                                                      | Password for the user account<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Private Key Path                       |                     |                                                      | The fully qualified path to the Private Key file<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| Private Key Passphrase                 |                     |                                                      | Password for the private key<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| **Strict Host Key Checking**           | false               | true<br/>false                                       | Indicates whether or not strict enforcement of hosts keys should be applied                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| Host Key File                          |                     |                                                      | If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| **Connection Timeout**                 | 30 sec              |                                                      | Amount of time to wait before timing out while creating a connection                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| **Data Timeout**                       | 30 sec              |                                                      | When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| **Send Keep Alive On Timeout**         | true                | true<br/>false                                       | Indicates whether or not to send a single Keep Alive message when SSH socket times out                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Proxy Type                             | DIRECT              | DIRECT<br/>HTTP<br/>SOCKS                            | Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. Supported proxies: HTTP + AuthN, SOCKS + AuthN                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| Proxy Host                             |                     |                                                      | The fully qualified hostname or IP address of the proxy server<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Proxy Port                             |                     |                                                      | The port of the proxy server<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| Http Proxy Username                    |                     |                                                      | Http Proxy Username<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| Http Proxy Password                    |                     |                                                      | Http Proxy Password<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| **Listing Strategy**                   | Tracking Timestamps | Tracking Timestamps<br/>Tracking Entities            | Specify how to determine new/updated entities. See each strategy descriptions for detail.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Remote Path                            |                     |                                                      | The fully qualified filename on the remote system<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| **Search Recursively**                 | false               | true<br/>false                                       | If true, will pull files from arbitrarily nested subdirectories; otherwise, will not traverse subdirectories                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| **Follow symlink**                     | false               | true<br/>false                                       | If true, will pull even symbolic files and also nested symbolic subdirectories; otherwise, will not read symbolic files and will not traverse symbolic link subdirectories                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| File Filter Regex                      |                     |                                                      | Provides a Java Regular Expression for filtering Filenames; if a filter is supplied, only files whose names match that Regular Expression will be fetched                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Path Filter Regex                      |                     |                                                      | When Search Recursively is true, then only subdirectories whose path matches the given Regular Expression will be scanned                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| **Ignore Dotted Files**                | true                | true<br/>false                                       | If true, files whose names begin with a dot (".") will be ignored                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| **Target System Timestamp Precision**  | Auto Detect         | Auto Detect<br/>Milliseconds<br/>Seconds<br/>Minutes | Specify timestamp precision at the target system. Since this processor uses timestamp of entities to decide which should be listed, it is crucial to use the right timestamp precision.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| Entity Tracking Time Window            |                     |                                                      | Specify how long this processor should track already-listed entities. 'Tracking Entities' strategy can pick any entity whose timestamp is inside the specified time window. For example, if set to '30 minutes', any entity having timestamp in recent 30 minutes will be the listing target when this processor runs. A listed entity is considered 'new/updated' and a FlowFile is emitted if one of following condition meets: 1. does not exist in the already-listed entities, 2. has newer timestamp than the cached entity, 3. has different size than the cached entity. If a cached entity's timestamp becomes older than specified time window, that entity will be removed from the cached already-listed entities. Used by 'Tracking Entities' strategy. |
| Entity Tracking Initial Listing Target | All Available       | Tracking Time Window<br/>All Available               | Specify how initial listing should be handled. Used by 'Tracking Entities' strategy.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| **Minimum File Age**                   | 0 sec               |                                                      | The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| Maximum File Age                       |                     |                                                      | The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| **Minimum File Size**                  | 0 B                 |                                                      | The minimum size that a file must be in order to be pulled                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| Maximum File Size                      |                     |                                                      | The maximum size that a file must be in order to be pulled                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |

### Relationships

| Name    | Description                                           |
|---------|-------------------------------------------------------|
| success | All FlowFiles that are received are routed to success |


## ListSmb

### Description

Retrieves a listing of files from an SMB share. For each file that is listed, creates a FlowFile that represents the file so that it can be fetched in conjunction with FetchSmb.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                  | Default Value | Allowable Values | Description                                                                                                                                                                           |
|---------------------------------------|---------------|------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **SMB Connection Controller Service** |               |                  | Specifies the SMB connection controller service to use for connecting to the SMB server. If the SMB share is auto-mounted to a drive letter, its recommended to use ListFile instead. |
| Input Directory                       |               |                  | The input directory to list the contents of                                                                                                                                           |
| **Recurse Subdirectories**            | true          | true<br/>false   | Indicates whether to list files from subdirectories of the directory                                                                                                                  |
| File Filter                           |               |                  | Only files whose names match the given regular expression will be picked up                                                                                                           |
| Path Filter                           |               |                  | When Recurse Subdirectories is true, then only subdirectories whose path matches the given regular expression will be scanned                                                         |
| **Minimum File Age**                  | 5 sec         |                  | The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored                            |
| Maximum File Age                      |               |                  | The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored                              |
| Minimum File Size                     |               |                  | The minimum size that a file must be in order to be pulled                                                                                                                            |
| Maximum File Size                     |               |                  | The maximum size that a file can be in order to be pulled                                                                                                                             |
| **Ignore Hidden Files**               | true          | true<br/>false   | Indicates whether or not hidden files should be ignored                                                                                                                               |

### Relationships

| Name    | Description                                           |
|---------|-------------------------------------------------------|
| success | All FlowFiles that are received are routed to success |

### Output Attributes

| Attribute        | Relationship | Description                                                                                                                                                                                                                                                                                                                                                  |
|------------------|--------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| filename         | success      | The name of the file that was read from filesystem.                                                                                                                                                                                                                                                                                                          |
| path             | success      | The path is set to the relative path of the file's directory on the remote filesystem compared to the Share root directory. For example, for a given remote location smb://HOSTNAME:PORT/SHARE/DIRECTORY, and a file is being listed from smb://HOSTNAME:PORT/SHARE/DIRECTORY/sub/folder/file then the path attribute will be set to "DIRECTORY/sub/folder". |
| serviceLocation  | success      | The SMB URL of the share.                                                                                                                                                                                                                                                                                                                                    |
| lastModifiedTime | success      | The timestamp of when the file's content changed in the filesystem as 'yyyy-MM-dd'T'HH:mm:ss'.                                                                                                                                                                                                                                                               |
| creationTime     | success      | The timestamp of when the file was created in the filesystem as 'yyyy-MM-dd'T'HH:mm:ss'.                                                                                                                                                                                                                                                                     |
| lastAccessTime   | success      | The timestamp of when the file was accessed in the filesystem as 'yyyy-MM-dd'T'HH:mm:ss'.                                                                                                                                                                                                                                                                    |
| size             | success      | The size of the file in bytes.                                                                                                                                                                                                                                                                                                                               |


## RunLlamaCppInference

### Description

LlamaCpp processor to use llama.cpp library for running language model inference. The inference will be based on the System Prompt and the Prompt property values, together with the content of the incoming flow file. In the Prompt, the content of the incoming flow file can be referred to as 'the input data' or 'the flow file content'.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                             | Default Value                                                                                                                                                                                             | Allowable Values | Description                                                                                                             |
|----------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------|-------------------------------------------------------------------------------------------------------------------------|
| **Model Path**                   |                                                                                                                                                                                                           |                  | The filesystem path of the model file in gguf format.                                                                   |
| Temperature                      | 0.8                                                                                                                                                                                                       |                  | The temperature to use for sampling.                                                                                    |
| Top K                            | 40                                                                                                                                                                                                        |                  | Limit the next token selection to the K most probable tokens. Set <= 0 value to use vocab size.                         |
| Top P                            | 0.9                                                                                                                                                                                                       |                  | Limit the next token selection to a subset of tokens with a cumulative probability above a threshold P. 1.0 = disabled. |
| Min P                            |                                                                                                                                                                                                           |                  | Sets a minimum base probability threshold for token selection. 0.0 = disabled.                                          |
| **Min Keep**                     | 0                                                                                                                                                                                                         |                  | If greater than 0, force samplers to return N possible tokens at minimum.                                               |
| **Text Context Size**            | 4096                                                                                                                                                                                                      |                  | Size of the text context, use 0 to use size set in model.                                                               |
| **Logical Maximum Batch Size**   | 2048                                                                                                                                                                                                      |                  | Logical maximum batch size that can be submitted to the llama.cpp decode function.                                      |
| **Physical Maximum Batch Size**  | 512                                                                                                                                                                                                       |                  | Physical maximum batch size.                                                                                            |
| **Max Number Of Sequences**      | 1                                                                                                                                                                                                         |                  | Maximum number of sequences (i.e. distinct states for recurrent models).                                                |
| **Threads For Generation**       | 4                                                                                                                                                                                                         |                  | Number of threads to use for generation.                                                                                |
| **Threads For Batch Processing** | 4                                                                                                                                                                                                         |                  | Number of threads to use for batch processing.                                                                          |
| Prompt                           |                                                                                                                                                                                                           |                  | The user prompt for the inference.<br/>**Supports Expression Language: true**                                           |
| System Prompt                    | You are a helpful assistant. You are given a question with some possible input data otherwise called flow file content. You are expected to generate a response based on the question and the input data. |                  | The system prompt for the inference.                                                                                    |

### Relationships

| Name    | Description                      |
|---------|----------------------------------|
| success | Generated results from the model |
| failure | Generation failed                |

### Output Attributes

| Attribute                    | Relationship | Description                                    |
|------------------------------|--------------|------------------------------------------------|
| llamacpp.time.to.first.token | success      | Time to first token generated in milliseconds. |
| llamacpp.tokens.per.second   | success      | Tokens generated per second.                   |


## LogAttribute

### Description

Logs attributes of flow files in the MiNiFi application log.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                        | Default Value | Allowable Values                                         | Description                                                                                                                                                    |
|-----------------------------|---------------|----------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Log Level                   |               | trace<br/>debug<br/>info<br/>warn<br/>error<br/>critical | The Log Level to use when logging the Attributes                                                                                                               |
| Attributes to Log           |               |                                                          | A comma-separated list of Attributes to Log. If not specified, all attributes will be logged.                                                                  |
| Attributes to Ignore        |               |                                                          | A comma-separated list of Attributes to ignore. If not specified, no attributes will be ignored.                                                               |
| Log Payload                 | false         | true<br/>false                                           | If true, the FlowFile's payload will be logged, in addition to its attributes. Otherwise, just the Attributes will be logged.                                  |
| Hexencode Payload           | false         | true<br/>false                                           | If true, the FlowFile's payload will be logged in a hexencoded format                                                                                          |
| Maximum Payload Line Length | 0             |                                                          | The logged payload will be broken into lines this long. 0 means no newlines will be added.                                                                     |
| Log Prefix                  |               |                                                          | Log prefix appended to the log lines. It helps to distinguish the output of multiple LogAttribute processors.                                                  |
| FlowFiles To Log            | 1             |                                                          | Number of flow files to log. If set to zero all flow files will be logged. Please note that this may block other threads from running if not used judiciously. |

### Relationships

| Name    | Description                            |
|---------|----------------------------------------|
| success | success operational on the flow record |


## ManipulateArchive

### Description

Performs an operation which manipulates an archive without needing to split the archive into multiple FlowFiles.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name        | Default Value | Allowable Values | Description                                                                                                   |
|-------------|---------------|------------------|---------------------------------------------------------------------------------------------------------------|
| Operation   |               |                  | Operation to perform on the archive (touch, remove, copy, move).                                              |
| Target      |               |                  | An existing entry within the archive to perform the operation on.                                             |
| Destination |               |                  | Destination for operations (touch, move or copy) which result in new entries.                                 |
| Before      |               |                  | For operations which result in new entries, places the new entry before the entry specified by this property. |
| After       |               |                  | For operations which result in new entries, places the new entry after the entry specified by this property.  |

### Relationships

| Name    | Description                                                                          |
|---------|--------------------------------------------------------------------------------------|
| success | FlowFiles will be transferred to the success relationship if the operation succeeds. |
| failure | FlowFiles will be transferred to the failure relationship if the operation fails.    |


## MergeContent

### Description

Merges a Group of FlowFiles together based on a user-defined strategy and packages them into a single FlowFile. MergeContent should be configured with only one incoming connection as it won't create grouped Flow Files.This processor updates the mime.type attribute as appropriate.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value               | Allowable Values                                             | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
|----------------------------|-----------------------------|--------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Minimum Group Size         | 0                           |                                                              | The minimum size of for the bundle                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Maximum Group Size         |                             |                                                              | The maximum size for the bundle. If not specified, there is no maximum.                                                                                                                                                                                                                                                                                                                                                                                                          |
| Minimum Number of Entries  | 1                           |                                                              | The minimum number of files to include in a bundle                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Maximum Number of Entries  |                             |                                                              | The maximum number of files to include in a bundle. If not specified, there is no maximum.                                                                                                                                                                                                                                                                                                                                                                                       |
| Maximum number of Bins     | 100                         |                                                              | Specifies the maximum number of bins that can be held in memory at any one time                                                                                                                                                                                                                                                                                                                                                                                                  |
| Max Bin Age                |                             |                                                              | The maximum age of a Bin that will trigger a Bin to be complete. Expected format is <duration> <time unit>                                                                                                                                                                                                                                                                                                                                                                       |
| Batch Size                 | 1                           |                                                              | Maximum number of FlowFiles processed in a single session                                                                                                                                                                                                                                                                                                                                                                                                                        |
| Merge Strategy             | Bin-Packing Algorithm       | Defragment<br/>Bin-Packing Algorithm                         | Defragment or Bin-Packing Algorithm                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| Merge Format               | Binary Concatenation        | Binary Concatenation<br/>TAR<br/>ZIP<br/>FlowFile Stream, v3 | Merge Format                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| Correlation Attribute Name |                             |                                                              | Correlation Attribute Name                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| Delimiter Strategy         | Filename                    | Filename<br/>Text                                            | Determines if Header, Footer, and Demarcator should point to files                                                                                                                                                                                                                                                                                                                                                                                                               |
| Keep Path                  | false                       | true<br/>false                                               | If using the Zip or Tar Merge Format, specifies whether or not the FlowFiles' paths should be included in their entry                                                                                                                                                                                                                                                                                                                                                            |
| Header File                |                             |                                                              | Filename specifying the header to use                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Footer File                |                             |                                                              | Filename specifying the footer to use                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Demarcator File            |                             |                                                              | Filename specifying the demarcator to use                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| Attribute Strategy         | Keep Only Common Attributes | Keep Only Common Attributes<br/>Keep All Unique Attributes   | Determines which FlowFile attributes should be added to the bundle. If 'Keep All Unique Attributes' is selected, any attribute on any FlowFile that gets bundled will be kept unless its value conflicts with the value from another FlowFile (in which case neither, or none, of the conflicting attributes will be kept). If 'Keep Only Common Attributes' is selected, only the attributes that exist on all FlowFiles in the bundle, with the same value, will be preserved. |

### Relationships

| Name     | Description                                                                                                                  |
|----------|------------------------------------------------------------------------------------------------------------------------------|
| failure  | If the bundle cannot be created, all FlowFiles that would have been used to create the bundle will be transferred to failure |
| original | The FlowFiles that were used to create the bundle                                                                            |
| merged   | The FlowFile containing the merged content                                                                                   |


## MotionDetector

### Description

Detect motion from captured images.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                           | Default Value | Allowable Values | Description                                                                               |
|--------------------------------|---------------|------------------|-------------------------------------------------------------------------------------------|
| **Image Encoding**             | .jpg          | .jpg<br/>.png    | The encoding that should be applied to the output                                         |
| **Minimum Area**               | 100           |                  | We only consider the movement regions with area greater than this.                        |
| **Threshold for segmentation** | 42            |                  | Pixel greater than this will be white, otherwise black.                                   |
| **Dilate iteration**           | 10            |                  | For image processing, if an object is detected as 2 separate objects, increase this value |
| **Path to background frame**   |               |                  | If not provided then the processor will take the first input frame as background          |

### Relationships

| Name    | Description                 |
|---------|-----------------------------|
| success | Successful to detect motion |
| failure | Failure to detect motion    |

## PerformanceDataMonitor

### Description

This Windows only processor can create FlowFiles populated with various performance data with the help of Windows Performance Counters. Windows Performance Counters provide a high-level abstraction layer with a consistent interface for collecting various kinds of system data such as CPU, memory, and disk usage statistics.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                    | Default Value | Allowable Values       | Description                                                                                                                                          |
|-------------------------|---------------|------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------|
| Predefined Groups       |               |                        | Comma separated list from the allowable values, to monitor multiple common Windows Performance counters related to these groups (e.g. "CPU,Network") |
| Custom PDH Counters     |               |                        | Comma separated list of Windows Performance Counters to monitor (e.g. "\\System\\Threads,\\Process(*)\\ID Process")                                  |
| Output Format           | JSON          | JSON<br/>OpenTelemetry | Format of the created flowfiles                                                                                                                      |
| Output Compactness      | Pretty        | Pretty<br/>Compact     | Format of the created flowfiles                                                                                                                      |
| Round to decimal places |               |                        | The number of decimal places to round the values to (blank for no rounding)                                                                          |

#### Predefined Groups

<ul>
  <li>CPU
    <ul>
      <li>\Processor(*)\% Processor Time</li>
      <li>\Processor(*)\% User Time</li>
      <li>\Processor(*)\% Privileged Time</li>
    </ul>
  </li>
  <li>IO
    <ul>
      <li>\Process(_Total)\IO Read Bytes/sec</li>
      <li>\Process(_Total)\IO Write Bytes/sec</li>
    </ul>
  </li>
  <li>Disk
    <ul>
      <li>\LogicalDisk(*)\% Free Space</li>
      <li>\LogicalDisk(*)\Free Megabytes</li>
      <li>\PhysicalDisk(*)\% Disk Read Time</li>
      <li>\PhysicalDisk(*)\% Disk Time</li>
      <li>\PhysicalDisk(*)\% Disk Write Time</li>
      <li>\PhysicalDisk(*)\% Idle Time</li>
      <li>\PhysicalDisk(*)\Avg. Disk Bytes/Transfer</li>
      <li>\PhysicalDisk(*)\Avg. Disk Bytes/Read</li>
      <li>\PhysicalDisk(*)\Avg. Disk Bytes/Write</li>
      <li>\PhysicalDisk(*)\Avg. Disk Write Queue Length</li>
      <li>\PhysicalDisk(*)\Avg. Disk Read Queue Length</li>
      <li>\PhysicalDisk(*)\Avg. Disk Queue Length</li>
      <li>\PhysicalDisk(*)\Avg. Disk sec/Transfer</li>
      <li>\PhysicalDisk(*)\Avg. Disk sec/Read</li>
      <li>\PhysicalDisk(*)\Avg. Disk sec/Write</li>
      <li>\PhysicalDisk(*)\Current Disk Queue Length</li>
      <li>\PhysicalDisk(*)\Disk Transfers/sec</li>
      <li>\PhysicalDisk(*)\Disk Reads/sec</li>
      <li>\PhysicalDisk(*)\Disk Writes/sec</li>
      <li>\PhysicalDisk(*)\Disk Bytes/sec</li>
      <li>\PhysicalDisk(*)\Disk Read Bytes/sec</li>
      <li>\PhysicalDisk(*)\Disk Write Bytes/sec</li>
      <li>\PhysicalDisk(*)\Split IO/Sec</li>
    </ul>
  </li>
  <li>Network
    <ul>
      <li>\Network Interface(*)\Bytes Received/sec</li>
      <li>\Network Interface(*)\Bytes Sent/sec</li>
      <li>\Network Interface(*)\Bytes Total/sec</li>
      <li>\Network Interface(*)\Current Bandwidth</li>
      <li>\Network Interface(*)\Packets/sec</li>
      <li>\Network Interface(*)\Packets Received/sec</li>
      <li>\Network Interface(*)\Packets Sent/sec</li>
      <li>\Network Interface(*)\Packets Received Discarded</li>
      <li>\Network Interface(*)\Packets Received Errors</li>
      <li>\Network Interface(*)\Packets Received Unknown</li>
      <li>\Network Interface(*)\Packets Received Non-Unicast/sec</li>
      <li>\Network Interface(*)\Packets Received Unicast/sec</li>
      <li>\Network Interface(*)\Packets Sent Unicast/sec</li>
      <li>\Network Interface(*)\Packets Sent Non-Unicast/sec</li>
    </ul>
  </li>
  <li>Memory
    <ul>
      <li>\Memory\% Committed Bytes In Use</li>
      <li>\Memory\Available MBytes</li>
      <li>\Memory\Page Faults/sec</li>
      <li>\Memory\Pages/sec</li>
      <li>\Paging File(_Total)\% Usage</li>
      <li>Different source: Total Physical Memory</li>
      <li>Different source: Available Physical Memory</li>
      <li>Different source: Total paging file size</li>
    </ul>
  </li>
  <li>System
    <ul>
      <li>\System\% Registry Quota In Use</li>
      <li>\System\Context Switches/sec</li>
      <li>\System\File Control Bytes/sec</li>
      <li>\System\File Control Operations/sec</li>
      <li>\System\File Read Bytes/sec</li>
      <li>\System\File Read Operations/sec</li>
      <li>\System\File Write Bytes/sec</li>
      <li>\System\File Write Operations/sec</li>
      <li>\System\File Data Operations/sec</li>
      <li>\System\Processes</li>
      <li>\System\Processor Queue Length</li>
      <li>\System\System Calls/sec</li>
      <li>\System\System Up Time</li>
      <li>\System\Threads</li>
    </ul>
  </li>
  <li>Process
    <ul>
      <li>\Process(*)\% Processor Time</li>
      <li>\Process(*)\Elapsed Time</li>
      <li>\Process(*)\ID Process</li>
      <li>\Process(*)\Private Bytes</li>
    </ul>
  </li>
</ul>

### Relationships

| Name    | Description                     |
|---------|---------------------------------|
| success | All files are routed to success |


## PostElasticsearch

### Description

An Elasticsearch/Opensearch post processor that uses the Elasticsearch/Opensearch _bulk REST API.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                           | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                                                               |
|------------------------------------------------|---------------|------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Action**                                     |               |                  | The type of the operation used to index (create, delete, index, update, upsert)<br/>**Supports Expression Language: true**                                                                                                                                                                                |
| Max Batch Size                                 | 100           |                  | The maximum number of flow files to process at a time.                                                                                                                                                                                                                                                    |
| **Elasticsearch Credentials Provider Service** |               |                  | The Controller Service used to obtain Elasticsearch credentials.                                                                                                                                                                                                                                          |
| SSL Context Service                            |               |                  | The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.                                                                                                                                                                                                   |
| **Hosts**                                      |               |                  | A comma-separated list of HTTP hosts that host Elasticsearch query nodes. Currently only supports a single host.<br/>**Supports Expression Language: true**                                                                                                                                               |
| **Index**                                      |               |                  | The name of the index to use.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                  |
| Identifier                                     |               |                  | If the Action is "index" or "create", this property may be left empty or evaluate to an empty value, in which case the document's identifier will be auto-generated by Elasticsearch. For all other Actions, the attribute must evaluate to a non-empty value.<br/>**Supports Expression Language: true** |

### Relationships

| Name    | Description                                                                                   |
|---------|-----------------------------------------------------------------------------------------------|
| success | All flowfiles that succeed in being transferred into Elasticsearch go here.                   |
| failure | All flowfiles that fail for reasons unrelated to server availability go to this relationship. |
| error   | All flowfiles that Elasticsearch responded to with an error go to this relationship.          |


## ProcFsMonitor

### Description

This processor can create FlowFiles with various performance data through the proc pseudo-filesystem. (Linux only)

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                    | Default Value | Allowable Values       | Description                                                                                |
|-------------------------|---------------|------------------------|--------------------------------------------------------------------------------------------|
| **Output Format**       | JSON          | JSON<br/>OpenTelemetry | The output type of the new flowfile                                                        |
| **Output Compactness**  | Pretty        | Compact<br/>Pretty     | The output format of the new flowfile                                                      |
| Round to decimal places |               |                        | The number of decimal places to round the values to (blank for no rounding)                |
| **Result Type**         | Absolute      | Relative<br/>Absolute  | Absolute returns the current procfs values, relative calculates the usage between triggers |

### Relationships

| Name    | Description                     |
|---------|---------------------------------|
| success | All files are routed to success |


## PublishKafka

### Description

This Processor puts the contents of a FlowFile to a Topic in Apache Kafka. The content of a FlowFile becomes the contents of a Kafka message. This message is optionally assigned a key by using the <Kafka Key> Property.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                          | Default Value | Allowable Values                                  | Description                                                                                                                                                                                                                                                             |
|-------------------------------|---------------|---------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| SSL Context Service           |               |                                                   | SSL Context Service Name                                                                                                                                                                                                                                                |
| **Security Protocol**         | plaintext     | plaintext<br/>ssl<br/>sasl_plaintext<br/>sasl_ssl | Protocol used to communicate with brokers. Corresponds to Kafka's 'security.protocol' property.                                                                                                                                                                         |
| Kerberos Service Name         |               |                                                   | Kerberos Service Name                                                                                                                                                                                                                                                   |
| Kerberos Principal            |               |                                                   | Kerberos Principal                                                                                                                                                                                                                                                      |
| Kerberos Keytab Path          |               |                                                   | The path to the location on the local filesystem where the kerberos keytab is located. Read permission on the file is required.                                                                                                                                         |
| **SASL Mechanism**            | GSSAPI        | GSSAPI<br/>PLAIN                                  | The SASL mechanism to use for authentication. Corresponds to Kafka's 'sasl.mechanism' property.                                                                                                                                                                         |
| Username                      |               |                                                   | The username when the SASL Mechanism is sasl_plaintext                                                                                                                                                                                                                  |
| Password                      |               |                                                   | The password for the given username when the SASL Mechanism is sasl_plaintext<br/>**Sensitive Property: true**                                                                                                                                                          |
| **Known Brokers**             |               |                                                   | A comma-separated list of known Kafka Brokers in the format <host>:<port><br/>**Supports Expression Language: true**                                                                                                                                                    |
| **Topic Name**                |               |                                                   | The Kafka Topic of interest<br/>**Supports Expression Language: true**                                                                                                                                                                                                  |
| Delivery Guarantee            | 1             |                                                   | Specifies the requirement for guaranteeing that a message is sent to Kafka. Valid values are 0 (do not wait for acks), -1 or all (block until message is committed by all in sync replicas) or any concrete number of nodes.<br/>**Supports Expression Language: true** |
| Max Request Size              |               |                                                   | Maximum Kafka protocol request message size                                                                                                                                                                                                                             |
| Request Timeout               | 10 sec        |                                                   | The ack timeout of the producer request                                                                                                                                                                                                                                 |
| Message Timeout               | 30 sec        |                                                   | The total time sending a message could take                                                                                                                                                                                                                             |
| **Client Name**               |               |                                                   | Client Name to use when communicating with Kafka<br/>**Supports Expression Language: true**                                                                                                                                                                             |
| Batch Size                    | 10            |                                                   | Maximum number of messages batched in one MessageSet                                                                                                                                                                                                                    |
| Target Batch Payload Size     | 512 KB        |                                                   | The target total payload size for a batch. 0 B means unlimited (Batch Size is still applied).                                                                                                                                                                           |
| Attributes to Send as Headers |               |                                                   | Any attribute whose name matches the regex will be added to the Kafka messages as a Header                                                                                                                                                                              |
| Queue Buffering Max Time      | 5 millis      |                                                   | Delay to wait for messages in the producer queue to accumulate before constructing message batches                                                                                                                                                                      |
| Queue Max Buffer Size         | 1 MB          |                                                   | Maximum total message size sum allowed on the producer queue                                                                                                                                                                                                            |
| Queue Max Message             | 1000          |                                                   | Maximum number of messages allowed on the producer queue                                                                                                                                                                                                                |
| Compress Codec                | none          | none<br/>gzip<br/>snappy<br/>lz4<br/>zstd         | compression codec to use for compressing message sets                                                                                                                                                                                                                   |
| Max Flow Segment Size         | 0 B           |                                                   | Maximum flow content payload segment size for the kafka record. 0 B means unlimited.                                                                                                                                                                                    |
| Security CA                   |               |                                                   | DEPRECATED in favor of SSL Context Service. File or directory path to CA certificate(s) for verifying the broker's key                                                                                                                                                  |
| Security Cert                 |               |                                                   | DEPRECATED in favor of SSL Context Service.Path to client's public key (PEM) used for authentication                                                                                                                                                                    |
| Security Private Key          |               |                                                   | DEPRECATED in favor of SSL Context Service.Path to client's private key (PEM) used for authentication                                                                                                                                                                   |
| Security Pass Phrase          |               |                                                   | DEPRECATED in favor of SSL Context Service.Private key passphrase<br/>**Sensitive Property: true**                                                                                                                                                                      |
| Kafka Key                     |               |                                                   | The key to use for the message. If not specified, the UUID of the flow file is used as the message key.<br/>**Supports Expression Language: true**                                                                                                                      |
| Message Key Field             |               |                                                   | DEPRECATED, does not work -- use Kafka Key instead                                                                                                                                                                                                                      |
| Debug contexts                |               |                                                   | A comma-separated list of debug contexts to enable.Including: generic, broker, topic, metadata, feature, queue, msg, protocol, cgrp, security, fetch, interceptor, plugin, consumer, admin, eos, all                                                                    |
| Fail empty flow files         | true          | true<br/>false                                    | Keep backwards compatibility with <=0.7.0 bug which caused flow files with empty content to not be published to Kafka and forwarded to failure. The old behavior is deprecated. Use connections to drop empty flow files!                                               |

### Relationships

| Name    | Description                                                                         |
|---------|-------------------------------------------------------------------------------------|
| success | Any FlowFile that is successfully sent to Kafka will be routed to this Relationship |
| failure | Any FlowFile that cannot be sent to Kafka will be routed to this Relationship       |


## PublishMQTT

### Description

PublishMQTT serializes FlowFile content as an MQTT payload, sending the message to the configured topic and broker.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                    | Default Value | Allowable Values                     | Description                                                                                                                 |
|-------------------------|---------------|--------------------------------------|-----------------------------------------------------------------------------------------------------------------------------|
| **Broker URI**          |               |                                      | The URI to use to connect to the MQTT broker                                                                                |
| Client ID               |               |                                      | MQTT client ID to use. If not set, a UUID will be generated.                                                                |
| **MQTT Version**        | 3.x AUTO      | 3.x AUTO<br/>3.1.0<br/>3.1.1<br/>5.0 | The MQTT specification version when connecting to the broker.                                                               |
| **Topic**               |               |                                      | The topic to publish to.<br/>**Supports Expression Language: true**                                                         |
| Retain                  | false         | true<br/>false                       | Retain published message in broker                                                                                          |
| Message Expiry Interval |               |                                      | Time while message is valid and will be forwarded by broker. MQTT 5.x only.                                                 |
| Content Type            |               |                                      | Content type of the message. MQTT 5.x only.<br/>**Supports Expression Language: true**                                      |
| Record Reader           |               |                                      | The Record Reader to use for parsing the incoming FlowFile into Records.                                                    |
| Record Writer           |               |                                      | The Record Writer to use for serializing Records before publishing them as an MQTT Message.                                 |
| **Quality of Service**  | 0             | 0<br/>1<br/>2                        | The Quality of Service (QoS) of messages.                                                                                   |
| Connection Timeout      | 10 sec        |                                      | Maximum time interval the client will wait for the network connection to the MQTT broker                                    |
| Keep Alive Interval     | 60 sec        |                                      | Defines the maximum time interval between messages sent or received                                                         |
| Last Will Topic         |               |                                      | The topic to send the client's Last Will to. If the Last Will topic is not set then a Last Will will not be sent            |
| Last Will Message       |               |                                      | The message to send as the client's Last Will. If the Last Will Message is empty, Last Will will be deleted from the broker |
| **Last Will QoS**       | 0             | 0<br/>1<br/>2                        | The Quality of Service (QoS) to send the last will with.                                                                    |
| Last Will Retain        | false         | true<br/>false                       | Whether to retain the client's Last Will                                                                                    |
| Last Will Content Type  |               |                                      | Content type of the client's Last Will. MQTT 5.x only.                                                                      |
| Username                |               |                                      | Username to use when connecting to the broker                                                                               |
| Password                |               |                                      | Password to use when connecting to the broker<br/>**Sensitive Property: true**                                              |
| Security Protocol       |               |                                      | Protocol used to communicate with brokers                                                                                   |
| Security CA             |               |                                      | File or directory path to CA certificate(s) for verifying the broker's key                                                  |
| Security Cert           |               |                                      | Path to client's public key (PEM) used for authentication                                                                   |
| Security Private Key    |               |                                      | Path to client's private key (PEM) used for authentication                                                                  |
| Security Pass Phrase    |               |                                      | Private key passphrase<br/>**Sensitive Property: true**                                                                     |

### Relationships

| Name    | Description                                                                                  |
|---------|----------------------------------------------------------------------------------------------|
| success | FlowFiles that are sent successfully to the destination are transferred to this relationship |
| failure | FlowFiles that failed to be sent to the destination are transferred to this relationship     |


## PushGrafanaLokiGrpc

### Description

A Grafana Loki push processor that uses the Grafana Loki Grpc port. The processor expects each flow file to contain a single log line to be pushed to Grafana Loki, therefore it is usually used together with the TailFile processor.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                         | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                                    |
|------------------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Url**                      |               |                  | Url of the Grafana Loki server. For example localhost:3100.                                                                                                                                                                                                                    |
| **Stream Labels**            |               |                  | Comma separated list of <key>=<value> labels to be sent as stream labels.                                                                                                                                                                                                      |
| Log Line Metadata Attributes |               |                  | Comma separated list of attributes to be sent as log line metadata for a log line.                                                                                                                                                                                             |
| Tenant ID                    |               |                  | The tenant ID used by default to push logs to Grafana Loki. If omitted or empty it assumes Grafana Loki is running in single-tenant mode and no X-Scope-OrgID header is sent.                                                                                                  |
| Max Batch Size               | 100           |                  | The maximum number of flow files to process at a time. If it is set to 0, all FlowFiles will be processed at once.                                                                                                                                                             |
| Log Line Batch Wait          |               |                  | Time to wait before sending a log line batch to Grafana Loki, full or not. If this property and Log Line Batch Size are both unset, the log batch of the current trigger will be sent immediately.                                                                             |
| Log Line Batch Size          |               |                  | Number of log lines to send in a batch to Loki. If this property and Log Line Batch Wait are both unset, the log batch of the current trigger will be sent immediately.                                                                                                        |
| **Connection Timeout**       | 5 s           |                  | Max wait time for connection to the Grafana Loki service.                                                                                                                                                                                                                      |
| SSL Context Service          |               |                  | The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.                                                                                                                                                                        |
| Keep Alive Time              |               |                  | The period after which a keepalive ping is sent on the transport. If not set, then the keep alive is disabled.                                                                                                                                                                 |
| Keep Alive Timeout           |               |                  | The amount of time the sender of the keepalive ping waits for an acknowledgement. If it does not receive an acknowledgment within this time, it will close the connection. If not set, then the default value 20 seconds is used.                                              |
| Max Pings Without Data       |               |                  | The maximum number of pings that can be sent when there is no data/header frame to be sent. gRPC Core will not continue sending pings if we run over the limit. Setting it to 0 allows sending pings without such a restriction. If not set, then the default value 2 is used. |

### Relationships

| Name    | Description                                                                                    |
|---------|------------------------------------------------------------------------------------------------|
| success | All flowfiles that succeed in being transferred into Grafana Loki go here.                     |
| failure | If a submitted request fails all flow files in the batch are transferred to this relationship. |


## PushGrafanaLokiREST

### Description

A Grafana Loki push processor that uses the Grafana Loki REST API. The processor expects each flow file to contain a single log line to be pushed to Grafana Loki, therefore it is usually used together with the TailFile processor.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                         | Default Value | Allowable Values | Description                                                                                                                                                                                        |
|------------------------------|---------------|------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Url**                      |               |                  | Url of the Grafana Loki server. For example http://localhost:3100/.                                                                                                                                |
| **Stream Labels**            |               |                  | Comma separated list of <key>=<value> labels to be sent as stream labels.                                                                                                                          |
| Log Line Metadata Attributes |               |                  | Comma separated list of attributes to be sent as log line metadata for a log line.                                                                                                                 |
| Tenant ID                    |               |                  | The tenant ID used by default to push logs to Grafana Loki. If omitted or empty it assumes Grafana Loki is running in single-tenant mode and no X-Scope-OrgID header is sent.                      |
| Max Batch Size               | 100           |                  | The maximum number of flow files to process at a time. If it is set to 0, all FlowFiles will be processed at once.                                                                                 |
| Log Line Batch Wait          |               |                  | Time to wait before sending a log line batch to Grafana Loki, full or not. If this property and Log Line Batch Size are both unset, the log batch of the current trigger will be sent immediately. |
| Log Line Batch Size          |               |                  | Number of log lines to send in a batch to Loki. If this property and Log Line Batch Wait are both unset, the log batch of the current trigger will be sent immediately.                            |
| **Connection Timeout**       | 5 s           |                  | Max wait time for connection to the Grafana Loki service.                                                                                                                                          |
| **Read Timeout**             | 15 s          |                  | Max wait time for response from remote service.                                                                                                                                                    |
| SSL Context Service          |               |                  | The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.                                                                                            |
| Username                     |               |                  | Username for authenticating using basic authentication.                                                                                                                                            |
| Password                     |               |                  | Password for authenticating using basic authentication.<br/>**Sensitive Property: true**                                                                                                           |
| Bearer Token File            |               |                  | Path of file containing bearer token for bearer token authentication.                                                                                                                              |

### Relationships

| Name    | Description                                                                                    |
|---------|------------------------------------------------------------------------------------------------|
| success | All flowfiles that succeed in being transferred into Grafana Loki go here.                     |
| failure | If a submitted request fails all flow files in the batch are transferred to this relationship. |


## PutAzureBlobStorage

### Description

Puts content into an Azure Storage Blob

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                   | Default Value   | Allowable Values                                                                  | Description                                                                                                                                                                                                                                                                                                                                                                                                    |
|----------------------------------------|-----------------|-----------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Azure Storage Credentials Service      |                 |                                                                                   | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                                                                                                                                                                                                     |
| Proxy Configuration Service            |                 |                                                                                   | Specifies the Proxy Configuration Controller Service to proxy network requests.                                                                                                                                                                                                                                                                                                                                |
| **Container Name**                     |                 |                                                                                   | Name of the Azure Storage container. In case of PutAzureBlobStorage processor, container can be created if it does not exist.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                       |
| Storage Account Name                   |                 |                                                                                   | The storage account name.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                           |
| Storage Account Key                    |                 |                                                                                   | The storage account key. This is an admin-like password providing access to every container in this account. It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies if Credential Configuration Strategy is set to From Properties. If set, SAS Token must be empty.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**        |
| SAS Token                              |                 |                                                                                   | Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Credential Configuration Strategy is set to From Properties. If set, Storage Account Key must be empty.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                         |
| Common Storage Account Endpoint Suffix |                 |                                                                                   | Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a different suffix in certain circumstances (like Azure Stack or non-public Azure regions).<br/>**Supports Expression Language: true**                                                                                                                                                                    |
| Connection String                      |                 |                                                                                   | Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Credential Configuration Strategy is set to From Properties.<br/>**Supports Expression Language: true**                                                                                                                                                                                      |
| **Credential Configuration Strategy**  | From Properties | From Properties<br/>Default Credential<br/>Managed Identity<br/>Workload Identity | The strategy to use for credential configuration. If set to From Properties, the credentials are parsed from the SAS Token, Storage Account Key, and Connection String properties. In other cases, the selected Azure identity source is used.                                                                                                                                                                 |
| Managed Identity Client ID             |                 |                                                                                   | Client ID of the managed identity. The property is required when User Assigned Managed Identity is used for authentication and multiple user-assigned identities are added to the resource. It must be empty in case of System Assigned Managed Identity and can also be left empty if only one user-assigned identity is present.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true** |
| Blob                                   |                 |                                                                                   | The filename of the blob. If left empty the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                             |
| **Create Container**                   | false           | true<br/>false                                                                    | Specifies whether to check if the container exists and to automatically create it if it does not. Permission to list containers is required. If false, this check is not made, but the Put operation will fail if the container does not exist.                                                                                                                                                                |

### Relationships

| Name    | Description                                                             |
|---------|-------------------------------------------------------------------------|
| success | All successfully processed FlowFiles are routed to this relationship    |
| failure | Unsuccessful operations will be transferred to the failure relationship |


## PutAzureDataLakeStorage

### Description

Puts content into an Azure Data Lake Storage Gen 2

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                              | Default Value | Allowable Values            | Description                                                                                                                                                                                                                             |
|-----------------------------------|---------------|-----------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Azure Storage Credentials Service |               |                             | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                              |
| Proxy Configuration Service       |               |                             | Specifies the Proxy Configuration Controller Service to proxy network requests.                                                                                                                                                         |
| **Filesystem Name**               |               |                             | Name of the Azure Storage File System. It is assumed to be already existing.<br/>**Supports Expression Language: true**                                                                                                                 |
| Directory Name                    |               |                             | Name of the Azure Storage Directory. The Directory Name cannot contain a leading '/'. If left empty it designates the root directory. The directory will be created if not already existing.<br/>**Supports Expression Language: true** |
| File Name                         |               |                             | The filename in Azure Storage. If left empty the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                 |
| **Conflict Resolution Strategy**  | fail          | fail<br/>replace<br/>ignore | Indicates what should happen when a file with the same name already exists in the output directory.                                                                                                                                     |

### Relationships

| Name    | Description                                                                                           |
|---------|-------------------------------------------------------------------------------------------------------|
| success | Files that have been successfully written to Azure storage are transferred to this relationship       |
| failure | Files that could not be written to Azure storage for some reason are transferred to this relationship |


## PutCouchbaseKey

### Description

Put a document to Couchbase Server via Key/Value access.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                     | Default Value | Allowable Values                                   | Description                                                                                                                                                                                                                                           |
|------------------------------------------|---------------|----------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Couchbase Cluster Controller Service** |               |                                                    | A Couchbase Cluster Controller Service which manages connections to a Couchbase cluster.                                                                                                                                                              |
| **Bucket Name**                          | default       |                                                    | The name of bucket to access.<br/>**Supports Expression Language: true**                                                                                                                                                                              |
| Scope Name                               |               |                                                    | Scope to use inside the bucket. If not specified, the _default scope is used.<br/>**Supports Expression Language: true**                                                                                                                              |
| Collection Name                          |               |                                                    | Collection to use inside the bucket scope. If not specified, the _default collection is used.<br/>**Supports Expression Language: true**                                                                                                              |
| **Document Type**                        | Json          | Json<br/>Binary<br/>String                         | Content type to store data as.                                                                                                                                                                                                                        |
| Document Id                              |               |                                                    | A static, fixed Couchbase document id, or an expression to construct the Couchbase document id. If not specified, either the FlowFile uuid attribute or if that's not found a generated uuid will be used.<br/>**Supports Expression Language: true** |
| **Persist To**                           | NONE          | NONE<br/>ACTIVE<br/>ONE<br/>TWO<br/>THREE<br/>FOUR | Durability constraint about disk persistence.                                                                                                                                                                                                         |
| **Replicate To**                         | NONE          | NONE<br/>ONE<br/>TWO<br/>THREE                     | Durability constraint about replication.                                                                                                                                                                                                              |

### Relationships

| Name    | Description                                                                                                |
|---------|------------------------------------------------------------------------------------------------------------|
| success | All FlowFiles that are written to Couchbase Server are routed to this relationship.                        |
| failure | All FlowFiles failed to be written to Couchbase Server and not retry-able are routed to this relationship. |
| retry   | All FlowFiles failed to be written to Couchbase Server but can be retried are routed to this relationship. |

### Output Attributes

| Attribute                     | Relationship | Description                                   |
|-------------------------------|--------------|-----------------------------------------------|
| couchbase.bucket              | success      | Bucket where the document was stored.         |
| couchbase.doc.id              | success      | Id of the document.                           |
| couchbase.doc.cas             | success      | CAS of the document.                          |
| couchbase.doc.sequence.number | success      | Sequence number associated with the document. |
| couchbase.partition.uuid      | success      | UUID of partition.                            |
| couchbase.partition.id        | success      | ID of partition (also known as vBucket).      |


## PutFile

### Description

Writes the contents of a FlowFile to the local file system

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                           | Default Value | Allowable Values            | Description                                                                                                                                                                          |
|--------------------------------|---------------|-----------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Permissions                    |               |                             | Sets the permissions on the output file to the value of this attribute. Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems.                                |
| Directory Permissions          |               |                             | Sets the permissions on the directories being created if 'Create Missing Directories' property is set. Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems. |
| Directory                      | .             |                             | The output directory to which to put files<br/>**Supports Expression Language: true**                                                                                                |
| Conflict Resolution Strategy   | fail          | fail<br/>replace<br/>ignore | Indicates what should happen when a file with the same name already exists in the output directory                                                                                   |
| **Create Missing Directories** | true          | true<br/>false              | If true, then missing destination directories will be created. If false, flowfiles are penalized and sent to failure.                                                                |
| Maximum File Count             | -1            |                             | Specifies the maximum number of files that can exist in the output directory                                                                                                         |

### Relationships

| Name    | Description                                                             |
|---------|-------------------------------------------------------------------------|
| success | All files are routed to success                                         |
| failure | Failed files (conflict, write failure, etc.) are transferred to failure |


## PutGCSObject

### Description

Puts flow files to a Google Cloud Storage Bucket.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                 | Default Value | Allowable Values                                                                                                                   | Description                                                                                                                                                        |
|--------------------------------------|---------------|------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **GCP Credentials Provider Service** |               |                                                                                                                                    | The Controller Service used to obtain Google Cloud Platform credentials. Should be the name of a GCPCredentialsControllerService.                                  |
| **Number of retries**                | 6             |                                                                                                                                    | How many retry attempts should be made before routing to the failure relationship.                                                                                 |
| Endpoint Override URL                |               |                                                                                                                                    | Overrides the default Google Cloud Storage endpoints<br/>**Supports Expression Language: true**                                                                    |
| Bucket                               | ${gcs.bucket} |                                                                                                                                    | Bucket of the object.<br/>**Supports Expression Language: true**                                                                                                   |
| Key                                  | ${filename}   |                                                                                                                                    | Name of the object.<br/>**Supports Expression Language: true**                                                                                                     |
| Content Type                         | ${mime.type}  |                                                                                                                                    | Content Type for the file, i.e. text/plain <br/>**Supports Expression Language: true**                                                                             |
| MD5 Hash                             |               |                                                                                                                                    | MD5 Hash (encoded in Base64) of the file for server-side validation.<br/>**Supports Expression Language: true**                                                    |
| CRC32C Checksum                      |               |                                                                                                                                    | CRC32C Checksum (encoded in Base64, big-Endian order) of the file for server-side validation.<br/>**Supports Expression Language: true**                           |
| Server Side Encryption Key           |               |                                                                                                                                    | An AES256 Encryption Key (encoded in base64) for server-side encryption of the object.<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true** |
| Object ACL                           |               | authenticatedRead<br/>bucketOwnerFullControl<br/>bucketOwnerRead<br/>private<br/>projectPrivate<br/>publicRead<br/>publicReadWrite | Access Control to be attached to the object uploaded. Not providing this will revert to bucket defaults.                                                           |
| Overwrite Object                     | true          | true<br/>false                                                                                                                     | If false, the upload to GCS will succeed only if the object does not exist.                                                                                        |

### Relationships

| Name    | Description                                                                                                  |
|---------|--------------------------------------------------------------------------------------------------------------|
| success | Files that have been successfully written to Google Cloud Storage are transferred to this relationship       |
| failure | Files that could not be written to Google Cloud Storage for some reason are transferred to this relationship |

### Output Attributes

| Attribute                | Relationship | Description                                                        |
|--------------------------|--------------|--------------------------------------------------------------------|
| gcs.status.message       | failure      | The status message received from google cloud.                     |
| gcs.error.reason         | failure      | The description of the error occurred during upload.               |
| gcs.error.domain         | failure      | The domain of the error occurred during upload.                    |
| gcs.bucket               | success      | Bucket of the object.                                              |
| gcs.key                  | success      | Name of the object.                                                |
| gcs.size                 | success      | Size of the object.                                                |
| gcs.crc32c               | success      | The CRC32C checksum of object's data, encoded in base64.           |
| gcs.md5                  | success      | The MD5 hash of the object's data, encoded in base64.              |
| gcs.owner.entity         | success      | The owner entity, in the form "user-emailAddress".                 |
| gcs.owner.entity.id      | success      | The ID for the entity.                                             |
| gcs.content.encoding     | success      | The content encoding of the object.                                |
| gcs.content.language     | success      | The content language of the object.                                |
| gcs.content.disposition  | success      | The data content disposition of the object.                        |
| gcs.media.link           | success      | The media download link to the object.                             |
| gcs.self.link            | success      | The link to this object.                                           |
| gcs.etag                 | success      | The HTTP 1.1 Entity tag for the object.                            |
| gcs.generated.id         | success      | The service-generated ID for the object.                           |
| gcs.generation           | success      | The content generation of this object. Used for object versioning. |
| gcs.metageneration       | success      | The metageneration of the object.                                  |
| gcs.create.time          | success      | Unix timestamp of the object's creation in milliseconds.           |
| gcs.update.time          | success      | Unix timestamp of the object's last modification in milliseconds.  |
| gcs.delete.time          | success      | Unix timestamp of the object's deletion in milliseconds.           |
| gcs.encryption.algorithm | success      | The algorithm used to encrypt the object.                          |
| gcs.encryption.sha256    | success      | The SHA256 hash of the key used to encrypt the object.             |


## PutKinesisStream

### Description

Sends the contents to a specified Amazon Kinesis. In order to send data to Kinesis, the stream name has to be specified.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                | Default Value | Allowable Values                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         | Description                                                                                                                                                                                                                                                                                                 |
|-------------------------------------|---------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Access Key                          |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | AWS account access key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                           |
| Secret Key                          |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | AWS account secret key<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                          |
| Credentials File                    |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey                                                                                                                                                                                 |
| AWS Credentials Provider service    |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.                                                                                                                                                                                                         |
| **Region**                          | us-west-2     | af-south-1<br/>ap-east-1<br/>ap-northeast-1<br/>ap-northeast-2<br/>ap-northeast-3<br/>ap-south-1<br/>ap-south-2<br/>ap-southeast-1<br/>ap-southeast-2<br/>ap-southeast-3<br/>ap-southeast-4<br/>ap-southeast-5<br/>ap-southeast-7<br/>ca-central-1<br/>ca-west-1<br/>cn-north-1<br/>cn-northwest-1<br/>eu-central-1<br/>eu-central-2<br/>eu-isoe-west-1<br/>eu-north-1<br/>eu-south-1<br/>eu-south-2<br/>eu-west-1<br/>eu-west-2<br/>eu-west-3<br/>il-central-1<br/>me-central-1<br/>me-south-1<br/>mx-central-1<br/>sa-east-1<br/>us-east-1<br/>us-east-2<br/>us-gov-east-1<br/>us-gov-west-1<br/>us-iso-east-1<br/>us-iso-west-1<br/>us-isob-east-1<br/>us-isof-east-1<br/>us-isof-south-1<br/>us-west-1<br/>us-west-2 | AWS Region                                                                                                                                                                                                                                                                                                  |
| **Communications Timeout**          | 30 sec        |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Sets the timeout of the communication between the AWS server and the client                                                                                                                                                                                                                                 |
| Endpoint Override URL               |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Endpoint URL to use instead of the AWS default including scheme, host, port, and path. The AWS libraries select an endpoint URL based on the AWS region, but this property overrides the selected endpoint URL, allowing use with other S3-compatible endpoints.<br/>**Supports Expression Language: true** |
| Proxy Host                          |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Proxy host name or IP<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                            |
| Proxy Port                          |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The port number of the proxy host<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                |
| Proxy Username                      |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Username to set when authenticating against proxy<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
| Proxy Password                      |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Password to set when authenticating against proxy<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                               |
| **Use Default Credentials**         | false         | true<br/>false                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc.                                                                                                                                                                 |
| **Amazon Kinesis Stream Name**      |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The name of Kinesis Stream<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                       |
| Amazon Kinesis Stream Partition Key |               |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The partition key attribute. If it is not set, a random value is used<br/>**Supports Expression Language: true**                                                                                                                                                                                            |
| Batch Size                          | 250           |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Batch size for messages. [1-500]                                                                                                                                                                                                                                                                            |
| Max Batch Data Size                 | 1 MB          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Soft cap on the data size of the batch to a single stream. (max 4MB)                                                                                                                                                                                                                                        |

### Relationships

| Name    | Description                                  |
|---------|----------------------------------------------|
| success | FlowFiles are routed to success relationship |
| failure | FlowFiles are routed to failure relationship |

### Output Attributes

| Attribute                   | Relationship | Description                                                 |
|-----------------------------|--------------|-------------------------------------------------------------|
| aws.kinesis.error.message   | failure      | Error message on posting message to AWS Kinesis             |
| aws.kinesis.error.code      | failure      | Error code for the message when posting to AWS Kinesis      |
| aws.kinesis.sequence.number | success      | Sequence number for the message when posting to AWS Kinesis |
| aws.kinesis.shard.id        | success      | Shard id of the message posted to AWS Kinesis               |


## PutOPCProcessor

### Description

Creates/updates OPC nodes

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                            | Default Value | Allowable Values                                                                  | Description                                                                                                                                                                                                                                                                                                        |
|---------------------------------|---------------|-----------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **OPC server endpoint**         |               |                                                                                   | Specifies the address, port and relative path of an OPC endpoint                                                                                                                                                                                                                                                   |
| Application URI                 |               |                                                                                   | Application URI of the client in the format 'urn:unconfigured:application'. Mandatory, if using Secure Channel and must match the URI included in the certificate's Subject Alternative Names.                                                                                                                     |
| Username                        |               |                                                                                   | Username to log in with.                                                                                                                                                                                                                                                                                           |
| Password                        |               |                                                                                   | Password to log in with.<br/>**Sensitive Property: true**                                                                                                                                                                                                                                                          |
| Certificate path                |               |                                                                                   | Path to the DER-encoded cert file                                                                                                                                                                                                                                                                                  |
| Key path                        |               |                                                                                   | Path to the DER-encoded key file                                                                                                                                                                                                                                                                                   |
| Trusted server certificate path |               |                                                                                   | Comma separated list of paths to the DER-encoded trusted server certificates                                                                                                                                                                                                                                       |
| Path reference types            |               |                                                                                   | Specify the reference types between nodes in the path if Path Node ID type is used. If not provided, all reference types are assumed to be Organizes. The format is 'referenceType1/referenceType2/.../referenceTypeN' and the supported reference types are Organizes, HasComponent, HasProperty, and HasSubtype. |
| **Parent node ID type**         |               | Path<br/>Int<br/>String                                                           | Specifies the type of the provided node ID                                                                                                                                                                                                                                                                         |
| **Parent node ID**              |               |                                                                                   | Specifies the ID of the root node to traverse                                                                                                                                                                                                                                                                      |
| **Parent node namespace index** | 0             |                                                                                   | The index of the namespace of the parent node.                                                                                                                                                                                                                                                                     |
| **Value type**                  |               | Int64<br/>UInt64<br/>Int32<br/>UInt32<br/>Boolean<br/>Float<br/>Double<br/>String | Set the OPC value type of the created nodes                                                                                                                                                                                                                                                                        |
| **Target node ID type**         |               | Int<br/>String                                                                    | ID type of target node. Allowed values are: Int, String.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
| **Target node ID**              |               |                                                                                   | ID of target node.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                      |
| Target node browse name         |               |                                                                                   | Browse name of target node. Only used when new node is created.<br/>**Supports Expression Language: true**                                                                                                                                                                                                         |
| **Target node namespace index** | 0             |                                                                                   | The index of the namespace of the target node.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                          |
| Create node reference type      | HasComponent  | Organizes<br/>HasComponent<br/>HasProperty<br/>HasSubtype                         | Reference type used when a new node is created.                                                                                                                                                                                                                                                                    |

### Relationships

| Name    | Description                  |
|---------|------------------------------|
| success | Successfully put OPC-UA node |
| failure | Failed to put OPC-UA node    |


## PutS3Object

### Description

Puts FlowFiles to an Amazon S3 Bucket. The upload uses either the PutS3Object method or the PutS3MultipartUpload method. The PutS3Object method sends the file in a single synchronous call, but it has a 5GB size limit. Larger files are sent using the PutS3MultipartUpload method. This multipart process saves state after each step so that a large upload can be resumed with minimal loss if the processor or cluster is stopped and restarted. A multipart upload consists of three steps: 1) initiate upload, 2) upload the parts, and 3) complete the upload. For multipart uploads, the processor saves state locally tracking the upload ID and parts uploaded, which must both be provided to complete the upload. The AWS libraries select an endpoint URL based on the AWS region, but this can be overridden with the 'Endpoint Override URL' property for use with other S3-compatible endpoints. The S3 API specifies that the maximum file size for a PutS3Object upload is 5GB. It also requires that parts in a multipart upload must be at least 5MB in size, except for the last part. These limits establish the bounds for the Multipart Upload Threshold and Part Size properties.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                   | Default Value            | Allowable Values                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         | Description                                                                                                                                                                                                                                                                                                 |
|----------------------------------------|--------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Access Key                             |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | AWS account access key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                           |
| Secret Key                             |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | AWS account secret key<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                          |
| Credentials File                       |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey                                                                                                                                                                                 |
| AWS Credentials Provider service       |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.                                                                                                                                                                                                         |
| **Region**                             | us-west-2                | af-south-1<br/>ap-east-1<br/>ap-northeast-1<br/>ap-northeast-2<br/>ap-northeast-3<br/>ap-south-1<br/>ap-south-2<br/>ap-southeast-1<br/>ap-southeast-2<br/>ap-southeast-3<br/>ap-southeast-4<br/>ap-southeast-5<br/>ap-southeast-7<br/>ca-central-1<br/>ca-west-1<br/>cn-north-1<br/>cn-northwest-1<br/>eu-central-1<br/>eu-central-2<br/>eu-isoe-west-1<br/>eu-north-1<br/>eu-south-1<br/>eu-south-2<br/>eu-west-1<br/>eu-west-2<br/>eu-west-3<br/>il-central-1<br/>me-central-1<br/>me-south-1<br/>mx-central-1<br/>sa-east-1<br/>us-east-1<br/>us-east-2<br/>us-gov-east-1<br/>us-gov-west-1<br/>us-iso-east-1<br/>us-iso-west-1<br/>us-isob-east-1<br/>us-isof-east-1<br/>us-isof-south-1<br/>us-west-1<br/>us-west-2 | AWS Region                                                                                                                                                                                                                                                                                                  |
| **Communications Timeout**             | 30 sec                   |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Sets the timeout of the communication between the AWS server and the client                                                                                                                                                                                                                                 |
| Endpoint Override URL                  |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Endpoint URL to use instead of the AWS default including scheme, host, port, and path. The AWS libraries select an endpoint URL based on the AWS region, but this property overrides the selected endpoint URL, allowing use with other S3-compatible endpoints.<br/>**Supports Expression Language: true** |
| Proxy Host                             |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Proxy host name or IP<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                            |
| Proxy Port                             |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The port number of the proxy host<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                |
| Proxy Username                         |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Username to set when authenticating against proxy<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
| Proxy Password                         |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Password to set when authenticating against proxy<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                               |
| Proxy Configuration Service            |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Specifies the Proxy Configuration Controller Service to proxy network requests. When used, this will override any values specified for Proxy Host, Proxy Port, Proxy Username, and Proxy Password properties.                                                                                               |
| **Use Default Credentials**            | false                    | true<br/>false                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc.                                                                                                                                                                 |
| **Bucket**                             |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The S3 bucket<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                    |
| Object Key                             |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | The key of the S3 object. If none is given the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                                                                                       |
| Content Type                           | application/octet-stream |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Sets the Content-Type HTTP header indicating the type of content stored in the associated object. The value of this header is a standard MIME type. If no content type is provided the default content type "application/octet-stream" will be used.<br/>**Supports Expression Language: true**             |
| **Storage Class**                      | Standard                 | Standard<br/>ReducedRedundancy<br/>StandardIA<br/>OnezoneIA<br/>IntelligentTiering<br/>Glacier<br/>DeepArchive<br/>Outposts<br/>GlacierIR<br/>Snow<br/>ExpressOneZone                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | AWS S3 Storage Class                                                                                                                                                                                                                                                                                        |
| **Server Side Encryption**             | None                     | None<br/>AES256<br/>aws_kms<br/>aws_kms_dsse                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             | Specifies the algorithm used for server side encryption.                                                                                                                                                                                                                                                    |
| FullControl User List                  |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have Full Control for an object.<br/>**Supports Expression Language: true**                                                                                                                                        |
| Read Permission User List              |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have Read Access for an object.<br/>**Supports Expression Language: true**                                                                                                                                         |
| Read ACL User List                     |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have permissions to read the Access Control List for an object.<br/>**Supports Expression Language: true**                                                                                                         |
| Write ACL User List                    |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have permissions to change the Access Control List for an object.<br/>**Supports Expression Language: true**                                                                                                       |
| Canned ACL                             |                          |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Amazon Canned ACL for an object. Allowed values: BucketOwnerFullControl, BucketOwnerRead, AuthenticatedRead, PublicReadWrite, PublicRead, Private, AwsExecRead; will be ignored if any other ACL/permission property is specified.<br/>**Supports Expression Language: true**                               |
| **Use Path Style Access**              | false                    | true<br/>false                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | Path-style access can be enforced by setting this property to true. Set it to true if your endpoint does not support virtual-hosted-style requests, only path-style requests.                                                                                                                               |
| **Multipart Threshold**                | 5 GB                     |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Specifies the file size threshold for switch from the PutS3Object API to the PutS3MultipartUpload API. Flow files bigger than this limit will be sent using the multipart process. The valid range is 5MB to 5GB.                                                                                           |
| **Multipart Part Size**                | 5 GB                     |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Specifies the part size for use when the PutS3Multipart Upload API is used. Flow files will be broken into chunks of this size for the upload process, but the last part sent can be smaller since it is not padded. The valid range is 5MB to 5GB.                                                         |
| **Multipart Upload AgeOff Interval**   | 60 min                   |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Specifies the interval at which existing multipart uploads in AWS S3 will be evaluated for ageoff. When processor is triggered it will initiate the ageoff evaluation if this interval has been exceeded.                                                                                                   |
| **Multipart Upload Max Age Threshold** | 7 days                   |                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | Specifies the maximum age for existing multipart uploads in AWS S3. When the ageoff process occurs, any upload older than this threshold will be aborted.                                                                                                                                                   |
| **Checksum Algorithm**                 | CRC64NVME                | CRC32<br/>CRC32C<br/>SHA1<br/>SHA256<br/>CRC64NVME                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       | Checksum algorithm used to verify the uploaded object.                                                                                                                                                                                                                                                      |

### Relationships

| Name    | Description                                  |
|---------|----------------------------------------------|
| success | FlowFiles are routed to success relationship |
| failure | FlowFiles are routed to failure relationship |


## PutSFTP

### Description

Sends FlowFiles to an SFTP Server

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                           | Default Value | Allowable Values                                           | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
|--------------------------------|---------------|------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Hostname**                   |               |                                                            | The fully qualified hostname or IP address of the remote system<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| **Port**                       |               |                                                            | The port that the remote system is listening on for file transfers<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| **Username**                   |               |                                                            | Username<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| Password                       |               |                                                            | Password for the user account<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| Private Key Path               |               |                                                            | The fully qualified path to the Private Key file<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Private Key Passphrase         |               |                                                            | Password for the private key<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| **Strict Host Key Checking**   | false         | true<br/>false                                             | Indicates whether or not strict enforcement of hosts keys should be applied                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Host Key File                  |               |                                                            | If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| **Connection Timeout**         | 30 sec        |                                                            | Amount of time to wait before timing out while creating a connection                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| **Data Timeout**               | 30 sec        |                                                            | When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| **Send Keep Alive On Timeout** | true          | true<br/>false                                             | Indicates whether or not to send a single Keep Alive message when SSH socket times out                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| Proxy Type                     | DIRECT        | DIRECT<br/>HTTP<br/>SOCKS                                  | Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. Supported proxies: HTTP + AuthN, SOCKS + AuthN                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| Proxy Host                     |               |                                                            | The fully qualified hostname or IP address of the proxy server<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| Proxy Port                     |               |                                                            | The port of the proxy server<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| Http Proxy Username            |               |                                                            | Http Proxy Username<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Http Proxy Password            |               |                                                            | Http Proxy Password<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| Remote Path                    |               |                                                            | The path on the remote system from which to pull or push files<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| **Create Directory**           | false         | true<br/>false                                             | Specifies whether or not the remote directory should be created if it does not exist.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| Disable Directory Listing      | false         | true<br/>false                                             | If set to 'true', directory listing is not performed prior to create missing directories. By default, this processor executes a directory listing command to see target directory existence before creating missing directories. However, there are situations that you might need to disable the directory listing such as the following. Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, then an error is returned because the directory already exists. |
| **Batch Size**                 | 500           |                                                            | The maximum number of FlowFiles to send in a single connection                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| **Conflict Resolution**        | NONE          | REPLACE<br/>IGNORE<br/>RENAME<br/>REJECT<br/>FAIL<br/>NONE | Determines how to handle the problem of filename collisions                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Reject Zero-Byte Files         | true          | true<br/>false                                             | Determines whether or not Zero-byte files should be rejected without attempting to transfer                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Dot Rename                     | true          | true<br/>false                                             | If true, then the filename of the sent file is prepended with a "." and then renamed back to the original once the file is completely sent. Otherwise, there is no rename. This property is ignored if the Temporary Filename property is set.                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Temporary Filename             |               |                                                            | If set, the filename of the sent file will be equal to the value specified during the transfer and after successful completion will be renamed to the original filename. If this value is set, the Dot Rename property is ignored.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                             |
| Last Modified Time             |               |                                                            | The lastModifiedTime to assign to the file after transferring it. If not set, the lastModifiedTime will not be changed. Format must be yyyy-MM-dd'T'HH:mm:ssZ. You may also use expression language such as ${file.lastModifiedTime}. If the value is invalid, the processor will not be invalid but will fail to change lastModifiedTime of the file.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                         |
| Permissions                    |               |                                                            | The permissions to assign to the file after transferring it. Format must be either UNIX rwxrwxrwx with a - in place of denied permissions (e.g. rw-r--r--) or an octal number (e.g. 644). If not set, the permissions will not be changed. You may also use expression language such as ${file.permissions}. If the value is invalid, the processor will not be invalid but will fail to change permissions of the file.<br/>**Supports Expression Language: true**                                                                                                                                                                                                       |
| Remote Owner                   |               |                                                            | Integer value representing the User ID to set on the file after transferring it. If not set, the owner will not be set. You may also use expression language such as ${file.owner}. If the value is invalid, the processor will not be invalid but will fail to change the owner of the file.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                  |
| Remote Group                   |               |                                                            | Integer value representing the Group ID to set on the file after transferring it. If not set, the group will not be set. You may also use expression language such as ${file.group}. If the value is invalid, the processor will not be invalid but will fail to change the group of the file.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                 |
| **Use Compression**            | false         | true<br/>false                                             | Indicates whether or not ZLIB compression should be used when transferring files                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |

### Relationships

| Name    | Description                                                                                          |
|---------|------------------------------------------------------------------------------------------------------|
| success | FlowFiles that are successfully sent will be routed to success                                       |
| reject  | FlowFiles that were rejected by the destination system                                               |
| failure | FlowFiles that failed to send to the remote system; failure is usually looped back to this processor |


## PutSmb

### Description

Writes the contents of a FlowFile to an smb network location

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                  | Default Value | Allowable Values            | Description                                                                                                                                                                          |
|---------------------------------------|---------------|-----------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **SMB Connection Controller Service** |               |                             | Specifies the SMB connection controller service to use for connecting to the SMB server. If the SMB share is auto-mounted to a drive letter, its recommended to use PutFile instead. |
| Directory                             | .             |                             | The output directory to which to put files<br/>**Supports Expression Language: true**                                                                                                |
| Conflict Resolution Strategy          | fail          | fail<br/>replace<br/>ignore | Indicates what should happen when a file with the same name already exists in the output directory                                                                                   |
| **Create Missing Directories**        | true          |                             | If true, then missing destination directories will be created. If false, flowfiles are penalized and sent to failure.                                                                |

### Relationships

| Name    | Description                                                             |
|---------|-------------------------------------------------------------------------|
| success | All files are routed to success                                         |
| failure | Failed files (conflict, write failure, etc.) are transferred to failure |


## PutSplunkHTTP

### Description

Sends the flow file contents to the specified Splunk HTTP Event Collector (see https://docs.splunk.com/Documentation/SplunkCloud/latest/Data/UsetheHTTPEventCollector) over HTTP or HTTPS.

The "Source", "Source Type", "Host" and "Index" properties are optional and will be set by Splunk if unspecified. If set,
the default values will be overwritten with the user specified ones. For more details about the Splunk API, please visit
[this documentation](https://docs.splunk.com/Documentation/Splunk/LATEST/RESTREF/RESTinput#services.2Fcollector.2Fraw)

HTTP Event Collector (HEC) in Splunk provides the possibility of index acknowledgement, which can be used to monitor
the indexing status of the individual events. PutSplunkHTTP supports this feature by enriching the outgoing flow file
with the necessary information, making it possible for a later processor to poll the status based on. The necessary
information for this is stored within flow file attributes "splunk.acknowledgement.id" and "splunk.responded.at".

For more refined processing, flow files are enriched with additional information if possible. The information is stored
in the flow file attribute "splunk.status.code" or "splunk.response.code", depending on the success of the processing.
The attribute "splunk.status.code" is always filled when the Splunk API call is executed and contains the HTTP status code
of the response. In case the flow file transferred into "failure" relationship, the "splunk.response.code" might be
also filled, based on the Splunk response code.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value | Allowable Values | Description                                                                                                                                                                                                                                |
|----------------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Hostname**               |               |                  | The ip address or hostname of the Splunk server.                                                                                                                                                                                           |
| **Port**                   | 8088          |                  | The HTTP Event Collector HTTP Port Number.                                                                                                                                                                                                 |
| **Token**                  |               |                  | HTTP Event Collector token starting with the string Splunk. For example 'Splunk 1234578-abcd-1234-abcd-1234abcd'<br/>**Sensitive Property: true**                                                                                          |
| **Splunk Request Channel** |               |                  | Identifier of the used request channel.                                                                                                                                                                                                    |
| SSL Context Service        |               |                  | The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.                                                                                                                                    |
| Source                     |               |                  | Basic field describing the source of the event. If unspecified, the event will use the default defined in splunk.<br/>**Supports Expression Language: true**                                                                               |
| Source Type                |               |                  | Basic field describing the source type of the event. If unspecified, the event will use the default defined in splunk.<br/>**Supports Expression Language: true**                                                                          |
| Host                       |               |                  | Basic field describing the host of the event. If unspecified, the event will use the default defined in splunk.<br/>**Supports Expression Language: true**                                                                                 |
| Index                      |               |                  | Identifies the index where to send the event. If unspecified, the event will use the default defined in splunk.<br/>**Supports Expression Language: true**                                                                                 |
| Content Type               |               |                  | The media type of the event sent to Splunk. If not set, "mime.type" flow file attribute will be used. In case of neither of them is specified, this information will not be sent to the server.<br/>**Supports Expression Language: true** |

### Relationships

| Name    | Description                                                                            |
|---------|----------------------------------------------------------------------------------------|
| success | FlowFiles that are sent successfully to the destination are sent to this relationship. |
| failure | FlowFiles that failed to be sent to the destination are sent to this relationship.     |


## PutSQL

### Description

Executes a SQL UPDATE or INSERT command. The content of an incoming FlowFile is expected to be the SQL command to execute. The SQL command may use the ? character to bind parameters. In this case, the parameters to use must exist as FlowFile attributes with the naming convention sql.args.N.type and sql.args.N.value, where N is a positive integer. The content of the FlowFile is expected to be in UTF-8 format.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                      | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                                                                                                                                                                                              |
|---------------------------|---------------|------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **DB Controller Service** |               |                  | Database Controller Service.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                  |
| SQL Statement             |               |                  | The SQL statement to execute. The statement can be empty, a constant value, or built from attributes using Expression Language. If this property is specified, it will be used regardless of the content of incoming flowfiles. If this property is empty, the content of the incoming flow file is expected to contain a valid SQL statement, to be issued by the processor to the database.<br/>**Supports Expression Language: true** |

### Relationships

| Name    | Description                                                              |
|---------|--------------------------------------------------------------------------|
| success | After a successful SQL update operation, the incoming FlowFile sent here |
| failure | Flow files that contain malformed sql statements                         |


## PutTCP

### Description

The PutTCP processor receives a FlowFile and transmits the FlowFile content over a TCP connection to the configured TCP server. By default, the FlowFiles are transmitted over the same TCP connection. To assist the TCP server with determining message boundaries, an optional "Outgoing Message Delimiter" string can be configured which is appended to the end of each FlowFiles content when it is transmitted over the TCP connection. An optional "Connection Per FlowFile" parameter can be specified to change the behaviour so that each FlowFiles content is transmitted over a single TCP connection which is closed after the FlowFile has been sent. Note: When using TLS 1.3 the processor can still route the flow file to success if the TLS handshake fails. This is due to TLS 1.3's faster handshake process which allows the message to be sent before we know the result of the TLS handshake.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                           | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                                                                                                                                                                        |
|--------------------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Hostname**                   | localhost     |                  | The ip address or hostname of the destination.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                          |
| **Port**                       |               |                  | The port or service on the destination.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                 |
| **Idle Connection Expiration** | 15 seconds    |                  | The amount of time a connection should be held open without being used before closing the connection. A value of 0 seconds will disable this feature.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                   |
| **Timeout**                    | 15 seconds    |                  | The timeout for connecting to and communicating with the destination.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                   |
| **Connection Per FlowFile**    | false         | true<br/>false   | Specifies whether to send each FlowFile's content on an individual connection.                                                                                                                                                                                                                                                                                                                                     |
| Outgoing Message Delimiter     |               |                  | Specifies the delimiter to use when sending messages out over the same TCP stream. The delimiter is appended to each FlowFile message that is transmitted over the stream so that the receiver can determine when one message ends and the next message begins. Users should ensure that the FlowFile content does not contain the delimiter character to avoid errors.<br/>**Supports Expression Language: true** |
| SSL Context Service            |               |                  | The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be sent over a secure connection.                                                                                                                                                                                                                                                                          |
| Max Size of Socket Send Buffer |               |                  | The maximum size of the socket send buffer that should be used. This is a suggestion to the Operating System to indicate how big the socket buffer should be.                                                                                                                                                                                                                                                      |

### Relationships

| Name    | Description                                                                |
|---------|----------------------------------------------------------------------------|
| success | FlowFiles that are sent to the destination are sent out this relationship. |
| failure | FlowFiles that encountered IO errors are sent out this relationship.       |


## PutUDP

### Description

The PutUDP processor receives a FlowFile and packages the FlowFile content into a single UDP datagram packet which is then transmitted to the configured UDP server. The processor doesn't guarantee a successful transfer, even if the flow file is routed to the success relationship.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name         | Default Value | Allowable Values | Description                                                                                                                                  |
|--------------|---------------|------------------|----------------------------------------------------------------------------------------------------------------------------------------------|
| **Hostname** | localhost     |                  | The ip address or hostname of the destination.<br/>**Supports Expression Language: true**                                                    |
| **Port**     |               |                  | The port on the destination. Can be a service name like ssh or http, as defined in /etc/services.<br/>**Supports Expression Language: true** |

### Relationships

| Name    | Description                                                                |
|---------|----------------------------------------------------------------------------|
| success | FlowFiles that are sent to the destination are sent out this relationship. |
| failure | FlowFiles that encountered IO errors are sent out this relationship.       |


## QueryDatabaseTable

### Description

Fetches all rows of a table, whose values in the specified Maximum-value Columns are larger than the previously-seen maxima. If that property is not provided, all rows are returned. The rows are grouped according to the value of Max Rows Per Flow File property and formatted as JSON.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value | Allowable Values     | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
|----------------------------|---------------|----------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **DB Controller Service**  |               |                      | Database Controller Service.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| **Output Format**          | JSON-Pretty   | JSON<br/>JSON-Pretty | Set the output format type.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| **Max Rows Per Flow File** | 0             |                      | The maximum number of result rows that will be included in a single FlowFile. This will allow you to break up very large result sets into multiple FlowFiles. If the value specified is zero, then all rows are returned in a single FlowFile.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| **Table Name**             |               |                      | The name of the database table to be queried.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| Columns to Return          |               |                      | A comma-separated list of column names to be used in the query. If your database requires special treatment of the names (quoting, e.g.), each name should include such treatment. If no column names are supplied, all columns in the specified table will be returned. NOTE: It is important to use consistent column names for a given table for incremental fetch to work properly.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| Maximum-value Columns      |               |                      | A comma-separated list of column names. The processor will keep track of the maximum value for each column that has been returned since the processor started running. Using multiple columns implies an order to the column list, and each column's values are expected to increase more slowly than the previous columns' values. Thus, using multiple columns implies a hierarchical structure of columns, which is usually used for partitioning tables. This processor can be used to retrieve only those rows that have been added/updated since the last retrieval. Note that some ODBC types such as bit/boolean are not conducive to maintaining maximum value, so columns of these types should not be listed in this property, and will result in error(s) during processing. If no columns are provided, all rows from the table will be considered, which could have a performance impact. NOTE: It is important to use consistent max-value column names for a given table for incremental fetch to work properly. NOTE: Because of a limitation of database access library 'soci', which doesn't support milliseconds in it's 'dt_date', there is a possibility that flowfiles might have duplicated records, if a max-value column with 'dt_date' type has value with milliseconds.<br/>**Supports Expression Language: true** |
| Where Clause               |               |                      | A custom clause to be added in the WHERE condition when building SQL queries.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |

### Dynamic Properties

| Name                                | Value                                          | Description                                                                                                                                                                                                                                                                                 |
|-------------------------------------|------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| initial.maxvalue.<max_value_column> | Initial maximum value for the specified column | Specifies an initial max value for max value column(s). Properties should be added in the format `initial.maxvalue.<max_value_column>`. This value is only used the first time the table is accessed (when a Maximum Value Column is specified).<br/>**Supports Expression Language: true** |

### Relationships

| Name    | Description                                              |
|---------|----------------------------------------------------------|
| success | Successfully created FlowFile from SQL query result set. |


## QuerySplunkIndexingStatus

### Description

Queries the Splunk server in order to acquire the status of indexing acknowledgement.

This processor is responsible for polling Splunk server and determine if a Splunk event is acknowledged at the time of
execution. For more details about the HEC Index Acknowledgement please see
https://docs.splunk.com/Documentation/Splunk/LATEST/Data/AboutHECIDXAck.

In order to work properly, the incoming flow files need to have the attributes "splunk.acknowledgement.id" and
"splunk.responded.at" filled properly. The flow file attribute "splunk.acknowledgement.id" should contain the "ackId"
which can be extracted from the response to the original Splunk put call. The flow file attribute "splunk.responded.at"
should contain the timestamp describing when the put call was answered by Splunk.
These required attributes are set by PutSplunkHTTP processor.

Undetermined cases are normal in healthy environment as it is possible that minifi asks for indexing status before Splunk
finishes and acknowledges it. These cases are safe to retry, and it is suggested to loop "undetermined" relationship
back to the processor for later try. Flow files transferred into the "Undetermined" relationship are penalized.

Please keep Splunk channel limitations in mind: there are multiple configuration parameters in Splunk which might have direct
effect on the performance and behaviour of the QuerySplunkIndexingStatus processor. For example "max_number_of_acked_requests_pending_query"
and "max_number_of_acked_requests_pending_query_per_ack_channel" might limit the amount of ackIDs Splunk stores.

Also, it is suggested to execute the query in batches. The "Maximum Query Size" property might be used for fine tune
the maximum number of events the processor will query about in one API request. This serves as an upper limit for the
batch but the processor might execute the query with smaller number of undetermined events.


### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                                       |
|----------------------------|---------------|------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Hostname**               |               |                  | The ip address or hostname of the Splunk server.                                                                                                                                                                                                                                  |
| **Port**                   | 8088          |                  | The HTTP Event Collector HTTP Port Number.                                                                                                                                                                                                                                        |
| **Token**                  |               |                  | HTTP Event Collector token starting with the string Splunk. For example 'Splunk 1234578-abcd-1234-abcd-1234abcd'<br/>**Sensitive Property: true**                                                                                                                                 |
| **Splunk Request Channel** |               |                  | Identifier of the used request channel.                                                                                                                                                                                                                                           |
| SSL Context Service        |               |                  | The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.                                                                                                                                                                           |
| **Maximum Waiting Time**   | 1 hour        |                  | The maximum time the processor tries to acquire acknowledgement confirmation for an index, from the point of registration. After the given amount of time, the processor considers the index as not acknowledged and transfers the FlowFile to the "unacknowledged" relationship. |
| **Maximum Query Size**     | 1000          |                  | The maximum number of acknowledgement identifiers the outgoing query contains in one batch. It is recommended not to set it too low in order to reduce network communication.                                                                                                     |

### Relationships

| Name           | Description                                                                                                                                                                                                                                                                                                                                                    |
|----------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| acknowledged   | A FlowFile is transferred to this relationship when the acknowledgement was successful.                                                                                                                                                                                                                                                                        |
| unacknowledged | A FlowFile is transferred to this relationship when the acknowledgement was not successful. This can happen when the acknowledgement did not happened within the time period set for Maximum Waiting Time. FlowFiles with acknowledgement id unknown for the Splunk server will be transferred to this relationship after the Maximum Waiting Time is reached. |
| undetermined   | A FlowFile is transferred to this relationship when the acknowledgement state is not determined. FlowFiles transferred to this relationship might be penalized. This happens when Splunk returns with HTTP 200 but with false response for the acknowledgement id in the flow file attribute.                                                                  |
| failure        | A FlowFile is transferred to this relationship when the acknowledgement was not successful due to errors during the communication, or if the flowfile was missing the acknowledgement id                                                                                                                                                                       |


## ReplaceText

### Description

Updates the content of a FlowFile by replacing parts of it using various replacement strategies.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                         | Default Value | Allowable Values                                                                                     | Description                                                                                                                                                                                                                                                                                                                                                                                                                                             |
|------------------------------|---------------|------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Evaluation Mode**          | Line-by-Line  | Line-by-Line<br/>Entire text                                                                         | Run the 'Replacement Strategy' against each line separately (Line-by-Line) or against the whole input treated as a single string (Entire Text).                                                                                                                                                                                                                                                                                                         |
| Line-by-Line Evaluation Mode | All           | All<br/>First-Line<br/>Last-Line<br/>Except-First-Line<br/>Except-Last-Line                          | Run the 'Replacement Strategy' against each line separately (Line-by-Line) for All lines in the FlowFile, First Line (Header) only, Last Line (Footer) only, all Except the First Line (Header) or all Except the Last Line (Footer).                                                                                                                                                                                                                   |
| **Replacement Strategy**     | Regex Replace | Prepend<br/>Append<br/>Regex Replace<br/>Literal Replace<br/>Always Replace<br/>Substitute Variables | The strategy for how and what to replace within the FlowFile's text content. Substitute Variables replaces ${attribute_name} placeholders with the corresponding attribute's value (if an attribute is not found, the placeholder is kept as it was).                                                                                                                                                                                                   |
| Search Value                 |               |                                                                                                      | The Search Value to search for in the FlowFile content. Only used for 'Literal Replace' and 'Regex Replace' matching strategies. Supports expression language except in Regex Replace mode.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                  |
| **Replacement Value**        |               |                                                                                                      | The value to insert using the 'Replacement Strategy'. Using 'Regex Replace' back-references to Regular Expression capturing groups are supported: $& is the entire matched substring, $1, $2, ... are the matched capturing groups. Use $$1 for a literal $1. Back-references to non-existent capturing groups will be replaced by empty strings. Supports expression language except in Regex Replace mode.<br/>**Supports Expression Language: true** |

### Relationships

| Name    | Description                                                                                                                                                  |
|---------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
| success | FlowFiles that have been successfully processed are routed to this relationship. This includes both FlowFiles that had text replaced and those that did not. |
| failure | FlowFiles that could not be updated are routed to this relationship.                                                                                         |


## RetryFlowFile

### Description

FlowFiles passed to this Processor have a 'Retry Attribute' value checked against a configured 'Maximum Retries' value. If the current attribute value is below the configured maximum, the FlowFile is passed to a retry relationship. The FlowFile may or may not be penalized in that condition. If the FlowFile's attribute value exceeds the configured maximum, the FlowFile will be passed to a 'retries_exceeded' relationship. WARNING: If the incoming FlowFile has a non-numeric value in the configured 'Retry Attribute' attribute, it will be reset to '1'. You may choose to fail the FlowFile instead of performing the reset. Additional dynamic properties can be defined for any attributes you wish to add to the FlowFiles transferred to 'retries_exceeded'. These attributes support attribute expression language.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                | Default Value    | Allowable Values                                | Description                                                                                                                                                                                                                                                                                                                        |
|-------------------------------------|------------------|-------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Retry Attribute**                 | flowfile.retries |                                                 | The name of the attribute that contains the current retry count for the FlowFile.WARNING: If the name matches an attribute already on the FlowFile that does not contain a numerical value, the processor will either overwrite that attribute with '1' or fail based on configuration.<br/>**Supports Expression Language: true** |
| **Maximum Retries**                 | 3                |                                                 | The maximum number of times a FlowFile can be retried before being passed to the 'retries_exceeded' relationship.<br/>**Supports Expression Language: true**                                                                                                                                                                       |
| **Penalize Retries**                | true             | true<br/>false                                  | If set to 'true', this Processor will penalize input FlowFiles before passing them to the 'retry' relationship. This does not apply to the 'retries_exceeded' relationship.                                                                                                                                                        |
| **Fail on Non-numerical Overwrite** | false            | true<br/>false                                  | If the FlowFile already has the attribute defined in 'Retry Attribute' that is *not* a number, fail the FlowFile instead of resetting that value to '1'                                                                                                                                                                            |
| **Reuse Mode**                      | Fail on Reuse    | Fail on Reuse<br/>Warn on Reuse<br/>Reset Reuse | Defines how the Processor behaves if the retry FlowFile has a different retry UUID than the instance that received the FlowFile. This generally means that the attribute was not reset after being successfully retried by a previous instance of this processor.                                                                  |

### Dynamic Properties

| Name                            | Value                                            | Description                                                                                                                                                         |
|---------------------------------|--------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Exceeded FlowFile Attribute Key | The value of the attribute added to the FlowFile | One or more dynamic properties can be used to add attributes to FlowFiles passed to the 'retries_exceeded' relationship.<br/>**Supports Expression Language: true** |

### Relationships

| Name             | Description                                                                                                                                                                                                                                                                                                                     |
|------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| retry            | Input FlowFile has not exceeded the configured maximum retry count, pass this relationship back to the input Processor to create a limited feedback loop.                                                                                                                                                                       |
| retries_exceeded | Input FlowFile has exceeded the configured maximum retry count, do not pass this relationship back to the input Processor to terminate the limited feedback loop.                                                                                                                                                               |
| failure          | The processor is configured such that a non-numerical value on 'Retry Attribute' results in a failure instead of resetting that value to '1'. This will immediately terminate the limited feedback loop. Might also include when 'Maximum Retries' contains  attribute expression language that does not resolve to an Integer. |

### Output Attributes

| Attribute             | Relationship | Description                                                                                                             |
|-----------------------|--------------|-------------------------------------------------------------------------------------------------------------------------|
| Retry Attribute       |              | User defined retry attribute is updated with the current retry count                                                    |
| Retry Attribute .uuid |              | User defined retry attribute with .uuid suffix is updated with the UUID of the processor that retried the FlowFile last |


## RouteOnAttribute

### Description

Routes FlowFiles based on their Attributes using the Attribute Expression Language.

Any number of user-defined dynamic properties can be added, which all support the Attribute Expression Language. Relationships matching the name of the properties will be added.
FlowFiles will be routed to all the relationships whose matching property evaluates to "true". Unmatched FlowFiles will be routed to the "unmatched" relationship, while failed ones to "failure".

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
|------|---------------|------------------|-------------|

### Relationships

| Name      | Description                                             |
|-----------|---------------------------------------------------------|
| unmatched | Files which do not match any expression are routed here |
| failure   | Failed files are transferred to failure                 |


## RouteText

### Description

Routes textual data based on a set of user-defined rules. Each segment in an incoming FlowFile is compared against the values specified by user-defined Properties. The mechanism by which the text is compared to these user-defined properties is defined by the 'Matching Strategy'. The data is then routed according to these rules, routing each segment of the text individually.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                   | Default Value   | Allowable Values                                                                                                | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
|----------------------------------------|-----------------|-----------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Routing Strategy**                   | Dynamic Routing | Dynamic Routing<br/>Route On All<br/>Route On Any                                                               | Specifies how to determine which Relationship(s) to use when evaluating the segments of incoming text against the 'Matching Strategy' and user-defined properties. 'Dynamic Routing' routes to all the matching dynamic relationships (or 'unmatched' if none matches). 'Route On All' routes to 'matched' iff all dynamic relationships match. 'Route On Any' routes to 'matched' iff any of the dynamic relationships match.                                                                                                                                                                                                                                                                                                                                                  |
| **Matching Strategy**                  |                 | Starts With<br/>Ends With<br/>Contains<br/>Equals<br/>Matches Regex<br/>Contains Regex<br/>Satisfies Expression | Specifies how to evaluate each segment of incoming text against the user-defined properties. Possible values are: 'Starts With', 'Ends With', 'Contains', 'Equals', 'Matches Regex', 'Contains Regex', 'Satisfies Expression'.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| **Ignore Leading/Trailing Whitespace** | true            | true<br/>false                                                                                                  | Indicates whether or not the whitespace at the beginning and end should be ignored when evaluating a segment.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| **Ignore Case**                        | false           | true<br/>false                                                                                                  | If true, capitalization will not be taken into account when comparing values. E.g., matching against 'HELLO' or 'hello' will have the same result. This property is ignored if the 'Matching Strategy' is set to 'Satisfies Expression'.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| Grouping Regular Expression            |                 |                                                                                                                 | Specifies a Regular Expression to evaluate against each segment to determine which Group it should be placed in. The Regular Expression must have at least one Capturing Group that defines the segment's Group. If multiple Capturing Groups exist in the Regular Expression, the values from all Capturing Groups will be joined together with ", ". Two segments will not be placed into the same FlowFile unless they both have the same value for the Group (or neither matches the Regular Expression). For example, to group together all lines in a CSV File by the first column, we can set this value to "(.*?),.*" (and use "Per Line" segmentation). Two segments that have the same Group but different Relationships will never be placed into the same FlowFile. |
| Grouping Fallback Value                |                 |                                                                                                                 | If the 'Grouping Regular Expression' is specified and the matching fails, this value will be considered the group of the segment.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| **Segmentation Strategy**              | Per Line        | Full Text<br/>Per Line                                                                                          | Specifies what portions of the FlowFile content constitutes a single segment to be processed. 'Full Text' considers the whole content as a single segment, 'Per Line' considers each line of the content as a separate segment                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |

### Dynamic Properties

| Name              | Value                  | Description                                                                                                                                                                      |
|-------------------|------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Relationship Name | value to match against | Routes data that matches the value specified in the Dynamic Property Value to the Relationship specified in the Dynamic Property Key.<br/>**Supports Expression Language: true** |

### Relationships

| Name      | Description                                                                                      |
|-----------|--------------------------------------------------------------------------------------------------|
| original  | The original input file will be routed to this destination                                       |
| unmatched | Segments that do not satisfy the required user-defined rules will be routed to this Relationship |
| matched   | Segments that satisfy the required user-defined rules will be routed to this Relationship        |

### Output Attributes

| Attribute       | Relationship | Description                                                                                                                                              |
|-----------------|--------------|----------------------------------------------------------------------------------------------------------------------------------------------------------|
| RouteText.Group |              | The value captured by all capturing groups in the 'Grouping Regular Expression' property. If this property is not set, this attribute will not be added. |


## TailEventLog

### Description

Windows event log reader that functions as a stateful tail of the provided windows event log name

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                    | Default Value | Allowable Values | Description                          |
|-------------------------|---------------|------------------|--------------------------------------|
| Log Source              |               |                  | Log Source from which to read events |
| Max Events Per FlowFile | 1             |                  | Events per flow file                 |

### Relationships

| Name    | Description                                             |
|---------|---------------------------------------------------------|
| success | All files, containing log events, are routed to success |


## SegmentContent

### Description

Segments a FlowFile into multiple smaller segments on byte boundaries.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name             | Default Value | Allowable Values | Description                                                                                |
|------------------|---------------|------------------|--------------------------------------------------------------------------------------------|
| **Segment Size** |               |                  | The maximum data size in bytes for each segment<br/>**Supports Expression Language: true** |

### Relationships

| Name     | Description                                                                                                                                                                     |
|----------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| original | The original FlowFile will be sent to this relationship                                                                                                                         |
| segments | All segments will be sent to this relationship. If the file was small enough that it was not segmented, a copy of the original is sent to this relationship as well as original |

### Output Attributes

| Attribute                 | Relationship | Description                                                                                                                                                |
|---------------------------|--------------|------------------------------------------------------------------------------------------------------------------------------------------------------------|
| fragment.identifier       |              | All segments produced from the same parent FlowFile will have this attribute set to the same UUID (which is the UUID of the parent FlowFile, if available) |
| fragment.index            |              | A sequence number starting with 1 that indicates the ordering of the segments that were created from a single parent FlowFile                              |
| fragment.count            |              | The number of segments generated from the parent FlowFile                                                                                                  |
| segment.original.filename |              | The filename of the parent FlowFile                                                                                                                        |


## SplitContent

### Description

Splits incoming FlowFiles by a specified byte sequence

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value | Allowable Values     | Description                                                                                                                                                                                                           |
|----------------------------|---------------|----------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Byte Sequence Format**   | Hexadecimal   | Hexadecimal<br/>Text | Specifies how the <Byte Sequence> property should be interpreted                                                                                                                                                      |
| **Byte Sequence**          |               |                      | A representation of bytes to look for and upon which to split the source file into separate files                                                                                                                     |
| **Keep Byte Sequence**     | false         | true<br/>false       | Determines whether or not the Byte Sequence should be included with each Split                                                                                                                                        |
| **Byte Sequence Location** | Trailing      | Trailing<br/>Leading | If <Keep Byte Sequence> is set to true, specifies whether the byte sequence should be added to the end of the first split or the beginning of the second; if <Keep Byte Sequence> is false, this property is ignored. |

### Relationships

| Name     | Description                                          |
|----------|------------------------------------------------------|
| original | The original file                                    |
| splits   | All Splits will be routed to the splits relationship |

### Output Attributes

| Attribute                 | Relationship | Description                                                                                                                    |
|---------------------------|--------------|--------------------------------------------------------------------------------------------------------------------------------|
| fragment.identifier       |              | All split FlowFiles produced from the same parent FlowFile will have the same randomly generated UUID added for this attribute |
| fragment.index            |              | A one-up number that indicates the ordering of the split FlowFiles that were created from a single parent FlowFile             |
| fragment.count            |              | The number of split FlowFiles generated from the parent FlowFile                                                               |
| segment.original.filename |              | The filename of the parent FlowFile                                                                                            |


## SplitJson

### Description

Splits a JSON File into multiple, separate FlowFiles for an array element specified by a JsonPath expression. Each generated FlowFile is comprised of an element of the specified array and transferred to relationship 'split,' with the original file transferred to the 'original' relationship. If the specified JsonPath is not found or does not evaluate to an array element, the original file is routed to 'failure' and no files are generated.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                          | Default Value | Allowable Values                   | Description                                                                                 |
|-------------------------------|---------------|------------------------------------|---------------------------------------------------------------------------------------------|
| **JsonPath Expression**       |               |                                    | A JsonPath expression that indicates the array element to split into JSON/scalar fragments. |
| **Null Value Representation** | empty string  | empty string<br/>the string 'null' | Indicates the desired representation of JSON Path expressions resulting in a null value.    |

### Relationships

| Name     | Description                                                                                                                                                              |
|----------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| failure  | If a FlowFile fails processing for any reason (for example, the FlowFile is not valid JSON or the specified path does not exist), it will be routed to this relationship |
| original | The original FlowFile that was split into segments. If the FlowFile fails processing, nothing will be sent to this relationship                                          |
| split    | All segments of the original FlowFile will be routed to this relationship                                                                                                |

### Output Attributes

| Attribute                 | Relationship    | Description                                                                                                                    |
|---------------------------|-----------------|--------------------------------------------------------------------------------------------------------------------------------|
| fragment.identifier       | split, original | All split FlowFiles produced from the same parent FlowFile will have the same randomly generated UUID added for this attribute |
| fragment.index            | split           | A one-up number that indicates the ordering of the split FlowFiles that were created from a single parent FlowFile             |
| fragment.count            | split, original | The number of split FlowFiles generated from the parent FlowFile                                                               |
| segment.original.filename | split           | The filename of the parent FlowFile                                                                                            |


## SplitRecord

### Description

Splits up an input FlowFile that is in a record-oriented data format into multiple smaller FlowFiles

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                  | Default Value | Allowable Values | Description                                                                                                                   |
|-----------------------|---------------|------------------|-------------------------------------------------------------------------------------------------------------------------------|
| **Record Reader**     |               |                  | Specifies the Controller Service to use for reading incoming data                                                             |
| **Record Writer**     |               |                  | Specifies the Controller Service to use for writing out the records                                                           |
| **Records Per Split** |               |                  | Specifies how many records should be written to each 'split' or 'segment' FlowFile<br/>**Supports Expression Language: true** |

### Relationships

| Name     | Description                                                                                                                                                       |
|----------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| failure  | If a FlowFile cannot be transformed from the configured input format to the configured output format, the unchanged FlowFile will be routed to this relationship. |
| splits   | The individual 'segments' of the original FlowFile will be routed to this relationship.                                                                           |
| original | Upon successfully splitting an input FlowFile, the original FlowFile will be sent to this relationship.                                                           |

### Output Attributes

| Attribute                 | Relationship | Description                                                                                                                                                       |
|---------------------------|--------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| record.count              | splits       | The number of records in the FlowFile. This is added to FlowFiles that are routed to the 'splits' Relationship.                                                   |
| fragment.identifier       | splits       | All split FlowFiles produced from the same parent FlowFile will have this attribute set to the same UUID (which is the UUID of the parent FlowFile, if available) |
| fragment.index            | splits       | A one-up number that indicates the ordering of the split FlowFiles that were created from a single parent FlowFile                                                |
| fragment.count            | splits       | The number of split FlowFiles generated from the parent FlowFile                                                                                                  |
| segment.original.filename | splits       | The filename of the parent FlowFile                                                                                                                               |


## SplitText

### Description

Splits a text file into multiple smaller text files on line boundaries limited by maximum number of lines or total size of fragment. Each output split file will contain no more than the configured number of lines or bytes. If both Line Split Count and Maximum Fragment Size are specified, the split occurs at whichever limit is reached first. If the first line of a fragment exceeds the Maximum Fragment Size, that line will be output in a single split file which exceeds the configured maximum size limit. This component also allows one to specify that each split should include a header lines. Header lines can be computed by either specifying the amount of lines that should constitute a header or by using header marker to match against the read lines. If such match happens then the corresponding line will be treated as header. Keep in mind that upon the first failure of header marker match, no more matches will be performed and the rest of the data will be parsed as regular lines for a given split. If after computation of the header there are no more data, the resulting split will consists of only header lines.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                          | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
|-------------------------------|---------------|------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Line Split Count**          |               |                  | The number of lines that will be added to each split file, excluding header lines. A value of zero requires Maximum Fragment Size to be set, and line count will not be considered in determining splits.                                                                                                                                                                                                                                                                                                   |
| Maximum Fragment Size         |               |                  | The maximum size of each split file, including header lines. NOTE: in the case where a single line exceeds this property (including headers, if applicable), that line will be output in a split of its own which exceeds this Maximum Fragment Size setting.                                                                                                                                                                                                                                               |
| **Header Line Count**         | 0             |                  | The number of lines that should be considered part of the header; the header lines will be duplicated to all split files.                                                                                                                                                                                                                                                                                                                                                                                   |
| Header Line Marker Characters |               |                  | The first character(s) on the line of the datafile which signifies a header line. This value is ignored when Header Line Count is non-zero. The first line not containing the Header Line Marker Characters and all subsequent lines are considered non-header                                                                                                                                                                                                                                              |
| **Remove Trailing Newlines**  | true          | true<br/>false   | Whether to remove newlines at the end of each split file. This should be false if you intend to merge the split files later. If this is set to 'true' and a FlowFile is generated that contains only 'empty lines' (i.e., consists only of \r and \n characters), the FlowFile will not be emitted. Note, however, that if header lines are specified, the resultant FlowFile will never be empty as it will consist of the header lines, so a FlowFile may be emitted that contains only the header lines. |

### Relationships

| Name     | Description                                                                                                                          |
|----------|--------------------------------------------------------------------------------------------------------------------------------------|
| failure  | If a file cannot be split for some reason, the original file will be routed to this destination and nothing will be routed elsewhere |
| original | The original input file will be routed to this destination when it has been successfully split into 1 or more files                  |
| splits   | The split files will be routed to this destination when an input file is successfully split into 1 or more split files               |

### Output Attributes

| Attribute                 | Relationship | Description                                                                                                                                                   |
|---------------------------|--------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------|
| text.line.count           |              | The number of lines of text from the original FlowFile that were copied to this FlowFile (does not count empty lines)                                         |
| fragment.size             |              | The number of bytes from the original FlowFile that were copied to this FlowFile, including header, if applicable, which is duplicated in each split FlowFile |
| fragment.identifier       |              | All split FlowFiles produced from the same parent FlowFile will have the same randomly generated UUID added for this attribute                                |
| fragment.index            |              | A one-up number that indicates the ordering of the split FlowFiles that were created from a single parent FlowFile                                            |
| fragment.count            |              | The number of split FlowFiles generated from the parent FlowFile                                                                                              |
| segment.original.filename |              | The filename of the parent FlowFile                                                                                                                           |


## TailFile

### Description

"Tails" a file, or a list of files, ingesting data from the file as it is written to the file. The file is expected to be textual. Data is ingested only when a new line is encountered (carriage return or new-line character or combination). If the file to tail is periodically "rolled over", as is generally the case with log files, an optional Rolling Filename Pattern can be used to retrieve data from files that have rolled over, even if the rollover occurred while NiFi was not running (provided that the data still exists upon restart of NiFi). It is generally advisable to set the Run Schedule to a few seconds, rather than running with the default value of 0 secs, as this Processor will consume a lot of resources if scheduled very aggressively. At this time, this Processor does not support ingesting files that have been compressed when 'rolled over'.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value           | Allowable Values                                         | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
|----------------------------|-------------------------|----------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **File to Tail**           |                         |                                                          | Fully-qualified filename of the file that should be tailed when using single file mode, or a file regex when using multifile mode                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| State File                 | TailFileState           |                                                          | DEPRECATED. Only use it for state migration from the legacy state file.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Input Delimiter            | \n                      |                                                          | Specifies the character that should be used for delimiting the data being tailedfrom the incoming file. If none is specified, data will be ingested as it becomes available.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| **tail-mode**              | Single file             | Single file<br/>Multiple file                            | Specifies the tail file mode. In 'Single file' mode only a single file will be watched. In 'Multiple file' mode a regex may be used. Note that in multiple file mode we will still continue to watch for rollover on the initial set of watched files. The Regex used to locate multiple files will be run during the schedule phrase. Note that if rotated files are matched by the regex, those files will be tailed.                                                                                                                                                                                                                                                                                               |
| tail-base-directory        |                         |                                                          | Base directory used to look for files to tail. This property is required when using Multiple file mode. Can contain expression language placeholders if Attribute Provider Service is set.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| Recursive lookup           | false                   | true<br/>false                                           | When using Multiple file mode, this property determines whether files are tailed in child directories of the Base Directory or not.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| Lookup frequency           | 10 min                  |                                                          | When using Multiple file mode, this property specifies the minimum duration the processor will wait between looking for new files to tail in the Base Directory.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| Rolling Filename Pattern   | ${filename}.*           |                                                          | If the file to tail "rolls over" as would be the case with log files, this filename pattern will be used to identify files that have rolled over so MiNiFi can read the remaining of the rolled-over file and then continue with the new log file. This pattern supports the wildcard characters * and ?, it also supports the notation ${filename} to specify a pattern based on the name of the file (without extension), and will assume that the files that have rolled over live in the same directory as the file being tailed.                                                                                                                                                                                 |
| **Initial Start Position** | Beginning of File       | Beginning of Time<br/>Beginning of File<br/>Current Time | When the Processor first begins to tail data, this property specifies where the Processor should begin reading data. Once data has been ingested from a file, the Processor will continue from the last point from which it has received data.<br/>Beginning of Time: Start with the oldest data that matches the Rolling Filename Pattern and then begin reading from the File to Tail.<br/>Beginning of File: Start with the beginning of the File to Tail. Do not ingest any data that has already been rolled over.<br/>Current Time: Start with the data at the end of the File to Tail. Do not ingest any data that has already been rolled over or any data in the File to Tail that has already been written. |
| Attribute Provider Service |                         |                                                          | Provides a list of key-value pair records which can be used in the Base Directory property using Expression Language. Requires Multiple file mode.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| **Batch Size**             | 0                       |                                                          | Maximum number of lines emitted in a single trigger. If set to 0 all new content will be processed.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| **Result Mode**            | Flow file per delimiter | Flow file per delimiter<br/>Flow file per batch          | Specifies how the result lines are arranged into output flow files                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |

### Relationships

| Name    | Description                     |
|---------|---------------------------------|
| success | All files are routed to success |


## UnfocusArchiveEntry

### Description

Restores a FlowFile which has had an archive entry focused via FocusArchiveEntry to its original state.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
|------|---------------|------------------|-------------|

### Relationships

| Name    | Description                            |
|---------|----------------------------------------|
| success | success operational on the flow record |


## UpdateAttribute

### Description

This processor updates the attributes of a FlowFile using properties that are added by the user. This allows you to set default attribute changes that affect every FlowFile going through the processor, equivalent to the "basic" usage in Apache NiFi.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
|------|---------------|------------------|-------------|

### Relationships

| Name    | Description                             |
|---------|-----------------------------------------|
| success | All files are routed to success         |
| failure | Failed files are transferred to failure |
