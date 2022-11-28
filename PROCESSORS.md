<!--Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements.  See the NOTICE file distributed withthis work for additional information regarding copyright ownership.The ASF licenses this file to You under the Apache License, Version 2.0(the "License"); you may not use this file except in compliance withthe License.  You may obtain a copy of the License at    http://www.apache.org/licenses/LICENSE-2.0Unless required by applicable law or agreed to in writing, softwaredistributed under the License is distributed on an "AS IS" BASIS,WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.See the License for the specific language governing permissions andlimitations under the License.-->
# Processors

## Table of Contents

- [AppendHostInfo](#appendhostinfo)
- [ApplyTemplate](#applytemplate)
- [AttributesToJSON](#AttributesToJSON)
- [BinFiles](#binfiles)
- [CapturePacket](#capturepacket)
- [CaptureRTSPFrame](#capturertspframe)
- [CollectorInitiatedSubscription](#collectorinitiatedsubscription)
- [CompressContent](#compresscontent)
- [ConsumeJournald](#consumejournald)
- [ConsumeKafka](#consumekafka)
- [ConsumeMQTT](#consumemqtt)
- [ConsumeWindowsEventLog](#consumewindowseventlog)
- [DefragmentText](#defragmenttext)
- [DeleteAzureBlobStorage](#deleteazureblobstorage)
- [DeleteAzureDataLakeStorage](#deleteazuredatalakestorage)
- [DeleteGCSObject](#deletegcsobject)
- [DeleteS3Object](#deletes3object)
- [ExecuteProcess](#executeprocess)
- [ExecutePythonProcessor](#executepythonprocessor)
- [ExecuteSQL](#executesql)
- [ExecuteScript](#executescript)
- [ExtractText](#extracttext)
- [FetchAzureBlobStorage](#fetchazureblobstorage)
- [FetchAzureDataLakeStorage](#fetchazuredatalakestorage)
- [FetchGCSObject](#fetchgcsobject)
- [FetchFile](#fetchfile)
- [FetchOPCProcessor](#fetchopcprocessor)
- [FetchS3Object](#fetchs3object)
- [FetchSFTP](#fetchsftp)
- [FocusArchiveEntry](#focusarchiveentry)
- [GenerateFlowFile](#generateflowfile)
- [GetFile](#getfile)
- [GetGPS](#getgps)
- [GetTCP](#gettcp)
- [GetUSBCamera](#getusbcamera)
- [HashContent](#hashcontent)
- [InvokeHTTP](#invokehttp)
- [ListAzureBlobStorage](#listazureblobstorage)
- [ListAzureDataLakeStorage](#listazuredatalakestorage)
- [ListGCSBucket](#listgcsbucket)
- [ListenHTTP](#listenhttp)
- [ListenSyslog](#listensyslog)
- [ListenTCP](#listentcp)
- [ListenUDP](#listenudp)
- [ListFile](#listfile)
- [ListS3](#lists3)
- [ListSFTP](#listsftp)
- [LogAttribute](#logattribute)
- [ManipulateArchive](#manipulatearchive)
- [MergeContent](#mergecontent)
- [MotionDetector](#motiondetector)
- [PerformanceDataMonitor](#performancedatamonitor)
- [PostElasticsearch](#postelasticsearch)
- [ProcFsMonitor](#procfsmonitor)
- [PublishKafka](#publishkafka)
- [PublishMQTT](#publishmqtt)
- [PutAzureBlobStorage](#putazureblobstorage)
- [PutAzureDataLakeStorage](#putazuredatalakestorage)
- [PutGCSObject](#putgcsobject)
- [PutFile](#putfile)
- [PutOPCProcessor](#putopcprocessor)
- [PutS3Object](#puts3object)
- [PutSFTP](#putsftp)
- [PutSplunkHTTP](#putsplunkhttp)
- [PutSQL](#putsql)
- [PutTCP](#puttcp)
- [PutUDP](#putudp)
- [QueryDatabaseTable](#querydatabasetable)
- [QuerySplunkIndexingStatus](#querysplunkindexingstatus)
- [ReplaceText](#replacetext)
- [RetryFlowFile](#retryflowfile)
- [RouteOnAttribute](#routeonattribute)
- [RouteText](#routetext)
- [TailEventLog](#taileventlog)
- [TailFile](#tailfile)
- [UnfocusArchiveEntry](#unfocusarchiveentry)
- [UpdateAttribute](#updateattribute)
## AppendHostInfo

### Description

Appends host information such as IP address and hostname as an attribute to incoming flowfiles.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                     | Default Value   | Allowable Values                | Description                                                                            |
|--------------------------|-----------------|---------------------------------|----------------------------------------------------------------------------------------|
| Hostname Attribute       | source.hostname |                                 | Flowfile attribute used to record the agent's hostname                                 |
| IP Attribute             | source.ipv4     |                                 | Flowfile attribute used to record the agent's IP address                               |
| Network Interface Filter |                 |                                 | A regular expression to filter ip addresses based on the name of the network interface |
| Refresh Policy           | On schedule     | On schedule<br>On every trigger | When to recalculate the host info                                                      |
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

## AttributesToJSON

### Description

Generates a JSON representation of the input FlowFile Attributes. The resulting JSON can be written to either a new Attribute 'JSONAttributes' or written to the FlowFile as content.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                          | Default Value      | Allowable Values                           | Description                                                                                                                                                                                                                                                                                                                       |
|-------------------------------|--------------------|--------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Attributes List               |                    |                                            | Comma separated list of attributes to be included in the resulting JSON. If this value is left empty then all existing Attributes will be included. This list of attributes is case sensitive. If an attribute specified in the list is not found it will be be emitted to the resulting JSON with an empty string or NULL value. |
| Attributes Regular Expression |                    |                                            | Regular expression that will be evaluated against the flow file attributes to select the matching attributes. Both the matching attributes and the selected attributes from the Attributes List property will be written in the resulting JSON.                                                                                   |
| **Destination**               | flowfile-attribute | flowfile-attribute<br>flowfile-content<br> | Control if JSON value is written as a new flowfile attribute 'JSONAttributes' or written in the flowfile content. Writing to flowfile content will overwrite any existing flowfile content.                                                                                                                                       |
| **Include Core Attributes**   | true               |                                            | Determines if the FlowFile core attributes which are contained in every FlowFile should be included in the final JSON value generated.                                                                                                                                                                                            |
| **Null Value**                | false              |                                            | If true a non existing selected attribute will be NULL in the resulting JSON. If false an empty string will be placed in the JSON.                                                                                                                                                                                                |
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
| Max Bin Age               |               |                  | The maximum age of a Bin that will trigger a Bin to be complete. Expected format is <duration> <time unit> |
| Maximum Group Size        |               |                  | The maximum size for the bundle. If not specified, there is no maximum.                                    |
| Maximum Number of Entries |               |                  | The maximum number of files to include in a bundle. If not specified, there is no maximum.                 |
| Maximum number of Bins    | 100           |                  | Specifies the maximum number of bins that can be held in memory at any one time                            |
| Minimum Group Size        | 0             |                  | The minimum size of for the bundle                                                                         |
| Minimum Number of Entries | 1             |                  | The minimum number of files to include in a bundle                                                         |
### Relationships

| Name     | Description                                                                                                                  |
|----------|------------------------------------------------------------------------------------------------------------------------------|
| failure  | If the bundle cannot be created, all FlowFiles that would have been used to create the bundle will be transferred to failure |
| original | The FlowFiles that were used to create the bundle                                                                            |


## CapturePacket

### Description

CapturePacket captures and writes one or more packets into a PCAP file that will be used as the content of a flow file. Configuration options exist to adjust the batching of PCAP files. PCAP batching will place a single PCAP into a flow file. A regular expression selects network interfaces. Bluetooth network interfaces can be selected through a separate option.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                | Default Value | Allowable Values | Description                                                             |
|---------------------|---------------|------------------|-------------------------------------------------------------------------|
| Base Directory      | /tmp/         |                  | Scratch directory for PCAP files                                        |
| Batch Size          | 50            |                  | The number of packets to combine within a given PCAP                    |
| Capture Bluetooth   | false         |                  | True indicates that we support bluetooth interfaces                     |
| Network Controllers | .*            |                  | Regular expression of the network controller(s) to which we will attach |
### Relationships

| Name    | Description                     |
|---------|---------------------------------|
| success | All files are routed to success |


## CaptureRTSPFrame

### Description

Captures a frame from the RTSP stream at specified intervals.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name           | Default Value | Allowable Values | Description                                                                            |
|----------------|---------------|------------------|----------------------------------------------------------------------------------------|
| Image Encoding | .jpg          |                  | The encoding that should be applied the the frame images captured from the RTSP stream |
| RTSP Hostname  |               |                  | Hostname of the RTSP stream we are trying to connect to                                |
| RTSP Password  |               |                  | Password used to connect to the RTSP stream                                            |
| RTSP Port      |               |                  | Port that should be connected to to receive RTSP Frames                                |
| RTSP URI       |               |                  | URI that should be appended to the RTSP stream hostname                                |
| RTSP Username  |               |                  | The username for connecting to the RTSP stream                                         |
### Relationships

| Name    | Description                      |
|---------|----------------------------------|
| failure | Failures to capture RTSP frame   |
| success | Successful capture of RTSP frame |

## CollectorInitiatedSubscription

### Description

Windows Event Log Subscribe Callback to receive FlowFiles from Events on Windows.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                               | Default Value   | Allowable Values | Description                                                                                                                                                                                                                                                                                                                                                          |
|------------------------------------|-----------------|------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Subscription Name**              |                 |                  | The name of the subscription. The value provided for this parameter should be unique within the computer's scope.<br/>**Supports Expression Language: true**                                                                                                                                                                                                         |
| **Subscription Description**       |                 |                  | A description of the subscription.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                        |
| **Source Address**                 |                 |                  | The IP address or fully qualified domain name (FQDN) of the local or remote computer (event source) from which the events are collected.<br/>**Supports Expression Language: true**                                                                                                                                                                                  |
| **Source User Name**               |                 |                  | The user name, which is used by the remote computer (event source) to authenticate the user.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                              |
| **Source Password**                |                 |                  | The password, which is used by the remote computer (event source) to authenticate the user.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                               |
| **Source Channels**                |                 |                  | The Windows Event Log Channels (on domain computer(s)) from which events are transferred.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                 |
| **Max Delivery Items**             | 1000            |                  | Determines the maximum number of items that will forwarded from an event source for each request.                                                                                                                                                                                                                                                                    |
| **Delivery MaxLatency Time**       | 10 min          |                  | How long, in milliseconds, the event source should wait before sending events.                                                                                                                                                                                                                                                                                       |
| **Heartbeat Interval**             | 10 min          |                  | Time interval, in milliseconds, which is observed between the sent heartbeat messages.  The event collector uses this property to determine the interval between queries to the event source.                                                                                                                                                                        |
| **Channel**                        | ForwardedEvents |                  | The Windows Event Log Channel (on local machine) to which events are transferred.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                         |
| **Query**                          | *               |                  | XPath Query to filter events. (See https://msdn.microsoft.com/en-us/library/windows/desktop/dd996910(v=vs.85).aspx for examples.)<br/>**Supports Expression Language: true**                                                                                                                                                                                         |
| **Max Buffer Size**                | 1 MB            |                  | The individual Event Log XMLs are rendered to a buffer. This specifies the maximum size in bytes that the buffer will be allowed to grow to. (Limiting the maximum size of an individual Event XML.)                                                                                                                                                                 |
| **Inactive Duration To Reconnect** | 10 min          |                  | If no new event logs are processed for the specified time period, this processor will try reconnecting to recover from a state where any further messages cannot be consumed. Such situation can happen if Windows Event Log service is restarted, or ERROR_EVT_QUERY_RESULT_STALE (15011) is returned. Setting no duration, e.g. '0 ms' disables auto-reconnection. |
### Relationships

| Name    | Description                                    |
|---------|------------------------------------------------|
| success | Relationship for successfully consumed events. |


## CompressContent

### Description

Compresses or decompresses the contents of FlowFiles using a user-specified compression algorithm and updates the mime.type attribute as appropriate
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name               | Default Value           | Allowable Values | Description                                                                    |
|--------------------|-------------------------|------------------|--------------------------------------------------------------------------------|
| Compression Format | use mime.type attribute |                  | The compression format to use.                                                 |
| Compression Level  | 1                       |                  | The compression level to use; this is valid only when using GZIP compression.  |
| Mode               | compress                |                  | Indicates whether the processor should compress content or decompress content. |
| Update Filename    | false                   |                  | Determines if filename extension need to be updated                            |
### Relationships

| Name    | Description                                                                                                   |
|---------|---------------------------------------------------------------------------------------------------------------|
| failure | FlowFiles will be transferred to the failure relationship if they fail to compress/decompress                 |
| success | FlowFiles will be transferred to the success relationship after successfully being compressed or decompressed |


## ConsumeJournald
### Description
Consume systemd-journald journal messages. Available on Linux only.

### Properties
All properties are required with a default value, making them effectively optional. None of the properties support the NiFi Expression Language.

| Name                 | Default Value | Allowable Values                                                                   | Description                                                                                                                                             |
|----------------------|---------------|------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------|
| Batch Size           | 1000          | Positive numbers                                                                   | The maximum number of entries processed in a single execution.                                                                                          |
| Payload Format       | Syslog        | Raw<br>Syslog                                                                      | Configures flow file content formatting.<br>Raw: only the message.<br>Syslog: similar to syslog or journalctl output.                                   |
| Include Timestamp    | true          | true<br>false                                                                      | Include message timestamp in the 'timestamp' attribute.                                                                                                 |
| Journal Type         | System        | User<br>System<br>Both                                                             | Type of journal to consume.                                                                                                                             |
| Process Old Messages | false         | true<br>false                                                                      | Process events created before the first usage (schedule) of the processor instance.                                                                     |
| Timestamp Format     | %x %X %Z      | [date format](https://howardhinnant.github.io/date/date.html#to_stream_formatting) | Format string to use when creating the timestamp attribute or writing messages in the syslog format. ISO/ISO 8601/ISO8601 are equivalent to "%FT%T%Ez". |

### Relationships

| Name    | Description                    |
|---------|--------------------------------|
| success | Journal messages as flow files |


## ConsumeKafka

### Description

Consumes messages from Apache Kafka and transform them into MiNiFi FlowFiles. The application should make sure that the processor is triggered at regular intervals, even if no messages are expected, to serve any queued callbacks waiting to be called. Rebalancing can also only happen on trigger.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                         | Default Value  | Allowable Values                                       | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
|------------------------------|----------------|--------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Duplicate Header Handling    | Keep Latest    | Comma-separated Merge<br>Keep First<br>Keep Latest<br> | For headers to be added as attributes, this option specifies how to handle cases where multiple headers are present with the same key. For example in case of receiving these two headers: "Accept: text/html" and "Accept: application/xml" and we want to attach the value of "Accept" as a FlowFile attribute:<br/> - "Keep First" attaches: "Accept -> text/html"<br/> - "Keep Latest" attaches: "Accept -> application/xml"<br/> - "Comma-separated Merge" attaches: "Accept -> text/html, application/xml"                                                                                                        |
| **Group ID**                 |                |                                                        | A Group ID is used to identify consumers that are within the same consumer group. Corresponds to Kafka's 'group.id' property.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| Headers To Add As Attributes |                |                                                        | A comma separated list to match against all message headers. Any message header whose name matches an item from the list will be added to the FlowFile as an Attribute. If not specified, no Header values will be added as FlowFile attributes. The behaviour on when multiple headers of the same name are present is set using the DuplicateHeaderHandling attribute.                                                                                                                                                                                                                                                |
| **Honor Transactions**       | true           |                                                        | Specifies whether or not MiNiFi should honor transactional guarantees when communicating with Kafka. If false, the Processor will use an "isolation level" of read_uncomitted. This means that messages will be received as soon as they are written to Kafka but will be pulled, even if the producer cancels the transactions. If this value is true, MiNiFi will not receive any messages for which the producer's transaction was canceled, but this can result in some latency since the consumer must wait for the producer to finish its entire transaction instead of pulling as the messages become available. |
| **Kafka Brokers**            | localhost:9092 |                                                        | A comma-separated list of known Kafka Brokers in the format <host>:<port>.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| Kerberos Keytab Path         |                |                                                        | The path to the location on the local filesystem where the kerberos keytab is located. Read permission on the file is required.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| Kerberos Principal           |                |                                                        | Keberos Principal                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| Kerberos Service Name        |                |                                                        | Kerberos Service Name                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| **Key Attribute Encoding**   | UTF-8          | Hex<br>UTF-8<br>                                       | FlowFiles that are emitted have an attribute named 'kafka.key'. This property dictates how the value of the attribute should be encoded.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| Max Poll Records             | 10000          |                                                        | Specifies the maximum number of records Kafka should return when polling each time the processor is triggered.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| **Max Poll Time**            | 4 seconds      |                                                        | Specifies the maximum amount of time the consumer can use for polling data from the brokers. Polling is a blocking operation, so the upper limit of this value is specified in 4 seconds.                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Message Demarcator           |                |                                                        | Since KafkaConsumer receives messages in batches, you have an option to output FlowFiles which contains all Kafka messages in a single batch for a given topic and partition and this property allows you to provide a string (interpreted as UTF-8) to use for demarcating apart multiple Kafka messages. This is an optional property and if not provided each Kafka message received will result in a single FlowFile which time it is triggered. <br/>**Supports Expression Language: true**                                                                                                                        |
| Message Header Encoding      | UTF-8          | Hex<br>UTF-8<br>                                       | Any message header that is found on a Kafka message will be added to the outbound FlowFile as an attribute. This property indicates the Character Encoding to use for deserializing the headers.                                                                                                                                                                                                                                                                                                                                                                                                                        |
| **Offset Reset**             | latest         | earliest<br>latest<br>none<br>                         | Allows you to manage the condition when there is no initial offset in Kafka or if the current offset does not exist any more on the server (e.g. because that data has been deleted). Corresponds to Kafka's 'auto.offset.reset' property.                                                                                                                                                                                                                                                                                                                                                                              |
| Password                     |                |                                                        | The password for the given username when the SASL Mechanism is sasl_plaintext                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| SASL Mechanism               | GSSAPI         | GSSAPI<br/>PLAIN                                       | The SASL mechanism to use for authentication. Corresponds to Kafka's 'sasl.mechanism' property.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| **Security Protocol**        | plaintext      | plaintext<br/>ssl<br/>sasl_plaintext<br/>sasl_ssl      | Protocol used to communicate with brokers. Corresponds to Kafka's 'security.protocol' property.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| Session Timeout              | 60 seconds     |                                                        | Client group session and failure detection timeout. The consumer sends periodic heartbeats to indicate its liveness to the broker. If no hearts are received by the broker for a group member within the session timeout, the broker will remove the consumer from the group and trigger a rebalance. The allowed range is configured with the broker configuration properties group.min.session.timeout.ms and group.max.session.timeout.ms.                                                                                                                                                                           |
| SSL Context Service          |                |                                                        | SSL Context Service Name                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| **Topic Name Format**        | Names          | Names<br>Patterns<br>                                  | Specifies whether the Topic(s) provided are a comma separated list of names or a single regular expression. Using regular expressions does not automatically discover Kafka topics created after the processor started.                                                                                                                                                                                                                                                                                                                                                                                                 |
| **Topic Names**              |                |                                                        | The name of the Kafka Topic(s) to pull from. Multiple topic names are supported as a comma separated list.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| Username                     |                |                                                        | The username when the SASL Mechanism is sasl_plaintext                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
### Properties

| Name    | Description                                                                                                                     |
|---------|---------------------------------------------------------------------------------------------------------------------------------|
| success | Incoming Kafka messages as flowfiles. Depending on the demarcation strategy, this can be one or multiple flowfiles per message. |

## ConsumeMQTT

### Description

This Processor gets the contents of a FlowFile from a MQTT broker for a specified topic. The the payload of the MQTT message becomes content of a FlowFile
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                  | Default Value | Allowable Values | Description                                                                                                                 |
|-----------------------|---------------|------------------|-----------------------------------------------------------------------------------------------------------------------------|
| **Broker URI**        |               |                  | The URI to use to connect to the MQTT broker                                                                                |
| **Topic**             |               |                  | The topic to subscribe to                                                                                                   |
| Client ID             |               |                  | MQTT client ID to use                                                                                                       |
| Quality of Service    | 0             |                  | The Quality of Service (QoS) to receive the message with. Accepts three values '0', '1' and '2'                             |
| Connection Timeout    | 30 sec        |                  | Maximum time interval the client will wait for the network connection to the MQTT broker                                    |
| Keep Alive Interval   | 60 sec        |                  | Defines the maximum time interval between messages being sent to the broker                                                 |
| Max Flow Segment Size |               |                  | Maximum flow content payload segment size for the MQTT record                                                               |
| Last Will Topic       |               |                  | The topic to send the client's Last Will to. If the Last Will topic is not set then a Last Will will not be sent            |
| Last Will Message     |               |                  | The message to send as the client's Last Will. If the Last Will Message is empty, Last Will will be deleted from the broker |
| Last Will QoS         | 0             |                  | The Quality of Service (QoS) to send the last will with. Accepts three values '0', '1' and '2'                              |
| Last Will Retain      | false         |                  | Whether to retain the client's Last Will                                                                                    |
| Security Protocol     |               |                  | Protocol used to communicate with brokers                                                                                   |
| Security CA           |               |                  | File or directory path to CA certificate(s) for verifying the broker's key                                                  |
| Security Cert         |               |                  | Path to client's public key (PEM) used for authentication                                                                   |
| Security Private Key  |               |                  | Path to client's private key (PEM) used for authentication                                                                  |
| Security Pass Phrase  |               |                  | Private key passphrase                                                                                                      |
| Username              |               |                  | Username to use when connecting to the broker                                                                               |
| Password              |               |                  | Password to use when connecting to the broker                                                                               |
| Clean Session         | true          |                  | Whether to start afresh rather than remembering previous subscriptions                                                      |
| Queue Max Message     | 1000          |                  | Maximum number of messages allowed on the received MQTT queue                                                               |


### Relationships

| Name    | Description                                                                                  |
|---------|----------------------------------------------------------------------------------------------|
| success | FlowFiles that are sent successfully to the destination are transferred to this relationship |


## ConsumeWindowsEventLog

### Description

Windows Event Log Subscribe Callback to receive FlowFiles from Events on Windows.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                               | Default Value                                                                                                                                                                                                               | Allowable Values                 | Description                                                                                                                                                                                                                                                                                                                                                          |
|------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Channel**                        | System                                                                                                                                                                                                                      |                                  | The Windows Event Log Channel to listen to.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                               |
| **Query**                          | *                                                                                                                                                                                                                           |                                  | XPath Query to filter events. (See https://msdn.microsoft.com/en-us/library/windows/desktop/dd996910(v=vs.85).aspx for examples.)<br/>**Supports Expression Language: true**                                                                                                                                                                                         |
| **Max Buffer Size**                | 1 MB                                                                                                                                                                                                                        |                                  | The individual Event Log XMLs are rendered to a buffer. This specifies the maximum size in bytes that the buffer will be allowed to grow to. (Limiting the maximum size of an individual Event XML.)                                                                                                                                                                 |
| **Inactive Duration To Reconnect** | 10 min                                                                                                                                                                                                                      |                                  | If no new event logs are processed for the specified time period, this processor will try reconnecting to recover from a state where any further messages cannot be consumed. Such situation can happen if Windows Event Log service is restarted, or ERROR_EVT_QUERY_RESULT_STALE (15011) is returned. Setting no duration, e.g. '0 ms' disables auto-reconnection. |
| Identifier Match Regex             | .*Sid                                                                                                                                                                                                                       |                                  | Regular Expression to match Subject Identifier Fields. These will be placed into the attributes of the FlowFile                                                                                                                                                                                                                                                      |
| Apply Identifier Function          | true                                                                                                                                                                                                                        | true<br>false                    | If true it will resolve SIDs matched in the 'Identifier Match Regex' to the DOMAIN\USERNAME associated with that ID                                                                                                                                                                                                                                                  |
| Resolve Metadata in Attributes     | true                                                                                                                                                                                                                        | true<br>false                    | If true, any metadata that is resolved ( such as IDs or keyword metadata ) will be placed into attributes, otherwise it will be replaced in the XML or text output                                                                                                                                                                                                   |
| Event Header Delimiter             |                                                                                                                                                                                                                             |                                  | If set, the chosen delimiter will be used in the Event output header. Otherwise, a colon followed by spaces will be used.                                                                                                                                                                                                                                            |
| Event Header                       | LOG_NAME=Log Name, SOURCE = Source, TIME_CREATED = Date,EVENT_RECORDID=Record ID,EVENTID = Event ID,TASK_CATEGORY = Task Category,LEVEL = Level,KEYWORDS = Keywords,USER = User,COMPUTER = Computer, EVENT_TYPE = EventType |                                  | Comma seperated list of key/value pairs with the following keys LOG_NAME, SOURCE, TIME_CREATED,EVENT_RECORDID,EVENTID,TASK_CATEGORY,LEVEL,KEYWORDS,USER,COMPUTER, and EVENT_TYPE. Eliminating fields will remove them from the header.                                                                                                                               |
| **Output Format**                  | Both                                                                                                                                                                                                                        | XML<br>Plaintext<br>JSON<br>Both | Set the output format type. In case 'Both' is selected the processor generates two flow files for every event captured in format XML and Plaintext                                                                                                                                                                                                                   |
| **JSON Format**                    | Simple                                                                                                                                                                                                                      | Simple<br>Raw<br>Flattened       | Set the json format type. Only applicable if Output Format is set to 'JSON'                                                                                                                                                                                                                                                                                          |
| Batch Commit Size                  | 1000                                                                                                                                                                                                                        |                                  | Maximum number of Events to consume and create to Flow Files from before committing.                                                                                                                                                                                                                                                                                 |
| **Process Old Events**             | false                                                                                                                                                                                                                       | true<br>false                    | This property defines if old events (which are created before first time server is started) should be processed.                                                                                                                                                                                                                                                     |
| State Directory                    | CWELState                                                                                                                                                                                                                   |                                  | DEPRECATED. Only use it for state migration from the state file, supplying the legacy state directory.                                                                                                                                                                                                                                                               |

### Relationships

| Name    | Description                                    |
|---------|------------------------------------------------|
| success | Relationship for successfully consumed events. |


## DefragmentText

### Description

DefragmentText splits and merges incoming flowfiles so cohesive messages are not split between them. It can handle multiple inputs differentiated by the <em>absolute.path</em> flow file attribute.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name             | Default Value    | Allowable Values                   | Description                                                                                                                                                              |
|------------------|------------------|------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Pattern**      |                  |                                    | A regular expression to match at the start or end of messages.                                                                                                           |
| Pattern Location | Start of Message | Start of Message<br>End of Message | Whether the pattern is located at the start or at the end of the messages.                                                                                               |
| Max Buffer Age   | 10 min           | &lt;duration&gt; &lt;time unit&gt; | The maximum age of the buffer after which it will be transferred to success when matching Start of Message patterns or to failure when matching End of Message patterns. |
| Max Buffer Size  |                  | &lt;size&gt; &lt;size unit&gt;     | The maximum buffer size, if the buffer exceed this, it will be transferred to failure.                                                                                   |

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

| Name                                   | Default Value | Allowable Values                                     | Description                                                                                                                                                                                                                                                            |
|----------------------------------------|---------------|------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Azure Storage Credentials Service      |               |                                                      | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                                                             |
| **Blob**                               |               |                                                      | The filename of the blob. If left empty the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                                                     |
| Common Storage Account Endpoint Suffix |               |                                                      | Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a different suffix in certain circumstances (like Azure Stack or non-public Azure regions).<br/>**Supports Expression Language: true**                            |
| Connection String                      |               |                                                      | Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Managed Identity is not used.<br/>**Supports Expression Language: true**                                                                             |
| **Container Name**                     |               |                                                      | Name of the Azure storage container. In case of PutAzureBlobStorage processor, container can be created if it does not exist.<br/>**Supports Expression Language: true**                                                                                               |
| **Delete Snapshots Option**            | None          | None<br/>Include Snapshots<br/>Delete Snapshots Only | Specifies the snapshot deletion options to be used when deleting a blob.<br/>None: Deletes the blob only.<br/>Include Snapshots: Delete the blob and its snapshots.<br/>Delete Snapshots Only: Delete only the blob's snapshots.                                       |
| SAS Token                              |               |                                                      | Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Managed Identity is not used.<br/>**Supports Expression Language: true**                                                            |
| Storage Account Key                    |               |                                                      | The storage account key. This is an admin-like password providing access to every container in this account. It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies.<br/>**Supports Expression Language: true** |
| Storage Account Name                   |               |                                                      | The storage account name.<br/>**Supports Expression Language: true**                                                                                                                                                                                                   |
| **Use Managed Identity Credentials**   | false         |                                                      | If true Managed Identity credentials will be used together with the Storage Account Name for authentication.                                                                                                                                                           |

### Relationships

| Name    | Description                                                             |
|---------|-------------------------------------------------------------------------|
| failure | Unsuccessful operations will be transferred to the failure relationship |
| success | All successfully processed FlowFiles are routed to this relationship    |


## DeleteAzureDataLakeStorage

### Description

Deletes the provided file from Azure Data Lake Storage Gen 2
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                  | Default Value | Allowable Values | Description                                                                                                                                                                                                                             |
|---------------------------------------|---------------|------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Azure Storage Credentials Service** |               |                  | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                              |
| File Name                             |               |                  | The filename in Azure Storage. If left empty the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                 |
| **Filesystem Name**                   |               |                  | Name of the Azure Storage File System. It is assumed to be already existing.<br/>**Supports Expression Language: true**                                                                                                                 |
| Directory Name                        |               |                  | Name of the Azure Storage Directory. The Directory Name cannot contain a leading '/'. If left empty it designates the root directory. The directory will be created if not already existing.<br/>**Supports Expression Language: true** |

### Relationships

| Name    | Description                                                                                             |
|---------|---------------------------------------------------------------------------------------------------------|
| failure | If file deletion from Azure Data Lake storage fails the flowfile is transferred to this relationship    |
| success | If file deletion from Azure Data Lake storage succeeds the flowfile is transferred to this relationship |

## DeleteGCSObject

### Description

Deletes an object from a Google Cloud Bucket.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                 | Default Value | Allowable Values                                                                  | Description                                                                                                                                     |
|--------------------------------------|---------------|-----------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------|
| **Bucket**                           | ${gcs.bucket} |                                                                                   | Bucket of the object.<br>**Supports Expression Language: true**                                                                                 |
| **Key**                              | ${filename}   |                                                                                   | Name of the object.<br>**Supports Expression Language: true**                                                                                   |
| **Number of retries**                | 6             | integers                                                                          | How many retry attempts should be made before routing to the failure relationship.                                                              |
| **GCP Credentials Provider Service** |               | [GCPCredentialsControllerService](CONTROLLERS.md#GCPCredentialsControllerService) | The Controller Service used to obtain Google Cloud Platform credentials.                                                                        |
| Server Side Encryption Key           |               |                                                                                   | An AES256 Encryption Key (encoded in base64) for server-side encryption of the object.<br>**Supports Expression Language: true**                |
| Object Generation                    |               |                                                                                   | The generation of the Object to download. If left empty, then it will download the latest generation.<br>**Supports Expression Language: true** |
| Endpoint Override URL                |               |                                                                                   | Overrides the default Google Cloud Storage endpoints                                                                                            |


### Relationships

| Name    | Description                                                                                  |
|---------|----------------------------------------------------------------------------------------------|
| success | FlowFiles are routed to this relationship after a successful Google Cloud Storage operation. |
| failure | FlowFiles are routed to this relationship if the Google Cloud Storage operation fails.       |


### Output Attributes

| Attribute            | Relationship | Description                                             |
|----------------------|--------------|---------------------------------------------------------|
| _gcs.status.message_ | failure      | The status message received from google cloud.          |
| _gcs.error.reason_   | failure      | The description of the error occurred during operation. |
| _gcs.error.domain_   | failure      | The domain of the error occurred during operation.      |

## DeleteS3Object

### Description

Deletes FlowFiles on an Amazon S3 Bucket. If attempting to delete a file that does not exist, FlowFile is routed to success.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                             | <div style="width:7em">Default Value</div> | <div style="width:8em">Allowable Values</div>                                                                                                                                                                                                                                                                                                                                                               | Description                                                                                                                                                                                                                                                                                                 |
|----------------------------------|--------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Object Key                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The key of the S3 object. If none is given the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                                                                                       |
| **Bucket**                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The S3 bucket<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                    |
| Access Key                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | AWS account access key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                           |
| Secret Key                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | AWS account secret key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                           |
| Credentials File                 |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey                                                                                                                                                                                 |
| AWS Credentials Provider service |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.                                                                                                                                                                                                         |
| **Region**                       | us-west-2                                  | af-south-1<br/>ap-east-1<br/>ap-northeast-1<br/>ap-northeast-2<br/>ap-northeast-3<br/>ap-south-1<br/>ap-southeast-1<br/>ap-southeast-2<br/>ca-central-1<br/>cn-north-1<br/>cn-northwest-1<br/>eu-central-1<br/>eu-north-1<br/>eu-south-1<br/>eu-west-1<br/>eu-west-2<br/>eu-west-3<br/>me-south-1<br/>sa-east-1<br/>us-east-1<br/>us-east-2<br/>us-gov-east-1<br/>us-gov-west-1<br/>us-west-1<br/>us-west-2 | AWS Region                                                                                                                                                                                                                                                                                                  |
| **Communications Timeout**       | 30 sec                                     |                                                                                                                                                                                                                                                                                                                                                                                                             | Sets the timeout of the communication between the AWS server and the client                                                                                                                                                                                                                                 |
| Endpoint Override URL            |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Endpoint URL to use instead of the AWS default including scheme, host, port, and path. The AWS libraries select an endpoint URL based on the AWS region, but this property overrides the selected endpoint URL, allowing use with other S3-compatible endpoints.<br/>**Supports Expression Language: true** |
| Proxy Host                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Proxy host name or IP<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                            |
| Proxy Port                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The port number of the proxy host<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                |
| Proxy Username                   |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Username to set when authenticating against proxy<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
| Proxy Password                   |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Password to set when authenticating against proxy<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
| Version                          |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The Version of the Object to delete<br/>**Supports Expression Language: true**                                                                                                                                                                                                                              |
### Relationships

| Name    | Description                                  |
|---------|----------------------------------------------|
| failure | FlowFiles are routed to failure relationship |
| success | FlowFiles are routed to success relationship |


## ExecuteProcess

### Description

Runs an operating system command specified by the user and writes the output of that command to a FlowFile. If the command is expected to be long-running,the Processor can output the partial data on a specified interval. When this option is used, the output is expected to be in textual format,as it typically does not make sense to split binary data on arbitrary time-based intervals. This processor is not available on Windows systems.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                  | Default Value | Allowable Values | Description                                                                                                                      |
|-----------------------|---------------|------------------|----------------------------------------------------------------------------------------------------------------------------------|
| Batch Duration        | 0 sec         |                  | If the process is expected to be long-running and produce textual output, a batch duration can be specified.                     |
| Command               |               |                  | Specifies the command to be executed; if just the name of an executable is provided, it must be in the user's environment PATH.  |
| Command Arguments     |               |                  | The arguments to supply to the executable delimited by white space. White space can be escaped by enclosing it in double-quotes. |
| Redirect Error Stream | false         |                  | If true will redirect any error stream output of the process to the output stream.                                               |
| Working Directory     |               |                  | The directory to use as the current working directory when executing the command                                                 |
### Relationships

| Name    | Description                                            |
|---------|--------------------------------------------------------|
| success | All created FlowFiles are routed to this relationship. |


## ExecutePythonProcessor

### Description

Executes a script given the flow file and a process session. The script is responsible for handling the incoming flow file (transfer to SUCCESS or remove, e.g.) as well as any flow files created by the script. If the handling is incomplete or incorrect, the session will be rolled back. Scripts must define an onTrigger function which accepts NiFi Context and Property objects. For efficiency, scripts are executed once when the processor is run, then the onTrigger method is called for each incoming flowfile. This enables scripts to keep state if they wish, although there will be a script context per concurrent task of the processor. In order to, e.g., compute an arithmetic sum based on incoming flow file information, set the concurrent tasks to 1. The python script files are expected to contain `describe(procesor)` and `onTrigger(context, session)`.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                        | Default Value | Allowable Values | Description                                                                                                                                                  |
|-----------------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Module Directory            |               |                  | Comma-separated list of paths to files and/or directories which contain modules required by the script                                                       |
| **Reload on Script Change** | true          |                  | If true and Script File property is used, then script file will be reloaded if it has changed, otherwise the first loaded version will be used at all times. |
| Script Body                 |               |                  | Script to execute                                                                                                                                            |
| Script File                 |               |                  | Path to script file to execute. Only one of Script File or Script Body may be used                                                                           |
### Relationships

| Name    | Description      |
|---------|------------------|
| failure | Script failures  |
| success | Script successes |


## ExecuteSQL

### Description

Execute provided SQL query. Query result rows will be outputted as new flow files with attribute keys equal to result column names and values equal to result values. There will be one output FlowFile per result row. This processor can be scheduled to run using the standard timer-based scheduling methods, or it can be triggered by an incoming FlowFile. If it is triggered by an incoming FlowFile, then attributes of that FlowFile will be available when evaluating the query.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value | Allowable Values     | Description                                                                                                                                                                                                                                                                                                                                                                                     |
|----------------------------|---------------|----------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **DB Controller Service**  |               |                      | Database Controller Service.                                                                                                                                                                                                                                                                                                                                                                    |
| **Max Rows Per Flow File** | 0             |                      | The maximum number of result rows that will be included in a single FlowFile. This will allow you to break up very large result sets into multiple FlowFiles. If the value specified is zero, then all rows are returned in a single FlowFile.                                                                                                                                                  |
| **Output Format**          | JSON-Pretty   | JSON<br/>JSON-Pretty | Set the output format type.                                                                                                                                                                                                                                                                                                                                                                     |
| SQL select query           |               |                      | The SQL select query to execute. The query can be empty, a constant value, or built from attributes using Expression Language. If this property is specified, it will be used regardless of the content of incoming flowfiles. If this property is empty, the content of the incoming flow file is expected to contain a valid SQL select query, to be issued by the processor to the database. |
### Relationships

| Name    | Description                                              |
|---------|----------------------------------------------------------|
| success | Successfully created FlowFile from SQL query result set. |


## ExecuteScript

### Description

Executes a script given the flow file and a process session. The script is responsible for handling the incoming flow file (transfer to SUCCESS or remove, e.g.) as well as any flow files created by the script. If the handling is incomplete or incorrect, the session will be rolled back. Scripts must define an onTrigger function which accepts NiFi Context and Property objects. For efficiency, scripts are executed once when the processor is run, then the onTrigger method is called for each incoming flowfile. This enables scripts to keep state if they wish, although there will be a script context per concurrent task of the processor. In order to, e.g., compute an arithmetic sum based on incoming flow file information, set the concurrent tasks to 1.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                 | Default Value | Allowable Values | Description                                                                                            |
|----------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------|
| Module Directory     |               |                  | Comma-separated list of paths to files and/or directories which contain modules required by the script |
| Script Body          |               |                  | Body of script to execute. Only one of Script File or Script Body may be used                          |
| **Script Engine**    | python        | python<br>lua    | The engine to execute scripts (python, lua)                                                            |
| Script File          |               |                  | Path to script file to execute. Only one of Script File or Script Body may be used                     |

### Relationships

| Name    | Description      |
|---------|------------------|
| failure | Script failures  |
| success | Script successes |


## ExtractText

### Description

Extracts the content of a FlowFile and places it into an attribute.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                             | Default Value | Allowable Values | Description                                                                                                                                                                                            |
|----------------------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Attribute                        |               |                  | Attribute to set from content                                                                                                                                                                          |
| Enable Case-insensitive Matching | false         |                  | Indicates that two characters match even if they are in a different case.                                                                                                                              |
| Enable repeating capture group   | false         |                  | f set to true, every string matching the capture groups will be extracted. Otherwise, if the Regular Expression matches more than once, only the first match will be extracted.                        |
| Include Capture Group 0          | true          |                  | Indicates that Capture Group 0 should be included as an attribute. Capture Group 0 represents the entirety of the regular expression match, is typically not used, and could have considerable length. |
| Maximum Capture Group Length     | 1024          |                  | Specifies the maximum number of characters a given capture group value can have. Any characters beyond the max will be truncated.                                                                      |
| Regex Mode                       | false         |                  | Set this to extract parts of flowfile content using regular experssions in dynamic properties                                                                                                          |
| Size Limit                       | 2097152       |                  | Maximum number of bytes to read into the attribute. 0 for no limit. Default is 2MB.                                                                                                                    |
### Relationships

| Name    | Description                            |
|---------|----------------------------------------|
| success | success operational on the flow record |


## FetchAzureBlobStorage

### Description

Retrieves contents of an Azure Storage Blob, writing the contents to the content of the FlowFile
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                   | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                            |
|----------------------------------------|---------------|------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Azure Storage Credentials Service      |               |                  | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                                                             |
| **Blob**                               |               |                  | The filename of the blob. If left empty the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                                                     |
| Common Storage Account Endpoint Suffix |               |                  | Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a different suffix in certain circumstances (like Azure Stack or non-public Azure regions).<br/>**Supports Expression Language: true**                            |
| Connection String                      |               |                  | Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Managed Identity is not used.<br/>**Supports Expression Language: true**                                                                             |
| **Container Name**                     |               |                  | Name of the Azure storage container. In case of PutAzureBlobStorage processor, container can be created if it does not exist.<br/>**Supports Expression Language: true**                                                                                               |
| Range Length                           |               |                  | The number of bytes to download from the blob, starting from the Range Start. An empty value or a value that extends beyond the end of the blob will read to the end of the blob.<br/>**Supports Expression Language: true**                                           |
| Range Start                            |               |                  | The byte position at which to start reading from the blob. An empty value or a value of zero will start reading at the beginning of the blob.<br/>**Supports Expression Language: true**                                                                               |
| SAS Token                              |               |                  | Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Managed Identity is not used.<br/>**Supports Expression Language: true**                                                            |
| Storage Account Key                    |               |                  | The storage account key. This is an admin-like password providing access to every container in this account. It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies.<br/>**Supports Expression Language: true** |
| Storage Account Name                   |               |                  | The storage account name.<br/>**Supports Expression Language: true**                                                                                                                                                                                                   |
| **Use Managed Identity Credentials**   | false         |                  | If true Managed Identity credentials will be used together with the Storage Account Name for authentication.                                                                                                                                                           |

### Relationships

| Name    | Description                                                             |
|---------|-------------------------------------------------------------------------|
| failure | Unsuccessful operations will be transferred to the failure relationship |
| success | All successfully processed FlowFiles are routed to this relationship    |


## FetchAzureDataLakeStorage

### Description

Fetch the provided file from Azure Data Lake Storage Gen 2
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                  | Default Value | Allowable Values | Description                                                                                                                                                                                                                             |
|---------------------------------------|---------------|------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Azure Storage Credentials Service** |               |                  | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                              |
| File Name                             |               |                  | The filename in Azure Storage. If left empty the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                 |
| **Filesystem Name**                   |               |                  | Name of the Azure Storage File System. It is assumed to be already existing.<br/>**Supports Expression Language: true**                                                                                                                 |
| Directory Name                        |               |                  | Name of the Azure Storage Directory. The Directory Name cannot contain a leading '/'. If left empty it designates the root directory. The directory will be created if not already existing.<br/>**Supports Expression Language: true** |
| Range Start                           |               |                  | The byte position at which to start reading from the object. An empty value or a value of zero will start reading at the beginning of the object.<br/>**Supports Expression Language: true**                                            |
| Range Length                          |               |                  | The number of bytes to download from the object, starting from the Range Start. An empty value or a value that extends beyond the end of the object will read to the end of the object.<br/>**Supports Expression Language: true**      |
| Number of Retries                     | 0             |                  | The number of automatic retries to perform if the download fails.<br/>**Supports Expression Language: true**                                                                                                                            |

### Relationships

| Name    | Description                                                                                       |
|---------|---------------------------------------------------------------------------------------------------|
| failure | In case of fetch failure flowfiles are transferred to this relationship                           |
| success | Files that have been successfully fetched from Azure storage are transferred to this relationship |


## FetchGCSObject

### Description

Fetches a file from a Google Cloud Bucket. Designed to be used in tandem with ListGCSBucket.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                 | Default Value | Allowable Values                                                                  | Description                                                                                                                                     |
|--------------------------------------|---------------|-----------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------|
| **Bucket**                           | ${gcs.bucket} |                                                                                   | Bucket of the object.<br>**Supports Expression Language: true**                                                                                 |
| **Key**                              | ${filename}   |                                                                                   | Name of the object.<br>**Supports Expression Language: true**                                                                                   |
| **Number of retries**                | 6             | integers                                                                          | How many retry attempts should be made before routing to the failure relationship.                                                              |
| **GCP Credentials Provider Service** |               | [GCPCredentialsControllerService](CONTROLLERS.md#GCPCredentialsControllerService) | The Controller Service used to obtain Google Cloud Platform credentials.                                                                        |
| Server Side Encryption Key           |               |                                                                                   | An AES256 Encryption Key (encoded in base64) for server-side encryption of the object.<br>**Supports Expression Language: true**                |
| Object Generation                    |               |                                                                                   | The generation of the Object to download. If left empty, then it will download the latest generation.<br>**Supports Expression Language: true** |
| Endpoint Override URL                |               |                                                                                   | Overrides the default Google Cloud Storage endpoints                                                                                            |


### Relationships

| Name    | Description                                                                                  |
|---------|----------------------------------------------------------------------------------------------|
| success | FlowFiles are routed to this relationship after a successful Google Cloud Storage operation. |
| failure | FlowFiles are routed to this relationship if the Google Cloud Storage operation fails.       |


### Output Attributes

| Attribute            | Relationship | Description                                             |
|----------------------|--------------|---------------------------------------------------------|
| _gcs.status.message_ | failure      | The status message received from google cloud.          |
| _gcs.error.reason_   | failure      | The description of the error occurred during operation. |
| _gcs.error.domain_   | failure      | The domain of the error occurred during operation.      |

## FetchFile

### Description

Reads the contents of a file from disk and streams it into the contents of an incoming FlowFile. Once this is done, the file is optionally moved elsewhere or deleted to help keep the file system organized.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                 | Default Value | Allowable Values                                    | Description                                                                                                                                                                                                                                                              |
|--------------------------------------|---------------|-----------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| File to Fetch                        |               |                                                     | The fully-qualified filename of the file to fetch from the file system. If not defined the default ${absolute.path}/${filename} path is used.<br/>**Supports Expression Language: true**                                                                                 |
| **Completion Strategy**              | None          | None<br/>Move File<br/>Delete File                  | Specifies what to do with the original file on the file system once it has been pulled into MiNiFi                                                                                                                                                                       |
| Move Destination Directory           |               |                                                     | The directory to move the original file to once it has been fetched from the file system. This property is ignored unless the Completion Strategy is set to "Move File". If the directory does not exist, it will be created.<br/>**Supports Expression Language: true** |
| **Move Conflict Strategy**           | Rename        | Rename<br/>Replace File<br/>Keep Existing<br/>Fail  | If Completion Strategy is set to Move File and a file already exists in the destination directory with the same name, this property specifies how that naming conflict should be resolved                                                                                |
| **Log level when file not found**    | ERROR         | TRACE<br/>DEBUG<br/>INFO<br/>WARN<br/>ERROR<br/>OFF | Log level to use in case the file does not exist when the processor is triggered                                                                                                                                                                                         |
| **Log level when permission denied** | ERROR         | TRACE<br/>DEBUG<br/>INFO<br/>WARN<br/>ERROR<br/>OFF | Log level to use in case agent does not have sufficient permissions to read the file                                                                                                                                                                                     |

### Relationships

| Name              | Description                                                                                                                                                                       |
|-------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| success           | Any FlowFile that is successfully fetched from the file system will be transferred to this Relationship.                                                                          |
| not.found         | Any FlowFile that could not be fetched from the file system because the file could not be found will be transferred to this Relationship.                                         |
| permission.denied | Any FlowFile that could not be fetched from the file system due to the user running MiNiFi not having sufficient permissions will be transferred to this Relationship.            |
| failure           | Any FlowFile that could not be fetched from the file system for any reason other than insufficient permissions or the file not existing will be transferred to this Relationship. |


## FetchOPCProcessor

### Description

Fetches OPC-UA node
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                            | Default Value | Allowable Values          | Description                                                                                                                                                                                    |
|---------------------------------|---------------|---------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Application URI                 |               |                           | Application URI of the client in the format 'urn:unconfigured:application'. Mandatory, if using Secure Channel and must match the URI included in the certificate's Subject Alternative Names. |
| Certificate path                |               |                           | Path to the DER-encoded cert file                                                                                                                                                              |
| Key path                        |               |                           | Path to the DER-encoded key file                                                                                                                                                               |
| **Lazy mode**                   | Off           | Off<br>On<br>             | Only creates flowfiles from nodes with new timestamp from the server.                                                                                                                          |
| Max depth                       | 0             |                           | Specifiec the max depth of browsing. 0 means unlimited.                                                                                                                                        |
| Namespace index                 | 0             |                           | The index of the namespace. Used only if node ID type is not path.                                                                                                                             |
| **Node ID**                     |               |                           | Specifies the ID of the root node to traverse                                                                                                                                                  |
| **Node ID type**                |               | Int<br>Path<br>String<br> | Specifies the type of the provided node ID                                                                                                                                                     |
| **OPC server endpoint**         |               |                           | Specifies the address, port and relative path of an OPC endpoint                                                                                                                               |
| Password                        |               |                           | Password to log in with.                                                                                                                                                                       |
| Trusted server certificate path |               |                           | Path to the DER-encoded trusted server certificate                                                                                                                                             |
| Username                        |               |                           | Username to log in with.                                                                                                                                                                       |
### Relationships

| Name    | Description                                                              |
|---------|--------------------------------------------------------------------------|
| failure | Retrieved OPC-UA nodes where value cannot be extracted (only if enabled) |
| success | Successfully retrieved OPC-UA nodes                                      |


## FetchS3Object

### Description

Retrieves the contents of an S3 Object and writes it to the content of a FlowFile
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                             | <div style="width:7em">Default Value</div> | <div style="width:8em">Allowable Values</div>                                                                                                                                                                                                                                                                                                                                                               | Description                                                                                                                                                                                                                                                                                                 |
|----------------------------------|--------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Object Key                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The key of the S3 object. If none is given the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                                                                                       |
| **Bucket**                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The S3 bucket<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                    |
| Access Key                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | AWS account access key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                           |
| Secret Key                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | AWS account secret key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                           |
| Credentials File                 |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey                                                                                                                                                                                 |
| AWS Credentials Provider service |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.                                                                                                                                                                                                         |
| **Region**                       | us-west-2                                  | af-south-1<br/>ap-east-1<br/>ap-northeast-1<br/>ap-northeast-2<br/>ap-northeast-3<br/>ap-south-1<br/>ap-southeast-1<br/>ap-southeast-2<br/>ca-central-1<br/>cn-north-1<br/>cn-northwest-1<br/>eu-central-1<br/>eu-north-1<br/>eu-south-1<br/>eu-west-1<br/>eu-west-2<br/>eu-west-3<br/>me-south-1<br/>sa-east-1<br/>us-east-1<br/>us-east-2<br/>us-gov-east-1<br/>us-gov-west-1<br/>us-west-1<br/>us-west-2 | AWS Region                                                                                                                                                                                                                                                                                                  |
| **Communications Timeout**       | 30 sec                                     |                                                                                                                                                                                                                                                                                                                                                                                                             | Sets the timeout of the communication between the AWS server and the client                                                                                                                                                                                                                                 |
| Endpoint Override URL            |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Endpoint URL to use instead of the AWS default including scheme, host, port, and path. The AWS libraries select an endpoint URL based on the AWS region, but this property overrides the selected endpoint URL, allowing use with other S3-compatible endpoints.<br/>**Supports Expression Language: true** |
| Proxy Host                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Proxy host name or IP<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                            |
| Proxy Port                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The port number of the proxy host<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                |
| Proxy Username                   |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Username to set when authenticating against proxy<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
| Proxy Password                   |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Password to set when authenticating against proxy<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
| Version                          |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The Version of the Object to download<br/>**Supports Expression Language: true**                                                                                                                                                                                                                            |
| **Requester Pays**               | false                                      |                                                                                                                                                                                                                                                                                                                                                                                                             | If true, indicates that the requester consents to pay any charges associated with retrieving objects from the S3 bucket. This sets the 'x-amz-request-payer' header to 'requester'.                                                                                                                         |
### Relationships

| Name    | Description                                  |
|---------|----------------------------------------------|
| failure | FlowFiles are routed to failure relationship |
| success | FlowFiles are routed to success relationship |


## FetchSFTP

### Description

Fetches the content of a file from a remote SFTP server and overwrites the contents of an incoming FlowFile with the content of the remote file.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                           | Default Value | Allowable Values                     | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
|--------------------------------|---------------|--------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Completion Strategy**        | None          | Delete File<br>Move File<br>None<br> | Specifies what to do with the original file on the server once it has been pulled into NiFi. If the Completion Strategy fails, a warning will be logged but the data will still be transferred.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| **Connection Timeout**         | 30 sec        |                                      | Amount of time to wait before timing out while creating a connection                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| **Create Directory**           | false         |                                      | Specifies whether or not the remote directory should be created if it does not exist.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| **Data Timeout**               | 30 sec        |                                      | When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| Disable Directory Listing      | false         |                                      | Control how 'Move Destination Directory' is created when 'Completion Strategy' is 'Move File' and 'Create Directory' is enabled. If set to 'true', directory listing is not performed prior to create missing directories. By default, this processor executes a directory listing command to see target directory existence before creating missing directories. However, there are situations that you might need to disable the directory listing such as the following. Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, then an error is returned because the directory already exists. |
| Host Key File                  |               |                                      | If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| **Hostname**                   |               |                                      | The fully qualified hostname or IP address of the remote system<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| Http Proxy Password            |               |                                      | Http Proxy Password<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| Http Proxy Username            |               |                                      | Http Proxy Username<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| Move Destination Directory     |               |                                      | The directory on the remote server to move the original file to once it has been ingested into NiFi. This property is ignored unless the Completion Strategy is set to 'Move File'. The specified directory must already exist on the remote system if 'Create Directory' is disabled, or the rename will fail.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| Password                       |               |                                      | Password for the user account<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| **Port**                       |               |                                      | The port that the remote system is listening on for file transfers<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| Private Key Passphrase         |               |                                      | Password for the private key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| Private Key Path               |               |                                      | The fully qualified path to the Private Key file<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| Proxy Host                     |               |                                      | The fully qualified hostname or IP address of the proxy server<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| Proxy Port                     |               |                                      | The port of the proxy server<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| Proxy Type                     | DIRECT        | DIRECT<br>HTTP<br>SOCKS<br>          | Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. Supported proxies: HTTP + AuthN, SOCKS + AuthN                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| **Remote File**                |               |                                      | The fully qualified filename on the remote system<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| **Send Keep Alive On Timeout** | true          |                                      | Indicates whether or not to send a single Keep Alive message when SSH socket times out                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| **Strict Host Key Checking**   | false         |                                      | Indicates whether or not strict enforcement of hosts keys should be applied                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| **Use Compression**            | false         |                                      | Indicates whether or not ZLIB compression should be used when transferring files                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| **Username**                   |               |                                      | Username<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
### Relationships

| Name              | Description                                                                                                                             |
|-------------------|-----------------------------------------------------------------------------------------------------------------------------------------|
| comms.failure     | Any FlowFile that could not be fetched from the remote server due to a communications failure will be transferred to this Relationship. |
| not.found         | Any FlowFile for which we receive a 'Not Found' message from the remote server will be transferred to this Relationship.                |
| permission.denied | Any FlowFile that could not be fetched from the remote server due to insufficient permissions will be transferred to this Relationship. |
| success           | All FlowFiles that are received are routed to success                                                                                   |


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

| Name             | Default Value | Allowable Values   | Description                                                                                                                                                                                                                                                                                                                      |
|------------------|---------------|--------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Batch Size       | 1             |                    | The number of FlowFiles to be transferred in each invocation                                                                                                                                                                                                                                                                     |
| Data Format      | Binary        | Text<br>Binary<br> | Specifies whether the data should be Text or Binary                                                                                                                                                                                                                                                                              |
| File Size        | 1 kB          |                    | The size of the file that will be used                                                                                                                                                                                                                                                                                           |
| Unique FlowFiles | true          |                    | If true, each FlowFile that is generated will be unique. If false, a random value will be generated and all FlowFiles                                                                                                                                                                                                            |
| Custom Text      |               |                    | If Data Format is text and if Unique FlowFiles is false, then this custom text will be used as content of the generated FlowFiles and the File Size will be ignored. Finally, if Expression Language is used, evaluation will be performed only once per batch of generated FlowFiles<br/>**Supports Expression Language: true** |
### Relationships

| Name    | Description                            |
|---------|----------------------------------------|
| success | success operational on the flow record |


## GetFile

### Description

Creates FlowFiles from files in a directory. MiNiFi will ignore files for which it doesn't have read permissions.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                   | Default Value | Allowable Values | Description                                                                                                                                                |
|------------------------|---------------|------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Batch Size             | 10            |                  | The maximum number of files to pull in each iteration                                                                                                      |
| File Filter            | .*            |                  | Only files whose names match the given regular expression will be picked up                                                                                |
| Ignore Hidden Files    | true          |                  | Indicates whether or not hidden files should be ignored                                                                                                    |
| **Input Directory**    |               |                  | The input directory from which to pull files<br/>**Supports Expression Language: true**                                                                    |
| Keep Source File       | false         |                  | If true, the file is not deleted after it has been copied to the Content Repository                                                                        |
| Maximum File Age       | 0 sec         |                  | The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored   |
| Maximum File Size      | 0 B           |                  | The maximum size that a file can be in order to be pulled                                                                                                  |
| Minimum File Age       | 0 sec         |                  | The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored |
| Minimum File Size      | 0 B           |                  | The minimum size that a file can be in order to be pulled                                                                                                  |
| Polling Interval       | 0 sec         |                  | Indicates how long to wait before performing a directory listing                                                                                           |
| Recurse Subdirectories | true          |                  | Indicates whether or not to pull files from subdirectories                                                                                                 |
### Relationships

| Name    | Description                     |
|---------|---------------------------------|
| success | All files are routed to success |


## GetGPS

### Description

Obtains GPS coordinates from the GPSDHost and port.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name           | Default Value | Allowable Values | Description                                               |
|----------------|---------------|------------------|-----------------------------------------------------------|
| GPSD Host      | localhost     |                  | The host running the GPSD daemon                          |
| GPSD Port      | 2947          |                  | The GPSD daemon port                                      |
| GPSD Wait Time | 50000000      |                  | Timeout value for waiting for data from the GPSD instance |
### Relationships

| Name    | Description                     |
|---------|---------------------------------|
| success | All files are routed to success |


## GetTCP

### Description

Establishes a TCP Server that defines and retrieves one or more byte messages from clients
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value | Allowable Values | Description                                                                                                                                                                                |
|----------------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| SSL Context Service        |               |                  | SSL Context Service Name                                                                                                                                                                   |
| Stay Connected             | true          |                  | Determines if we keep the same socket despite having no data                                                                                                                               |
| concurrent-handler-count   | 1             |                  | Number of concurrent handlers for this session                                                                                                                                             |
| connection-attempt-timeout | 3             |                  | Maximum number of connection attempts before attempting backup hosts, if configured                                                                                                        |
| end-of-message-byte        | 13            |                  | Byte value which denotes end of message. Must be specified as integer within the valid byte range  (-128 thru 127). For example, '13' = Carriage return and '10' = New line. Default '13'. |
| **endpoint-list**          |               |                  | A comma delimited list of the endpoints to connect to. The format should be <server_address>:<port>.                                                                                       |
| receive-buffer-size        | 16 MB         |                  | The size of the buffer to receive data in. Default 16384 (16MB).                                                                                                                           |
### Relationships

| Name    | Description                                                                                 |
|---------|---------------------------------------------------------------------------------------------|
| partial | Indicates an incomplete message as a result of encountering the end of message byte trigger |
| success | All files are routed to success                                                             |


## GetUSBCamera

### Description

Gets images from USB Video Class (UVC)-compatible devices. Outputs one flow file per frame at the rate specified by the FPS property in the format specified by the Format property.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name           | Default Value | Allowable Values | Description                                                                                         |
|----------------|---------------|------------------|-----------------------------------------------------------------------------------------------------|
| FPS            | 1             |                  | Frames per second to capture from USB camera                                                        |
| Format         | PNG           |                  | Frame format (currently only PNG and RAW are supported; RAW is a binary pixel buffer of RGB values) |
| Height         |               |                  | Target height of image to capture from USB camera                                                   |
| USB Product ID | 0x0           |                  | USB Product ID of camera device, in hexadecimal format                                              |
| USB Serial No. |               |                  | USB Serial No. of camera device                                                                     |
| USB Vendor ID  | 0x0           |                  | USB Vendor ID of camera device, in hexadecimal format                                               |
| Width          |               |                  | Target width of image to capture from USB camera                                                    |
### Relationships

| Name    | Description                           |
|---------|---------------------------------------|
| failure | Failures sent here                    |
| success | Sucessfully captured images sent here |


## HashContent

### Description

HashContent calculates the checksum of the content of the flowfile and adds it as an attribute. Configuration options exist to select hashing algorithm and set the name of the attribute.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name           | Default Value | Allowable Values | Description                                            |
|----------------|---------------|------------------|--------------------------------------------------------|
| Hash Algorithm | SHA256        |                  | Name of the algorithm used to generate checksum        |
| Hash Attribute | Checksum      |                  | Attribute to store checksum to                         |
| Fail on empty  | false         |                  | Route to failure relationship in case of empty content |
### Properties

| Name    | Description                            |
|---------|----------------------------------------|
| failure | failure operational on the flow record |
| success | success operational on the flow record |


## InvokeHTTP

### Description

An HTTP client processor which can interact with a configurable HTTP Endpoint. The destination URL and HTTP Method are configurable. FlowFile attributes are converted to HTTP headers and the FlowFile contents are included as the body of the request (if the HTTP Method is PUT, POST or PATCH).
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                            | Default Value            | Allowable Values            | Description                                                                                                                                                                                                                                                                                                     |
|-------------------------------------------------|--------------------------|-----------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Always Output Response                          | false                    |                             | Will force a response FlowFile to be generated and routed to the 'Response' relationship regardless of what the server status code received is                                                                                                                                                                  |
| Attributes to Send                              |                          |                             | Regular expression that defines which attributes to send as HTTP headers in the request. If not defined, no attributes are sent as headers.                                                                                                                                                                     |
| Connection Timeout                              | 5 secs                   |                             | Max wait time for connection to remote service.                                                                                                                                                                                                                                                                 |
| Content-type                                    | application/octet-stream |                             | The Content-Type to specify for when content is being transmitted through a PUT, POST or PATCH. In the case of an empty value after evaluating an expression language expression, Content-Type defaults to                                                                                                      |
| Disable Peer Verification                       | false                    |                             | Disables peer verification for the SSL session                                                                                                                                                                                                                                                                  |
| Follow Redirects                                | true                     |                             | Follow HTTP redirects issued by remote server.                                                                                                                                                                                                                                                                  |
| HTTP Method                                     | GET                      |                             | HTTP request method (GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS). Arbitrary methods are also supported. Methods other than POST, PUT and PATCH will be sent without a message body.                                                                                                                           |
| Include Date Header                             | true                     |                             | Include an RFC-2616 Date header in the request.                                                                                                                                                                                                                                                                 |
| **Invalid HTTP Header Field Handling Strategy** | transform                | transform<br/>fail<br/>drop | Indicates what should happen when an attribute's name is not a valid HTTP header field name.<br/>Options:<br/>transform - invalid characters are replaced<br/>fail - flow file is transferred to failure<br/>drop - drops invalid attributes from HTTP message                                                  |
| invokehttp-proxy-password                       |                          |                             | Password to set when authenticating against proxy                                                                                                                                                                                                                                                               |
| invokehttp-proxy-username                       |                          |                             | Username to set when authenticating against proxy                                                                                                                                                                                                                                                               |
| Penalize on "No Retry"                          | false                    |                             | Enabling this property will penalize FlowFiles that are routed to the "No Retry" relationship.                                                                                                                                                                                                                  |
| Proxy Host                                      |                          |                             | The fully qualified hostname or IP address of the proxy server                                                                                                                                                                                                                                                  |
| Proxy Port                                      |                          |                             | The port of the proxy server                                                                                                                                                                                                                                                                                    |
| Put Response Body in Attribute                  |                          |                             | If set, the response body received back will be put into an attribute of the original FlowFile instead of a separate FlowFile. The attribute key to put to is determined by evaluating value of this property.                                                                                                  |
| Read Timeout                                    | 15 secs                  |                             | Max wait time for response from remote service.                                                                                                                                                                                                                                                                 |
| Remote URL                                      |                          |                             | Remote URL which will be connected to, including scheme, host, port, path.<br/>**Supports Expression Language: true**                                                                                                                                                                                           |
| send-message-body                               | true                     |                             | DEPRECATED. Only kept for backwards compatibility, no functionality is included.                                                                                                                                                                                                                                |
| Send Message Body                               | true                     |                             | If true, sends the HTTP message body on POST/PUT/PATCH requests (default).  If false, suppresses the message body and content-type header for these requests.                                                                                                                                                   |
| SSL Context Service                             |                          |                             | The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.                                                                                                                                                                                                         |
| Use Chunked Encoding                            | false                    |                             | When POST'ing, PUT'ing or PATCH'ing content set this property to true in order to not pass the 'Content-length' header and instead send 'Transfer-Encoding' with a value of 'chunked'. This will enable the data transfer mechanism which was introduced in HTTP 1.1 to pass data of unknown lengths in chunks. |
### Relationships

| Name     | Description                                                                                                                                                                                                      |
|----------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| success  | The original FlowFile will be routed upon success (2xx status codes). It will have new attributes detailing the success of the request.                                                                          |
| response | A Response FlowFile will be routed upon success (2xx status codes). If the 'Always Output Response' property is true then the response will be sent to this relationship regardless of the status code received. |
| retry    | The original FlowFile will be routed on any status code that can be retried (5xx status codes). It will have new attributes detailing the request.                                                               |
| no retry | The original FlowFile will be routed on any status code that should NOT be retried (1xx, 3xx, 4xx status codes). It will have new attributes detailing the request.                                              |
| failure  | The original FlowFile will be routed on any type of connection failure, timeout or general exception. It will have new attributes detailing the request.                                                         |

### Output Attributes

| Attribute                   | Description                                                    |
|-----------------------------|----------------------------------------------------------------|
| _invokehttp.status.code_    | The status code that is returned                               |
| _invokehttp.status.message_ | The status message that is returned                            |
| _invokehttp.request.url_    | The original request URL                                       |
| _invokehttp.tx.id_          | The transaction ID that is returned after reading the response |


## ListAzureBlobStorage

### Description

Lists blobs in an Azure Storage container. Listing details are attached to an empty FlowFile for use with FetchAzureBlobStorage.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                   | Default Value | Allowable Values    | Description                                                                                                                                                                                                                                                                                        |
|----------------------------------------|---------------|---------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Azure Storage Credentials Service      |               |                     | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                                                                                         |
| Common Storage Account Endpoint Suffix |               |                     | Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a different suffix in certain circumstances (like Azure Stack or non-public Azure regions).<br/>**Supports Expression Language: true**                                                        |
| Connection String                      |               |                     | Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Managed Identity is not used.<br/>**Supports Expression Language: true**                                                                                                         |
| **Container Name**                     |               |                     | Name of the Azure storage container. In case of PutAzureBlobStorage processor, container can be created if it does not exist.<br/>**Supports Expression Language: true**                                                                                                                           |
| Listing Strategy                       | timestamps    | none<br/>timestamps | Specify how to determine new/updated entities. If 'timestamps' is selected it tracks the latest timestamp of listed entity to determine new/updated entities. If 'none' is selected it lists an entity without any tracking, the same entity will be listed each time on executing this processor. |
| Prefix                                 |               |                     | Search prefix for listing<br/>**Supports Expression Language: true**                                                                                                                                                                                                                               |
| SAS Token                              |               |                     | Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Managed Identity is not used.<br/>**Supports Expression Language: true**                                                                                        |
| Storage Account Key                    |               |                     | The storage account key. This is an admin-like password providing access to every container in this account. It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies.<br/>**Supports Expression Language: true**                             |
| Storage Account Name                   |               |                     | The storage account name.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                               |
| **Use Managed Identity Credentials**   | false         |                     | If true Managed Identity credentials will be used together with the Storage Account Name for authentication.                                                                                                                                                                                       |

### Relationships

| Name    | Description                                           |
|---------|-------------------------------------------------------|
| success | All FlowFiles that are received are routed to success |


## ListAzureDataLakeStorage

### Description

Lists directory in an Azure Data Lake Storage Gen 2 filesystem
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                  | Default Value | Allowable Values    | Description                                                                                                                                                                                                                                                                                        |
|---------------------------------------|---------------|---------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Azure Storage Credentials Service** |               |                     | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                                                                                         |
| **Filesystem Name**                   |               |                     | Name of the Azure Storage File System. It is assumed to be already existing.<br/>**Supports Expression Language: true**                                                                                                                                                                            |
| Directory Name                        |               |                     | Name of the Azure Storage Directory. The Directory Name cannot contain a leading '/'. If left empty it designates the root directory. The directory will be created if not already existing.<br/>**Supports Expression Language: true**                                                            |
| **Recurse Subdirectories**            | true          |                     | Indicates whether to list files from subdirectories of the directory                                                                                                                                                                                                                               |
| File Filter                           |               |                     | Only files whose names match the given regular expression will be listed                                                                                                                                                                                                                           |
| Path Filter                           |               |                     | When 'Recurse Subdirectories' is true, then only subdirectories whose paths match the given regular expression will be scanned                                                                                                                                                                     |
| Listing Strategy                      | timestamps    | none<br/>timestamps | Specify how to determine new/updated entities. If 'timestamps' is selected it tracks the latest timestamp of listed entity to determine new/updated entities. If 'none' is selected it lists an entity without any tracking, the same entity will be listed each time on executing this processor. |

### Relationships

| Name    | Description                                           |
|---------|-------------------------------------------------------|
| success | All FlowFiles that are received are routed to success |


## ListGCSBucket

### Description

Retrieves a listing of objects from an GCS bucket.
For each object that is listed, creates a FlowFile that represents the object so that it can be fetched or deleted in conjunction with FetchGCSObject/DeleteGCSObject.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                 | Default Value | Allowable Values                                                                  | Description                                                                        |
|--------------------------------------|---------------|-----------------------------------------------------------------------------------|------------------------------------------------------------------------------------|
| **Bucket**                           |               |                                                                                   | Bucket of the object.<br>**Supports Expression Language: true**                    |
| **Number of retries**                | 6             | integers                                                                          | How many retry attempts should be made before routing to the failure relationship. |
| **GCP Credentials Provider Service** |               | [GCPCredentialsControllerService](CONTROLLERS.md#GCPCredentialsControllerService) | The Controller Service used to obtain Google Cloud Platform credentials.           |
| Endpoint Override URL                |               |                                                                                   | Overrides the default Google Cloud Storage endpoints                               |
| List all versions                    | false         | false<br>true                                                                     | Set this option to `true` to get all the previous versions separately.             |


### Relationships

| Name    | Description                                                                                   |
|---------|-----------------------------------------------------------------------------------------------|
| success | FlowFiles are routed to this relationship after a successful Google Cloud Storage operation.  |

### Output Attributes

| Attribute                  | Relationship | Description                                                        |
|----------------------------|--------------|--------------------------------------------------------------------|
| _gcs.bucket_               | success      | Bucket of the object.                                              |
| _gcs.key, filename_        | success      | Name of the object.                                                |
| _gcs.size_                 | success      | Size of the object.                                                |
| _gcs.crc32c_               | success      | The CRC32C checksum of object's data, encoded in base64            |
| _gcs.md5_                  | success      | The MD5 hash of the object's data encoded in base64.               |
| _gcs.owner.entity_         | success      | The owner entity, in the form "user-emailAddress".                 |
| _gcs.owner.entity.id_      | success      | The ID for the entity.                                             |
| _gcs.content.encoding_     | success      | The content encoding of the object.                                |
| _gcs.content.language_     | success      | The content language of the object.                                |
| _gcs.content.disposition_  | success      | The data content disposition of the object.                        |
| _gcs.media.link_           | success      | The media download link to the object.                             |
| _gcs.self.link_            | success      | The link to this object.                                           |
| _gcs.etag_                 | success      | The HTTP 1.1 Entity tag for the object.                            |
| _gcs.generated.id_         | success      | The service-generated ID for the object                            |
| _gcs.generation_           | success      | The content generation of this object. Used for object versioning. |
| _gcs.metageneration_       | success      | The metageneration of the object.                                  |
| _gcs.create.time_          | success      | Unix timestamp of the object's creation in milliseconds            |
| _gcs.update.time_          | success      | Unix timestamp of the object's last modification in milliseconds   |
| _gcs.delete.time_          | success      | Unix timestamp of the object's deletion in milliseconds            |
| _gcs.encryption.algorithm_ | success      | The algorithm used to encrypt the object.                          |
| _gcs.encryption.sha256_    | success      | The SHA256 hash of the key used to encrypt the object              |


## ListenHTTP

### Description

Starts an HTTP Server and listens on a given base path to transform incoming requests into FlowFiles. The default URI of the Service will be http://{hostname}:{port}/contentListener. Only HEAD, POST, and GET requests are supported. PUT, and DELETE will result in an error and the HTTP response status code 405. The response body text for all requests, by default, is empty (length of 0). A static response body can be set for a given URI by sending input files to ListenHTTP with the http.type attribute set to response_body. The response body FlowFile filename attribute is appended to the Base Path property (separated by a /) when mapped to incoming requests. The mime.type attribute of the response body FlowFile is used for the Content-type header in responses. Response body content can be cleared by sending an empty (size 0) FlowFile for a given URI mapping.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                          | Default Value   | Allowable Values | Description                                                                                                                                                                                        |
|-----------------------------------------------|-----------------|------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Authorized DN Pattern                         | .*              |                  | A Regular Expression to apply against the Distinguished Name of incoming connections. If the Pattern does not match the DN, the connection will be refused.                                        |
| Base Path                                     | contentListener |                  | Base path for incoming connections                                                                                                                                                                 |
| Batch Size                                    | 0               |                  | Maximum number of buffered requests to be processed in a single batch. If set to zero all buffered requests are processed.                                                                         |
| Buffer Size                                   | 0               |                  | Maximum number of HTTP Requests allowed to be buffered before processing them when the processor is triggered. If the buffer full, the request is refused. If set to zero the buffer is unlimited. |
| HTTP Headers to receive as Attributes (Regex) |                 |                  | Specifies the Regular Expression that determines the names of HTTP Headers that should be passed along as FlowFile attributes                                                                      |
| **Listening Port**                            | 80              |                  | The Port to listen on for incoming connections. 0 means port is going to be selected randomly.                                                                                                     |
| SSL Certificate                               |                 |                  | File containing PEM-formatted file including TLS/SSL certificate and key                                                                                                                           |
| SSL Certificate Authority                     |                 |                  | File containing trusted PEM-formatted certificates                                                                                                                                                 |
| SSL Minimum Version                           | TLS1.2          | TLS1.2<br>       | Minimum TLS/SSL version allowed (TLS1.2)                                                                                                                                                           |
| SSL Verify Peer                               | no              | no<br>yes<br>    | Whether or not to verify the client's certificate (yes/no)                                                                                                                                         |
### Relationships

| Name    | Description                     |
|---------|---------------------------------|
| success | All files are routed to success |


## ListenSyslog

### Description

Listens for Syslog messages being sent to a given port over TCP or UDP.
Incoming messages are optionally checked against regular expressions for RFC5424 and RFC3164 formatted messages.
With parsing enabled the individual parts of the message will be placed as FlowFile attributes and valid messages will be transferred to success relationship, while invalid messages will be transferred to invalid relationship.
With parsing disabled all message will be routed to the success relationship, but they will only contain the sender, protocol, and port attributes.


### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                      | Default Value | Allowable Values           | Description                                                                                                                                                                                                                       |
|---------------------------|---------------|----------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Listening Port            | 514           |                            | The port for Syslog communication. (Well-known ports (0-1023) require root access)                                                                                                                                                |
| Protocol                  | UDP           | UDP<br>TCP<br>             | The protocol for Syslog communication.                                                                                                                                                                                            |
| Parse Messages            | false         | false<br>true              | Indicates if the processor should parse the Syslog messages. If set to false, each outgoing FlowFile will only contain the sender, protocol, and port, and no additional attributes.                                              |
| Max Batch Size            | 500           |                            | The maximum number of Syslog events to process at a time.                                                                                                                                                                         |
| Max Size of Message Queue | 10000         |                            | Maximum number of Syslog messages allowed to be buffered before processing them when the processor is triggered. If the buffer is full, the message is ignored. If set to zero the buffer is unlimited.                           |
| SSL Context Service       |               |                            | The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be received over a secure connection. This Property is only considered if the \<Protocol\> Property has a value of "TCP". |
| Client Auth               | NONE          | NONE<br/>WANT<br/>REQUIRED | The client authentication policy to use for the SSL Context. Only used if an SSL Context Service is provided.                                                                                                                     |

### Relationships

| Name    | Description                                                                                                                                                                                   |
|---------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| invalid | Incoming messages that do not match the expected format when parsing will be sent to this relationship.                                                                                       |
| success | Incoming messages that match the expected format when parsing will be sent to this relationship. When Parse Messages is set to false, all incoming message will be sent to this relationship. |


### Output Attributes

| Attribute                | Description                                                        | Requirements           |
|--------------------------|--------------------------------------------------------------------|------------------------|
| _syslog.protocol_        | The protocol over which the Syslog message was received.           | -                      |
| _syslog.port_            | The port over which the Syslog message was received.               | -                      |
| _syslog.sender_          | The hostname of the Syslog server that sent the message.           | -                      |
| _syslog.valid_           | An indicator of whether this message matched the expected formats. | Parsing enabled        |
| _syslog.priority_        | The priority of the Syslog message.                                | Parsed RFC5424/RFC3164 |
| _syslog.severity_        | The severity of the Syslog message.                                | Parsed RFC5424/RFC3164 |
| _syslog.facility_        | The facility of the Syslog message.                                | Parsed RFC5424/RFC3164 |
| _syslog.timestamp_       | The timestamp of the Syslog message.                               | Parsed RFC5424/RFC3164 |
| _syslog.hostname_        | The hostname of the Syslog message.                                | Parsed RFC5424/RFC3164 |
| _syslog.msg_             | The free-form message of the Syslog message.                       | Parsed RFC5424/RFC3164 |
| _syslog.version_         | The version of the Syslog message.                                 | Parsed RFC5424         |
| _syslog.app_name_        | The app name of the Syslog message.                                | Parsed RFC5424         |
| _syslog.proc_id_         | The proc id of the Syslog message.                                 | Parsed RFC5424         |
| _syslog.msg_id_          | The message id of the Syslog message.                              | Parsed RFC5424         |
| _syslog.structured_data_ | The structured data of the Syslog message.                         | Parsed RFC5424         |



## ListenTCP

### Description

Listens for incoming TCP connections and reads data from each connection using a line separator as the message demarcator. For each message the processor produces a single FlowFile.


### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                          | Default Value | Allowable Values           | Description                                                                                                                                                                                      |
|-------------------------------|---------------|----------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Listening Port**            |               |                            | The port to listen on for communication.                                                                                                                                                         |
| **Max Batch Size**            | 500           |                            | The maximum number of messages to process at a time.                                                                                                                                             |
| **Max Size of Message Queue** | 10000         |                            | Maximum number of messages allowed to be buffered before processing them when the processor is triggered. If the buffer is full, the message is ignored. If set to zero the buffer is unlimited. |
| SSL Context Service           |               |                            | The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be received over a secure connection.                                                    |
| Client Auth                   | NONE          | NONE<br/>WANT<br/>REQUIRED | The client authentication policy to use for the SSL Context. Only used if an SSL Context Service is provided.                                                                                    |

### Relationships

| Name    | Description                                                        |
|---------|--------------------------------------------------------------------|
| success | Messages received successfully will be sent out this relationship. |

### Output Attributes

| Attribute    | Description                                  | Requirements |
|--------------|----------------------------------------------|--------------|
| _tcp.port_   | The sending port the messages were received. | -            |
| _tcp.sender_ | The sending host of the messages.            | -            |




## ListenUDP

### Description

Listens for incoming UDP datagrams. For each datagram the processor produces a single FlowFile.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                          | Default Value | Allowable Values           | Description                                                                                                                                                                                      |
|-------------------------------|---------------|----------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Listening Port**            |               |                            | The port to listen on for communication.                                                                                                                                                         |
| **Max Batch Size**            | 500           |                            | The maximum number of messages to process at a time.                                                                                                                                             |
| **Max Size of Message Queue** | 10000         |                            | Maximum number of messages allowed to be buffered before processing them when the processor is triggered. If the buffer is full, the message is ignored. If set to zero the buffer is unlimited. |

### Relationships

| Name    | Description                                                        |
|---------|--------------------------------------------------------------------|
| success | Messages received successfully will be sent out this relationship. |

### Output Attributes

| Attribute    | Description                                   | Requirements |
|--------------|-----------------------------------------------|--------------|
| _udp.port_   | The sending port the messages were received.  | -            |
| _udp.sender_ | The sending host of the messages.             | -            |


## ListFile

### Description

Retrieves a listing of files from the local filesystem. For each file that is listed, creates a FlowFile that represents the file so that it can be fetched in conjunction with FetchFile.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value | Allowable Values | Description                                                                                                                                                |
|----------------------------|---------------|------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Input Directory**        |               |                  | The input directory from which files to pull files                                                                                                         |
| **Recurse Subdirectories** | true          |                  | Indicates whether to list files from subdirectories of the directory                                                                                       |
| File Filter                |               |                  | Only files whose names match the given regular expression will be picked up                                                                                |
| Path Filter                |               |                  | When Recurse Subdirectories is true, then only subdirectories whose path matches the given regular expression will be scanned                              |
| **Minimum File Age**       | 0 sec         |                  | The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored |
| Maximum File Age           |               |                  | The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored   |
| **Minimum File Size**      | 0 B           |                  | The minimum size that a file must be in order to be pulled                                                                                                 |
| Maximum File Size          |               |                  | The maximum size that a file can be in order to be pulled                                                                                                  |
| **Ignore Hidden Files**    | true          |                  | Indicates whether or not hidden files should be ignored                                                                                                    |
### Relationships

| Name    | Description                                           |
|---------|-------------------------------------------------------|
| success | All FlowFiles that are received are routed to success |

### Output Attributes

| Attribute               | Relationship | Description                                                                                                                                                                                                                                                                                                                                                                                           |
|-------------------------|--------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| _filename_              | success      | The name of the file that was read from filesystem.                                                                                                                                                                                                                                                                                                                                                   |
| _path_                  | success      | The path is set to the relative path of the file's directory on filesystem compared to the Input Directory property. For example, if Input Directory is set to /tmp, then files picked up from /tmp will have the path attribute set to "./". If the Recurse Subdirectories property is set to true and a file is picked up from /tmp/abc/1/2/3, then the path attribute will be set to "abc/1/2/3/". |
| _absolute.path_         | success      | The absolute.path is set to the absolute path of the file's directory on filesystem. For example, if the Input Directory property is set to /tmp, then files picked up from /tmp will have the path attribute set to "/tmp/". If the Recurse Subdirectories property is set to true and a file is picked up from /tmp/abc/1/2/3, then the path attribute will be set to "/tmp/abc/1/2/3/".            |
| _file.owner_            | success      | The user that owns the file in filesystem                                                                                                                                                                                                                                                                                                                                                             |
| _file.group_            | success      | The group that owns the file in filesystem                                                                                                                                                                                                                                                                                                                                                            |
| _file.size_             | success      | The number of bytes in the file in filesystem                                                                                                                                                                                                                                                                                                                                                         |
| _file.permissions_      | success      | The permissions for the file in filesystem. This is formatted as 3 characters for the owner, 3 for the group, and 3 for other users. For example rw-rw-r--                                                                                                                                                                                                                                            |
| _file.lastModifiedTime_ | success      | The timestamp of when the file in filesystem was last modified as 'yyyy-MM-dd'T'HH:mm:ssZ'                                                                                                                                                                                                                                                                                                            |


## ListS3

### Description

Retrieves a listing of objects from an S3 bucket. For each object that is listed, creates a FlowFile that represents the object so that it can be fetched in conjunction with FetchS3Object.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                             | Default Value | Allowable Values                                                                                                                                                                                                                                                                                                                                                                                            | Description                                                                                                                                                                                                                                                                                                 |
|----------------------------------|---------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Bucket**                       |               |                                                                                                                                                                                                                                                                                                                                                                                                             | The S3 bucket<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                    |
| Access Key                       |               |                                                                                                                                                                                                                                                                                                                                                                                                             | AWS account access key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                           |
| Secret Key                       |               |                                                                                                                                                                                                                                                                                                                                                                                                             | AWS account secret key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                           |
| Credentials File                 |               |                                                                                                                                                                                                                                                                                                                                                                                                             | Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey                                                                                                                                                                                 |
| AWS Credentials Provider service |               |                                                                                                                                                                                                                                                                                                                                                                                                             | The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.                                                                                                                                                                                                         |
| **Region**                       | us-west-2     | af-south-1<br/>ap-east-1<br/>ap-northeast-1<br/>ap-northeast-2<br/>ap-northeast-3<br/>ap-south-1<br/>ap-southeast-1<br/>ap-southeast-2<br/>ca-central-1<br/>cn-north-1<br/>cn-northwest-1<br/>eu-central-1<br/>eu-north-1<br/>eu-south-1<br/>eu-west-1<br/>eu-west-2<br/>eu-west-3<br/>me-south-1<br/>sa-east-1<br/>us-east-1<br/>us-east-2<br/>us-gov-east-1<br/>us-gov-west-1<br/>us-west-1<br/>us-west-2 | AWS Region                                                                                                                                                                                                                                                                                                  |
| **Communications Timeout**       | 30 sec        |                                                                                                                                                                                                                                                                                                                                                                                                             | Sets the timeout of the communication between the AWS server and the client                                                                                                                                                                                                                                 |
| Endpoint Override URL            |               |                                                                                                                                                                                                                                                                                                                                                                                                             | Endpoint URL to use instead of the AWS default including scheme, host, port, and path. The AWS libraries select an endpoint URL based on the AWS region, but this property overrides the selected endpoint URL, allowing use with other S3-compatible endpoints.<br/>**Supports Expression Language: true** |
| Proxy Host                       |               |                                                                                                                                                                                                                                                                                                                                                                                                             | Proxy host name or IP<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                            |
| Proxy Port                       |               |                                                                                                                                                                                                                                                                                                                                                                                                             | The port number of the proxy host<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                |
| Proxy Username                   |               |                                                                                                                                                                                                                                                                                                                                                                                                             | Username to set when authenticating against proxy<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
| Proxy Password                   |               |                                                                                                                                                                                                                                                                                                                                                                                                             | Password to set when authenticating against proxy<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
| Delimiter                        |               |                                                                                                                                                                                                                                                                                                                                                                                                             | The string used to delimit directories within the bucket. Please consult the AWS documentation for the correct use of this field.                                                                                                                                                                           |
| Prefix                           |               |                                                                                                                                                                                                                                                                                                                                                                                                             | The prefix used to filter the object list. In most cases, it should end with a forward slash ('/').                                                                                                                                                                                                         |
| **Use Versions**                 | false         |                                                                                                                                                                                                                                                                                                                                                                                                             | Specifies whether to use S3 versions, if applicable. If false, only the latest version of each object will be returned.                                                                                                                                                                                     |
| **Minimum Object Age**           | 0 sec         |                                                                                                                                                                                                                                                                                                                                                                                                             | The minimum age that an S3 object must be in order to be considered; any object younger than this amount of time (according to last modification date) will be ignored.                                                                                                                                     |
| **Write Object Tags**            | false         |                                                                                                                                                                                                                                                                                                                                                                                                             | If set to 'true', the tags associated with the S3 object will be written as FlowFile attributes.                                                                                                                                                                                                            |
| **Write User Metadata**          | false         |                                                                                                                                                                                                                                                                                                                                                                                                             | If set to 'true', the user defined metadata associated with the S3 object will be added to FlowFile attributes/records.                                                                                                                                                                                     |
| **Requester Pays**               | false         |                                                                                                                                                                                                                                                                                                                                                                                                             | If true, indicates that the requester consents to pay any charges associated with listing the S3 bucket. This sets the 'x-amz-request-payer' header to 'requester'. Note that this setting is only used if Write User Metadata is true.                                                                     |


## ListSFTP

### Description

Performs a listing of the files residing on an SFTP server. For each file that is found on the remote server, a new FlowFile will be created with the filename attribute set to the name of the file on the remote server. This can then be used in conjunction with FetchSFTP in order to fetch those files.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                   | Default Value       | Allowable Values                                      | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
|----------------------------------------|---------------------|-------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Connection Timeout**                 | 30 sec              |                                                       | Amount of time to wait before timing out while creating a connection                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| **Data Timeout**                       | 30 sec              |                                                       | When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| Entity Tracking Initial Listing Target | All Available       | All Available<br>Tracking Time Window<br>             | Specify how initial listing should be handled. Used by 'Tracking Entities' strategy.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| Entity Tracking Time Window            |                     |                                                       | Specify how long this processor should track already-listed entities. 'Tracking Entities' strategy can pick any entity whose timestamp is inside the specified time window. For example, if set to '30 minutes', any entity having timestamp in recent 30 minutes will be the listing target when this processor runs. A listed entity is considered 'new/updated' and a FlowFile is emitted if one of following condition meets: 1. does not exist in the already-listed entities, 2. has newer timestamp than the cached entity, 3. has different size than the cached entity. If a cached entity's timestamp becomes older than specified time window, that entity will be removed from the cached already-listed entities. Used by 'Tracking Entities' strategy. |
| File Filter Regex                      |                     |                                                       | Provides a Java Regular Expression for filtering Filenames; if a filter is supplied, only files whose names match that Regular Expression will be fetched                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| **Follow symlink**                     | false               |                                                       | If true, will pull even symbolic files and also nested symbolic subdirectories; otherwise, will not read symbolic files and will not traverse symbolic link subdirectories                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| Host Key File                          |                     |                                                       | If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| **Hostname**                           |                     |                                                       | The fully qualified hostname or IP address of the remote system<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| Http Proxy Password                    |                     |                                                       | Http Proxy Password<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| Http Proxy Username                    |                     |                                                       | Http Proxy Username<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| **Ignore Dotted Files**                | true                |                                                       | If true, files whose names begin with a dot (".") will be ignored                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| **Listing Strategy**                   | Tracking Timestamps | Tracking Entities<br>Tracking Timestamps<br>          | Specify how to determine new/updated entities. See each strategy descriptions for detail.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Maximum File Age                       |                     |                                                       | The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| Maximum File Size                      |                     |                                                       | The maximum size that a file must be in order to be pulled                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| **Minimum File Age**                   | 0 sec               |                                                       | The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| **Minimum File Size**                  | 0 B                 |                                                       | The minimum size that a file must be in order to be pulled                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| Password                               |                     |                                                       | Password for the user account<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| Path Filter Regex                      |                     |                                                       | When Search Recursively is true, then only subdirectories whose path matches the given Regular Expression will be scanned                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| **Port**                               |                     |                                                       | The port that the remote system is listening on for file transfers<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| Private Key Passphrase                 |                     |                                                       | Password for the private key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| Private Key Path                       |                     |                                                       | The fully qualified path to the Private Key file<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| Proxy Host                             |                     |                                                       | The fully qualified hostname or IP address of the proxy server<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Proxy Port                             |                     |                                                       | The port of the proxy server<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| Proxy Type                             | DIRECT              | DIRECT<br>HTTP<br>SOCKS<br>                           | Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. Supported proxies: HTTP + AuthN, SOCKS + AuthN                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| Remote Path                            |                     |                                                       | The fully qualified filename on the remote system<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| **Search Recursively**                 | false               |                                                       | If true, will pull files from arbitrarily nested subdirectories; otherwise, will not traverse subdirectories                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| **Send Keep Alive On Timeout**         | true                |                                                       | Indicates whether or not to send a single Keep Alive message when SSH socket times out                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| **State File**                         | ListSFTP            |                                                       | Specifies the file that should be used for storing state about what data has been ingested so that upon restart MiNiFi can resume from where it left off                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| **Strict Host Key Checking**           | false               |                                                       | Indicates whether or not strict enforcement of hosts keys should be applied                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| **Target System Timestamp Precision**  | Auto Detect         | Auto Detect<br>Milliseconds<br>Minutes<br>Seconds<br> | Specify timestamp precision at the target system. Since this processor uses timestamp of entities to decide which should be listed, it is crucial to use the right timestamp precision.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| **Username**                           |                     |                                                       | Username<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
### Relationships

| Name    | Description                                           |
|---------|-------------------------------------------------------|
| success | All FlowFiles that are received are routed to success |


### Relationships

| Name    | Description                                  |
|---------|----------------------------------------------|
| success | FlowFiles are routed to success relationship |

## LogAttribute

### Description

Logs attributes of flow files in the MiNiFi application log.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                        | Default Value | Allowable Values                            | Description                                                                                                                                                    |
|-----------------------------|---------------|---------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Attributes to Ignore        |               |                                             | A comma-separated list of Attributes to ignore. If not specified, no attributes will be ignored.                                                               |
| Attributes to Log           |               |                                             | A comma-separated list of Attributes to Log. If not specified, all attributes will be logged.                                                                  |
| FlowFiles To Log            | 1             |                                             | Number of flow files to log. If set to zero all flow files will be logged. Please note that this may block other threads from running if not used judiciously. |
| Hexencode Payload           | false         |                                             | If true, the FlowFile's payload will be logged in a hexencoded format                                                                                          |
| Log Level                   |               | debug<br>error<br>info<br>trace<br>warn<br> | The Log Level to use when logging the Attributes                                                                                                               |
| Log Payload                 | false         |                                             | If true, the FlowFile's payload will be logged, in addition to its attributes.otherwise, just the Attributes will be logged                                    |
| Log Prefix                  |               |                                             | Log prefix appended to the log lines. It helps to distinguish the output of multiple LogAttribute processors.                                                  |
| Maximum Payload Line Length | 0             |                                             | The logged payload will be broken into lines this long. 0 means no newlines will be added.                                                                     |
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
| After       |               |                  | For operations which result in new entries, places the new entry after the entry specified by this property.  |
| Before      |               |                  | For operations which result in new entries, places the new entry before the entry specified by this property. |
| Destination |               |                  | Destination for operations (touch, move or copy) which result in new entries.                                 |
| Operation   |               |                  | Operation to perform on the archive (touch, remove, copy, move).                                              |
| Target      |               |                  | An existing entry within the archive to perform the operation on.                                             |
### Relationships

| Name    | Description                                                                          |
|---------|--------------------------------------------------------------------------------------|
| failure | FlowFiles will be transferred to the failure relationship if the operation fails.    |
| success | FlowFiles will be transferred to the success relationship if the operation succeeds. |


## MergeContent

### Description

Merges a Group of FlowFiles together based on a user-defined strategy and packages them into a single FlowFile. MergeContent should be configured with only one incoming connection as it won't create grouped Flow Files.This processor updates the mime.type attribute as appropriate.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value               | Allowable Values                                              | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
|----------------------------|-----------------------------|---------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Attribute Strategy         | Keep Only Common Attributes | Keep Only Common Attributes<Br>Keep All Unique Attributes<Br> | Determines which FlowFile attributes should be added to the bundle. If 'Keep All Unique Attributes' is selected, any attribute on any FlowFile that gets bundled will be kept unless its value conflicts with the value from another FlowFile (in which case neither, or none, of the conflicting attributes will be kept). If 'Keep Only Common Attributes' is selected, only the attributes that exist on all FlowFiles in the bundle, with the same value, will be preserved. |
| Correlation Attribute Name |                             |                                                               | Correlation Attribute Name                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| Delimiter Strategy         | Filename                    |                                                               | Determines if Header, Footer, and Demarcator should point to files                                                                                                                                                                                                                                                                                                                                                                                                               |
| Demarcator File            |                             |                                                               | Filename specifying the demarcator to use                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| Footer File                |                             |                                                               | Filename specifying the footer to use                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Header File                |                             |                                                               | Filename specifying the header to use                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Keep Path                  | false                       |                                                               | If using the Zip or Tar Merge Format, specifies whether or not the FlowFiles' paths should be included in their entry                                                                                                                                                                                                                                                                                                                                                            |
| Max Bin Age                |                             |                                                               | The maximum age of a Bin that will trigger a Bin to be complete. Expected format is <duration> <time unit>                                                                                                                                                                                                                                                                                                                                                                       |
| Maximum Group Size         |                             |                                                               | The maximum size for the bundle. If not specified, there is no maximum.                                                                                                                                                                                                                                                                                                                                                                                                          |
| Maximum Number of Entries  |                             |                                                               | The maximum number of files to include in a bundle. If not specified, there is no maximum.                                                                                                                                                                                                                                                                                                                                                                                       |
| Maximum number of Bins     | 100                         |                                                               | Specifies the maximum number of bins that can be held in memory at any one time                                                                                                                                                                                                                                                                                                                                                                                                  |
| Merge Format               | Binary Concatenation        |                                                               | Merge Format                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| Merge Strategy             | Defragment                  |                                                               | Defragment or Bin-Packing Algorithm                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| Minimum Group Size         | 0                           |                                                               | The minimum size of for the bundle                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Minimum Number of Entries  | 1                           |                                                               | The minimum number of files to include in a bundle                                                                                                                                                                                                                                                                                                                                                                                                                               |
### Relationships

| Name     | Description                                                                                                                   |
|----------|-------------------------------------------------------------------------------------------------------------------------------|
| failure  | If the bundle cannot be created, all FlowFiles that would have been used to created the bundle will be transferred to failure |
| merged   | The FlowFile containing the merged content                                                                                    |
| original | The FlowFiles that were used to create the bundle                                                                             |


## MotionDetector

### Description

Detect motion from captured images.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                           | Default Value | Allowable Values | Description                                                                               |
|--------------------------------|---------------|------------------|-------------------------------------------------------------------------------------------|
| **Dilate iteration**           | 10            |                  | For image processing, if an object is detected as 2 separate objects, increase this value |
| **Image Encoding**             | .jpg          | .jpg<br>.png<br> | The encoding that should be applied to the output                                         |
| **Minimum Area**               | 100           |                  | We only consider the movement regions with area greater than this.                        |
| **Path to background frame**   |               |                  | If not provided then the processor will take the first input frame as background          |
| **Threshold for segmentation** | 42            |                  | Pixel greater than this will be white, otherwise black.                                   |
### Relationships

| Name    | Description                 |
|---------|-----------------------------|
| failure | Failure to detect motion    |
| success | Successful to detect motion |


## PerformanceDataMonitor

### Description
This Windows only processor can create FlowFiles populated with various performance data with the help of Windows Performance Counters.
Windows Performance Counters provide a high-level abstraction layer with a consistent interface for collecting various kinds of system data such as CPU, memory, and disk usage statistics.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                    | Default Value | Allowable Values                                            | Description                                                                                                                                           |
|-------------------------|---------------|-------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------|
| Predefined Groups       |               | CPU<br>IO<br>Disk<br>Network<br>Memory<br>System<br>Process | Comma separated list from the allowable values, to monitor multiple common Windows Performance counters related to these groups. (e.g. "CPU,Network") |
| Custom PDH Counters     |               |                                                             | Comma separated list of Windows Performance Counters to monitor. (e.g. "\\System\\Threads,\\Process(*)\\ID Process")                                  |
| **Output Format**       | JSON          | JSON<br>OpenTelemetry                                       | The output format of the new flowfile                                                                                                                 |
| **Output Compactness**  | Pretty        | Pretty<br>Compact                                           | The output format of the new flowfile                                                                                                                 |
| Round to decimal places |               | integers                                                    | The number of decimal places to round the values to (blank for no rounding)                                                                           |

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
### Properties

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

| Name                    | Default Value | Allowable Values      | Description                                                                                |
|-------------------------|---------------|-----------------------|--------------------------------------------------------------------------------------------|
| **Output Format**       | JSON          | JSON<br>OpenTelemetry | The output format of the new flowfile                                                      |
| **Output Compactness**  | Pretty        | Pretty<br>Compact     | The output type of the new flowfile                                                        |
| **Result Type**         | Absolute      | Absolute<br>Relative  | Absolute returns the current procfs values, relative calculates the usage between triggers |
| Round to decimal places |               | integers              | The number of decimal places to round the values to (blank for no rounding)                |


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
| Attributes to Send as Headers |               |                                                   | Any attribute whose name matches the regex will be added to the Kafka messages as a Header                                                                                                                                                                              |
| Batch Size                    | 10            |                                                   | Maximum number of messages batched in one MessageSet                                                                                                                                                                                                                    |
| **Client Name**               |               |                                                   | Client Name to use when communicating with Kafka<br/>**Supports Expression Language: true**                                                                                                                                                                             |
| Compress Codec                | none          |                                                   | compression codec to use for compressing message sets                                                                                                                                                                                                                   |
| Debug contexts                |               |                                                   | A comma-separated list of debug contexts to enable.Including: generic, broker, topic, metadata, feature, queue, msg, protocol, cgrp, security, fetch, interceptor, plugin, consumer, admin, eos, all                                                                    |
| Delivery Guarantee            | 1             |                                                   | Specifies the requirement for guaranteeing that a message is sent to Kafka. Valid values are 0 (do not wait for acks), -1 or all (block until message is committed by all in sync replicas) or any concrete number of nodes.<br/>**Supports Expression Language: true** |
| Kerberos Keytab Path          |               |                                                   | The path to the location on the local filesystem where the kerberos keytab is located. Read permission on the file is required.                                                                                                                                         |
| Kerberos Principal            |               |                                                   | Keberos Principal                                                                                                                                                                                                                                                       |
| Kerberos Service Name         |               |                                                   | Kerberos Service Name                                                                                                                                                                                                                                                   |
| **Known Brokers**             |               |                                                   | A comma-separated list of known Kafka Brokers in the format <host>:<port><br/>**Supports Expression Language: true**                                                                                                                                                    |
| Max Flow Segment Size         | 0 B           |                                                   | Maximum flow content payload segment size for the kafka record. 0 B means unlimited.                                                                                                                                                                                    |
| Max Request Size              |               |                                                   | Maximum Kafka protocol request message size                                                                                                                                                                                                                             |
| Kafka Key                     |               |                                                   | The key to use for the message. If not specified, the UUID of the flow file is used as the message key.<br/>**Supports Expression Language: true**                                                                                                                      |
| Message Key Field             |               |                                                   | DEPRECATED, does not work -- use Kafka Key instead                                                                                                                                                                                                                      |
| Message Timeout               | 30 sec        |                                                   | The total time sending a message could take                                                                                                                                                                                                                             |
| Password                      |               |                                                   | The password for the given username when the SASL Mechanism is sasl_plaintext                                                                                                                                                                                           |
| Queue Buffering Max Time      |               |                                                   | Delay to wait for messages in the producer queue to accumulate before constructing message batches                                                                                                                                                                      |
| Queue Max Buffer Size         |               |                                                   | Maximum total message size sum allowed on the producer queue                                                                                                                                                                                                            |
| Queue Max Message             |               |                                                   | Maximum number of messages allowed on the producer queue                                                                                                                                                                                                                |
| Request Timeout               | 10 sec        |                                                   | The ack timeout of the producer request                                                                                                                                                                                                                                 |
| SASL Mechanism                | GSSAPI        | GSSAPI<br/>PLAIN                                  | The SASL mechanism to use for authentication. Corresponds to Kafka's 'sasl.mechanism' property.                                                                                                                                                                         |
| SSL Context Service           |               |                                                   | SSL Context Service Name                                                                                                                                                                                                                                                |
| Security CA                   |               |                                                   | DEPRECATED in favor of SSL Context Service. File or directory path to CA certificate(s) for verifying the broker's key                                                                                                                                                  |
| Security Cert                 |               |                                                   | DEPRECATED in favor of SSL Context Service. Path to client's public key (PEM) used for authentication                                                                                                                                                                   |
| Security Pass Phrase          |               |                                                   | DEPRECATED in favor of SSL Context Service. Private key passphrase                                                                                                                                                                                                      |
| Security Private Key          |               |                                                   | DEPRECATED in favor of SSL Context Service. Path to client's private key (PEM) used for authentication                                                                                                                                                                  |
| **Security Protocol**         | plaintext     | plaintext<br/>ssl<br/>sasl_plaintext<br/>sasl_ssl | Protocol used to communicate with brokers. Corresponds to Kafka's 'security.protocol' property.                                                                                                                                                                         |
| Username                      |               |                                                   | The username when the SASL Mechanism is sasl_plaintext                                                                                                                                                                                                                  |
| Target Batch Payload Size     | 512 KB        |                                                   | The target total payload size for a batch. 0 B means unlimited (Batch Size is still applied).                                                                                                                                                                           |
| **Topic Name**                |               |                                                   | The Kafka Topic of interest<br/>**Supports Expression Language: true**                                                                                                                                                                                                  |
### Relationships

| Name    | Description                                                                         |
|---------|-------------------------------------------------------------------------------------|
| failure | Any FlowFile that cannot be sent to Kafka will be routed to this Relationship       |
| success | Any FlowFile that is successfully sent to Kafka will be routed to this Relationship |

## PublishMQTT

### Description

PublishMQTT serializes FlowFile content as an MQTT payload, sending the message to the configured topic and broker.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                  | Default Value | Allowable Values | Description                                                                                                                 |
|-----------------------|---------------|------------------|-----------------------------------------------------------------------------------------------------------------------------|
| **Broker URI**        |               |                  | The URI to use to connect to the MQTT broker                                                                                |
| **Topic**             |               |                  | The topic to publish to                                                                                                     |
| Client ID             |               |                  | MQTT client ID to use                                                                                                       |
| Quality of Service    | 0             |                  | The Quality of Service (QoS) to send the message with. Accepts three values '0', '1' and '2'                                |
| Connection Timeout    | 30 sec        |                  | Maximum time interval the client will wait for the network connection to the MQTT broker                                    |
| Keep Alive Interval   | 60 sec        |                  | Defines the maximum time interval between messages being sent to the broker                                                 |
| Max Flow Segment Size |               |                  | Maximum flow content payload segment size for the MQTT record                                                               |
| Last Will Topic       |               |                  | The topic to send the client's Last Will to. If the Last Will topic is not set then a Last Will will not be sent            |
| Last Will Message     |               |                  | The message to send as the client's Last Will. If the Last Will Message is empty, Last Will will be deleted from the broker |
| Last Will QoS         | 0             |                  | The Quality of Service (QoS) to send the last will with. Accepts three values '0', '1' and '2'                              |
| Last Will Retain      | false         |                  | Whether to retain the client's Last Will                                                                                    |
| Security Protocol     |               |                  | Protocol used to communicate with brokers                                                                                   |
| Security CA           |               |                  | File or directory path to CA certificate(s) for verifying the broker's key                                                  |
| Security Cert         |               |                  | Path to client's public key (PEM) used for authentication                                                                   |
| Security Private Key  |               |                  | Path to client's private key (PEM) used for authentication                                                                  |
| Security Pass Phrase  |               |                  | Private key passphrase                                                                                                      |
| Username              |               |                  | Username to use when connecting to the broker                                                                               |
| Password              |               |                  | Password to use when connecting to the broker                                                                               |
| Retain                | false         |                  | Retain published message in broker                                                                                          |


### Relationships

| Name    | Description                                                                                  |
|---------|----------------------------------------------------------------------------------------------|
| failure | FlowFiles that failed to send to the destination are transferred to this relationship        |
| success | FlowFiles that are sent successfully to the destination are transferred to this relationship |


## PutAzureBlobStorage

### Description

Puts content into an Azure Storage Blob
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                   | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                            |
|----------------------------------------|---------------|------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Azure Storage Credentials Service      |               |                  | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                                                             |
| **Blob**                               |               |                  | The filename of the blob. If left empty the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                                                     |
| Common Storage Account Endpoint Suffix |               |                  | Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a different suffix in certain circumstances (like Azure Stack or non-public Azure regions).<br/>**Supports Expression Language: true**                            |
| Connection String                      |               |                  | Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Managed Identity is not used.<br/>**Supports Expression Language: true**                                                                             |
| **Container Name**                     |               |                  | Name of the Azure storage container. In case of PutAzureBlobStorage processor, container can be created if it does not exist.<br/>**Supports Expression Language: true**                                                                                               |
| **Create Container**                   | false         |                  | Specifies whether to check if the container exists and to automatically create it if it does not. Permission to list containers is required. If false, this check is not made, but the Put operation will fail if the container does not exist.                        |
| SAS Token                              |               |                  | Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Managed Identity is not used.<br/>**Supports Expression Language: true**                                                            |
| Storage Account Key                    |               |                  | The storage account key. This is an admin-like password providing access to every container in this account. It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies.<br/>**Supports Expression Language: true** |
| Storage Account Name                   |               |                  | The storage account name.<br/>**Supports Expression Language: true**                                                                                                                                                                                                   |
| **Use Managed Identity Credentials**   | false         |                  | If true Managed Identity credentials will be used together with the Storage Account Name for authentication.                                                                                                                                                           |

### Relationships

| Name    | Description                                                             |
|---------|-------------------------------------------------------------------------|
| failure | Unsuccessful operations will be transferred to the failure relationship |
| success | All successfully processed FlowFiles are routed to this relationship    |


## PutAzureDataLakeStorage

### Description

Puts content into an Azure Data Lake Storage Gen 2
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                  | Default Value | Allowable Values            | Description                                                                                                                                                                                                                             |
|---------------------------------------|---------------|-----------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Azure Storage Credentials Service** |               |                             | Name of the Azure Storage Credentials Service used to retrieve the connection string from.                                                                                                                                              |
| **Conflict Resolution Strategy**      | fail          | fail<br/>replace<br/>ignore | Indicates what should happen when a file with the same name already exists in the output directory.                                                                                                                                     |
| File Name                             |               |                             | The filename in Azure Storage. If left empty the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                 |
| **Filesystem Name**                   |               |                             | Name of the Azure Storage File System. It is assumed to be already existing.<br/>**Supports Expression Language: true**                                                                                                                 |
| Directory Name                        |               |                             | Name of the Azure Storage Directory. The Directory Name cannot contain a leading '/'. If left empty it designates the root directory. The directory will be created if not already existing.<br/>**Supports Expression Language: true** |

### Relationships

| Name    | Description                                                                                           |
|---------|-------------------------------------------------------------------------------------------------------|
| failure | Files that could not be written to Azure storage for some reason are transferred to this relationship |
| success | Files that have been successfully written to Azure storage are transferred to this relationship       |


## PutGCSObject

### Description

Puts content into a Google Cloud Storage bucket
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                 | Default Value | Allowable Values                                                                                          | Description                                                                                                                                                                                                                                               |
|--------------------------------------|---------------|-----------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Bucket**                           | ${gcs.bucket} |                                                                                                           | Bucket of the object.<br>**Supports Expression Language: true**                                                                                                                                                                                           |
| **Key**                              | ${filename}   |                                                                                                           | Name of the object.<br>**Supports Expression Language: true**                                                                                                                                                                                             |
| **Number of retries**                | 6             | integers                                                                                                  | How many retry attempts should be made before routing to the failure relationship.                                                                                                                                                                        |
| **GCP Credentials Provider Service** |               | [GCPCredentialsControllerService](CONTROLLERS.md#GCPCredentialsControllerService)                         | The Controller Service used to obtain Google Cloud Platform credentials.                                                                                                                                                                                  |
| Object ACL                           |               | authenticatedRead<br>bucketOwnerFullControl<br>bucketOwnerRead<br>private<br>projectPrivate<br>publicRead | Access Control to be attached to the object uploaded. Not providing this will revert to bucket defaults. For more information please visit [Google Cloud Access control lists](https://cloud.google.com/storage/docs/access-control/lists#predefined-acl) |
| Server Side Encryption Key           |               |                                                                                                           | An AES256 Encryption Key (encoded in base64) for server-side encryption of the object.<br>**Supports Expression Language: true**                                                                                                                          |
| CRC32 Checksum                       |               |                                                                                                           | The name of the attribute where the crc32 checksum is stored for server-side validation.<br>**Supports Expression Language: true**                                                                                                                        |
| MD5 Hash                             |               |                                                                                                           | The name of the attribute where the md5 hash is stored for server-side validation.<br>**Supports Expression Language: true**                                                                                                                              |
| Content Type                         | ${mime.type}  |                                                                                                           | Content Type for the file, i.e. text/plain<br>**Supports Expression Language: true**                                                                                                                                                                      |
| Endpoint Override URL                |               |                                                                                                           | Overrides the default Google Cloud Storage endpoints                                                                                                                                                                                                      |

### Relationships

| Name    | Description                                                                            |
|---------|----------------------------------------------------------------------------------------|
| success | FlowFiles that are sent successfully to the destination are sent to this relationship. |
| failure | FlowFiles that failed to be sent to the destination are sent to this relationship.     |

### Output Attributes

| Attribute                  | Relationship | Description                                                        |
|----------------------------|--------------|--------------------------------------------------------------------|
| _gcs.status.message_       | failure      | The status message received from google cloud.                     |
| _gcs.error.reason_         | failure      | The description of the error occurred during upload.               |
| _gcs.error.domain_         | failure      | The domain of the error occurred during upload.                    |
| _gcs.bucket_               | success      | Bucket of the object.                                              |
| _gcs.key_                  | success      | Name of the object.                                                |
| _gcs.size_                 | success      | Size of the object.                                                |
| _gcs.crc32c_               | success      | The CRC32C checksum of object's data, encoded in base64            |
| _gcs.md5_                  | success      | The MD5 hash of the object's data encoded in base64.               |
| _gcs.owner.entity_         | success      | The owner entity, in the form "user-emailAddress".                 |
| _gcs.owner.entity.id_      | success      | The ID for the entity.                                             |
| _gcs.content.encoding_     | success      | The content encoding of the object.                                |
| _gcs.content.language_     | success      | The content language of the object.                                |
| _gcs.content.disposition_  | success      | The data content disposition of the object.                        |
| _gcs.media.link_           | success      | The media download link to the object.                             |
| _gcs.self.link_            | success      | The link to this object.                                           |
| _gcs.etag_                 | success      | The HTTP 1.1 Entity tag for the object.                            |
| _gcs.generated.id_         | success      | The service-generated ID for the object                            |
| _gcs.generation_           | success      | The content generation of this object. Used for object versioning. |
| _gcs.metageneration_       | success      | The metageneration of the object.                                  |
| _gcs.create.time_          | success      | Unix timestamp of the object's creation in milliseconds            |
| _gcs.update.time_          | success      | Unix timestamp of the object's last modification in milliseconds   |
| _gcs.delete.time_          | success      | Unix timestamp of the object's deletion in milliseconds            |
| _gcs.encryption.algorithm_ | success      | The algorithm used to encrypt the object.                          |
| _gcs.encryption.sha256_    | success      | The SHA256 hash of the key used to encrypt the object              |

## PutFile

### Description

Writes the contents of a FlowFile to the local file system
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                           | Default Value | Allowable Values              | Description                                                                                                                                                                          |
|--------------------------------|---------------|-------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Conflict Resolution Strategy   | fail          | fail<br>ignore<br>replace<br> | Indicates what should happen when a file with the same name already exists in the output directory                                                                                   |
| **Create Missing Directories** | true          |                               | If true, then missing destination directories will be created. If false, flowfiles are penalized and sent to failure.                                                                |
| Directory                      | .             |                               | The output directory to which to put files<br/>**Supports Expression Language: true**                                                                                                |
| Maximum File Count             | -1            |                               | Specifies the maximum number of files that can exist in the output directory                                                                                                         |
| Permissions                    |               |                               | Sets the permissions on the output file to the value of this attribute. Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems.                                |
| Directory Permissions          |               |                               | Sets the permissions on the directories being created if 'Create Missing Directories' property is set. Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems. |
### Relationships

| Name    | Description                                                             |
|---------|-------------------------------------------------------------------------|
| failure | Failed files (conflict, write failure, etc.) are transferred to failure |
| success | All files are routed to success                                         |


## PutOPCProcessor

### Description

Creates/updates  OPC nodes
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                            | Default Value | Allowable Values                                                               | Description                                                                                                                                                                                    |
|---------------------------------|---------------|--------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Application URI                 |               |                                                                                | Application URI of the client in the format 'urn:unconfigured:application'. Mandatory, if using Secure Channel and must match the URI included in the certificate's Subject Alternative Names. |
| Certificate path                |               |                                                                                | Path to the DER-encoded cert file                                                                                                                                                              |
| Key path                        |               |                                                                                | Path to the DER-encoded key file                                                                                                                                                               |
| **OPC server endpoint**         |               |                                                                                | Specifies the address, port and relative path of an OPC endpoint                                                                                                                               |
| **Parent node ID**              |               |                                                                                | Specifies the ID of the root node to traverse                                                                                                                                                  |
| **Parent node ID type**         |               | Int<br>Path<br>String<br>                                                      | Specifies the type of the provided node ID                                                                                                                                                     |
| Parent node namespace index     | 0             |                                                                                | The index of the namespace. Used only if node ID type is not path.                                                                                                                             |
| Password                        |               |                                                                                | Password to log in with.                                                                                                                                                                       |
| Target node ID                  |               |                                                                                | ID of target node.<br/>**Supports Expression Language: true**                                                                                                                                  |
| Target node ID type             |               |                                                                                | ID type of target node. Allowed values are: Int, String.<br/>**Supports Expression Language: true**                                                                                            |
| Target node browse name         |               |                                                                                | Browse name of target node. Only used when new node is created.<br/>**Supports Expression Language: true**                                                                                     |
| Target node namespace index     |               |                                                                                | The index of the namespace. Used only if node ID type is not path.<br/>**Supports Expression Language: true**                                                                                  |
| Trusted server certificate path |               |                                                                                | Path to the DER-encoded trusted server certificate                                                                                                                                             |
| Username                        |               |                                                                                | Username to log in with.                                                                                                                                                                       |
| **Value type**                  |               | Boolean<br>Double<br>Float<br>Int32<br>Int64<br>String<br>UInt32<br>UInt64<br> | Set the OPC value type of the created nodes                                                                                                                                                    |
### Relationships

| Name    | Description                  |
|---------|------------------------------|
| failure | Failed to put OPC-UA node    |
| success | Successfully put OPC-UA node |


## PutS3Object

### Description

Puts FlowFiles to an Amazon S3 Bucket. The upload uses either the PutS3Object method. The PutS3Object method send the file in a single synchronous call, but it has a 5GB size limit. Larger files sent using the multipart upload methods are currently not supported. The AWS libraries select an endpoint URL based on the AWS region, but this can be overridden with the 'Endpoint Override URL' property for use with other S3-compatible endpoints. The S3 API specifies that the maximum file size for a PutS3Object upload is 5GB.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                             | <div style="width:7em">Default Value</div> | <div style="width:8em">Allowable Values</div>                                                                                                                                                                                                                                                                                                                                                               | Description                                                                                                                                                                                                                                                                                                 |
|----------------------------------|--------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Object Key                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The key of the S3 object. If none is given the filename attribute will be used by default.<br/>**Supports Expression Language: true**                                                                                                                                                                       |
| **Bucket**                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The S3 bucket<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                    |
| Content Type                     |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Sets the Content-Type HTTP header indicating the type of content stored in the associated object. The value of this header is a standard MIME type. If no content type is provided the default content type "application/octet-stream" will be used.<br/>**Supports Expression Language: true**             |
| **Use Default Credentials**      | false                                      |                                                                                                                                                                                                                                                                                                                                                                                                             | If true, uses the Default Credential chain to obtain AWS credentials, including EC2 instance profiles or roles, environment variables, default user credentials, etc.                                                                                                                                       |
| Access Key                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | AWS account access key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                           |
| Secret Key                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | AWS account secret key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                           |
| Credentials File                 |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey                                                                                                                                                                                 |
| AWS Credentials Provider service |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.                                                                                                                                                                                                         |
| **Storage Class**                | Standard                                   | Standard<br/>ReducedRedundancy<br/>StandardIA<br/>OnezoneIA<br/>IntelligentTiering<br/>Glacier<br/>DeepArchive                                                                                                                                                                                                                                                                                              | AWS S3 Storage Class                                                                                                                                                                                                                                                                                        |
| **Region**                       | us-west-2                                  | af-south-1<br/>ap-east-1<br/>ap-northeast-1<br/>ap-northeast-2<br/>ap-northeast-3<br/>ap-south-1<br/>ap-southeast-1<br/>ap-southeast-2<br/>ca-central-1<br/>cn-north-1<br/>cn-northwest-1<br/>eu-central-1<br/>eu-north-1<br/>eu-south-1<br/>eu-west-1<br/>eu-west-2<br/>eu-west-3<br/>me-south-1<br/>sa-east-1<br/>us-east-1<br/>us-east-2<br/>us-gov-east-1<br/>us-gov-west-1<br/>us-west-1<br/>us-west-2 | AWS Region                                                                                                                                                                                                                                                                                                  |
| **Communications Timeout**       | 30 sec                                     |                                                                                                                                                                                                                                                                                                                                                                                                             | Sets the timeout of the communication between the AWS server and the client                                                                                                                                                                                                                                 |
| FullControl User List            |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have Full Control for an object.<br/>**Supports Expression Language: true**                                                                                                                                        |
| Read Permission User List        |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have Read Access for an object.<br/>**Supports Expression Language: true**                                                                                                                                         |
| Read ACL User List               |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have permissions to read the Access Control List for an object.<br/>**Supports Expression Language: true**                                                                                                         |
| Write ACL User List              |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have permissions to change the Access Control List for an object.<br/>**Supports Expression Language: true**                                                                                                       | |Amazon Canned ACL for an object. Allowed values: BucketOwnerFullControl, BucketOwnerRead, AuthenticatedRead, PublicReadWrite, PublicRead, Private, AwsExecRead; will be ignored if any other ACL/permission property is specified.<br/>**Supports Expression Language: true**|
| Endpoint Override URL            |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Endpoint URL to use instead of the AWS default including scheme, host, port, and path. The AWS libraries select an endpoint URL based on the AWS region, but this property overrides the selected endpoint URL, allowing use with other S3-compatible endpoints.<br/>**Supports Expression Language: true** |
| **Server Side Encryption**       | None                                       | None<br/>AES256<br/>aws_kms                                                                                                                                                                                                                                                                                                                                                                                 | Specifies the algorithm used for server side encryption.                                                                                                                                                                                                                                                    |
| Proxy Host                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Proxy host name or IP<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                            |
| Proxy Port                       |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | The port number of the proxy host<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                |
| Proxy Username                   |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Username to set when authenticating against proxy<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
| Proxy Password                   |                                            |                                                                                                                                                                                                                                                                                                                                                                                                             | Password to set when authenticating against proxy<br/>**Supports Expression Language: true**                                                                                                                                                                                                                |
### Relationships

| Name    | Description                                  |
|---------|----------------------------------------------|
| failure | FlowFiles are routed to failure relationship |
| success | FlowFiles are routed to success relationship |


## PutSFTP

### Description

Sends FlowFiles to an SFTP Server
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                           | Default Value | Allowable Values                                          | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
|--------------------------------|---------------|-----------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Batch Size**                 | 500           |                                                           | The maximum number of FlowFiles to send in a single connection                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| **Conflict Resolution**        | NONE          | FAIL<br>IGNORE<br>NONE<br>REJECT<br>RENAME<br>REPLACE<br> | Determines how to handle the problem of filename collisions                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| **Connection Timeout**         | 30 sec        |                                                           | Amount of time to wait before timing out while creating a connection                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| **Create Directory**           | false         |                                                           | Specifies whether or not the remote directory should be created if it does not exist.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| **Data Timeout**               | 30 sec        |                                                           | When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| Disable Directory Listing      | false         |                                                           | If set to 'true', directory listing is not performed prior to create missing directories. By default, this processor executes a directory listing command to see target directory existence before creating missing directories. However, there are situations that you might need to disable the directory listing such as the following. Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, then an error is returned because the directory already exists. |
| Dot Rename                     | true          |                                                           | If true, then the filename of the sent file is prepended with a "." and then renamed back to the original once the file is completely sent. Otherwise, there is no rename. This property is ignored if the Temporary Filename property is set.                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Host Key File                  |               |                                                           | If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| **Hostname**                   |               |                                                           | The fully qualified hostname or IP address of the remote system<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| Http Proxy Password            |               |                                                           | Http Proxy Password<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Http Proxy Username            |               |                                                           | Http Proxy Username<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| Last Modified Time             |               |                                                           | The lastModifiedTime to assign to the file after transferring it. If not set, the lastModifiedTime will not be changed. Format must be yyyy-MM-dd'T'HH:mm:ssZ. You may also use expression language such as ${file.lastModifiedTime}. If the value is invalid, the processor will not be invalid but will fail to change lastModifiedTime of the file.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                         |
| Password                       |               |                                                           | Password for the user account<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| Permissions                    |               |                                                           | The permissions to assign to the file after transferring it. Format must be either UNIX rwxrwxrwx with a - in place of denied permissions (e.g. rw-r--r--) or an octal number (e.g. 644). If not set, the permissions will not be changed. You may also use expression language such as ${file.permissions}. If the value is invalid, the processor will not be invalid but will fail to change permissions of the file.<br/>**Supports Expression Language: true**                                                                                                                                                                                                       |
| **Port**                       |               |                                                           | The port that the remote system is listening on for file transfers<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| Private Key Passphrase         |               |                                                           | Password for the private key<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| Private Key Path               |               |                                                           | The fully qualified path to the Private Key file<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Proxy Host                     |               |                                                           | The fully qualified hostname or IP address of the proxy server<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| Proxy Port                     |               |                                                           | The port of the proxy server<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| Proxy Type                     | DIRECT        | DIRECT<br>HTTP<br>SOCKS<br>                               | Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. Supported proxies: HTTP + AuthN, SOCKS + AuthN                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| Reject Zero-Byte Files         | true          |                                                           | Determines whether or not Zero-byte files should be rejected without attempting to transfer                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Remote Group                   |               |                                                           | Integer value representing the Group ID to set on the file after transferring it. If not set, the group will not be set. You may also use expression language such as ${file.group}. If the value is invalid, the processor will not be invalid but will fail to change the group of the file.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                 |
| Remote Owner                   |               |                                                           | Integer value representing the User ID to set on the file after transferring it. If not set, the owner will not be set. You may also use expression language such as ${file.owner}. If the value is invalid, the processor will not be invalid but will fail to change the owner of the file.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                  |
| Remote Path                    |               |                                                           | The path on the remote system from which to pull or push files<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| **Send Keep Alive On Timeout** | true          |                                                           | Indicates whether or not to send a single Keep Alive message when SSH socket times out                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| **Strict Host Key Checking**   | false         |                                                           | Indicates whether or not strict enforcement of hosts keys should be applied                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| Temporary Filename             |               |                                                           | If set, the filename of the sent file will be equal to the value specified during the transfer and after successful completion will be renamed to the original filename. If this value is set, the Dot Rename property is ignored.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                             |
| **Use Compression**            | false         |                                                           | Indicates whether or not ZLIB compression should be used when transferring files                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| **Username**                   |               |                                                           | Username<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
### Relationships

| Name    | Description                                                                                          |
|---------|------------------------------------------------------------------------------------------------------|
| failure | FlowFiles that failed to send to the remote system; failure is usually looped back to this processor |
| reject  | FlowFiles that were rejected by the destination system                                               |
| success | FlowFiles that are successfully sent will be routed to success                                       |


## PutSplunkHTTP

### Description
Sends the flow file contents to the specified [Splunk HTTP Event Collector](https://docs.splunk.com/Documentation/SplunkCloud/latest/Data/UsetheHTTPEventCollector) over HTTP or HTTPS.

#### Event parameters
The "Source", "Source Type", "Host" and "Index" properties are optional and will be set by Splunk if unspecified. If set,
the default values will be overwritten with the user specified ones. For more details about the Splunk API, please visit
[this documentation](https://docs.splunk.com/Documentation/Splunk/LATEST/RESTREF/RESTinput#services.2Fcollector.2Fraw)

#### Acknowledgment
HTTP Event Collector (HEC) in Splunk provides the possibility of index acknowledgement, which can be used to monitor
the indexing status of the individual events. PutSplunkHTTP supports this feature by enriching the outgoing flow file
with the necessary information, making it possible for a later processor to poll the status based on. The necessary
information for this is stored within flow file attributes "splunk.acknowledgement.id" and "splunk.responded.at".

#### Error information
For more refined processing, flow files are enriched with additional information if possible. The information is stored
in the flow file attribute "splunk.status.code" or "splunk.response.code", depending on the success of the processing.
The attribute "splunk.status.code" is always filled when the Splunk API call is executed and contains the HTTP status code
of the response. In case the flow file transferred into "failure" relationship, the "splunk.response.code" might be
also filled, based on the Splunk response code.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value | Allowable Values    | Description                                                                                                                                                                                     |
|----------------------------|---------------|---------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Hostname**               |               |                     | The ip address or hostname of the Splunk server.                                                                                                                                                |
| **Port**                   | 8088          |                     | The HTTP Event Collector HTTP Port Number.                                                                                                                                                      |
| **Token**                  |               | Splunk &lt;guid&gt; | HTTP Event Collector token starting with the string Splunk. For example `Splunk 1234578-abcd-1234-abcd-1234abcd`                                                                                |
| **Splunk Request Channel** |               | &lt;guid&gt;        | Identifier of the used request channel.                                                                                                                                                         |
| SSL Context Service        |               |                     | The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.                                                                                         |
| Source                     |               |                     | Basic field describing the source of the event. If unspecified, the event will use the default defined in splunk.                                                                               |
| SourceType                 |               |                     | Basic field describing the source type of the event. If unspecified, the event will use the default defined in splunk.                                                                          |
| Host                       |               |                     | Basic field describing the host of the event. If unspecified, the event will use the default defined in splunk.                                                                                 |
| Index                      |               |                     | Identifies the index where to send the event. If unspecified, the event will use the default defined in splunk.                                                                                 |
| Content Type               |               |                     | The media type of the event sent to Splunk. If not set, "mime.type" flow file attribute will be used. In case of neither of them is specified, this information will not be sent to the server. |

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

| Name                      | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                                                                                                                                                   |
|---------------------------|---------------|------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **DB Controller Service** |               |                  | Database Controller Service.                                                                                                                                                                                                                                                                                                                                                                  |
| SQL Statement             |               |                  | The SQL statement to execute. The statement can be empty, a constant value, or built from attributes using Expression Language. If this property is specified, it will be used regardless of the content of incoming flowfiles. If this property is empty, the content of the incoming flow file is expected to contain a valid SQL statement, to be issued by the processor to the database. |
### Relationships

| Name    | Description                                                              |
|---------|--------------------------------------------------------------------------|
| success | After a successful SQL update operation, the incoming FlowFile sent here |


## PutTCP

### Description
The PutTCP processor receives a FlowFile and transmits the FlowFile content over a TCP connection to the configured TCP server. By default, the FlowFiles are transmitted over the same TCP connection. To assist the TCP server with determining message boundaries, an optional "Outgoing Message Delimiter" string can be configured which is appended to the end of each FlowFiles content when it is transmitted over the TCP connection. An optional "Connection Per FlowFile" parameter can be specified to change the behaviour so that each FlowFiles content is transmitted over a single TCP connection which is closed after the FlowFile has been sent.

### Properties
In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                           | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                                                                                                                                                                        |
|--------------------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Hostname**                   | localhost     |                  | The ip address or hostname of the destination.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                          |
| **Port**                       |               |                  | The port or service on the destination.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                 |
| **Idle Connection Expiration** | 15 seconds    |                  | The amount of time a connection should be held open without being used before closing the connection. A value of 0 seconds will disable this feature.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                   |
| **Timeout**                    | 15 seconds    |                  | The timeout for connecting to and communicating with the destination.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                   |
| **Connection Per FlowFile**    | false         |                  | Specifies whether to send each FlowFile's content on an individual connection.                                                                                                                                                                                                                                                                                                                                     |
| Outgoing Message Delimiter     |               |                  | Specifies the delimiter to use when sending messages out over the same TCP stream. The delimiter is appended to each FlowFile message that is transmitted over the stream so that the receiver can determine when one message ends and the next message begins. Users should ensure that the FlowFile content does not contain the delimiter character to avoid errors.<br/>**Supports Expression Language: true** |
| SSL Context Service            |               |                  | The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be sent over a secure connection.                                                                                                                                                                                                                                                                          |
| Max Size of Socket Send Buffer |               |                  | The maximum size of the socket send buffer that should be used. This is a suggestion to the Operating System to indicate how big the socket buffer should be.                                                                                                                                                                                                                                                      |

### Properties
| Name    | Description                                                                |
|---------|----------------------------------------------------------------------------|
| success | FlowFiles that are sent to the destination are sent out this relationship. |
| failure | FlowFiles that encountered IO errors are sent out this relationship.       |


## PutUDP

### Description

The PutUDP processor receives a FlowFile and packages the FlowFile content into a single UDP datagram packet which is then transmitted to the configured UDP server.
The processor doesn't guarantee a successful transfer, even if the flow file is routed to the success relationship.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name     | Default Value | Allowable Values | Description                                                                               |
|----------|---------------|------------------|-------------------------------------------------------------------------------------------|
| **Host** | localhost     |                  | The ip address or hostname of the destination.<br/>**Supports Expression Language: true** |
| **Port** |               |                  | The port on the destination.<br/>**Supports Expression Language: true**                   |

### Relationships

| Name    | Description                                                                   |
|---------|-------------------------------------------------------------------------------|
| success | FlowFiles that are sent to the destination are sent out this relationship.    |
| failure | FlowFiles that encounter IO or network errors are sent out this relationship. |

## QueryDatabaseTable

### Description

Fetches all rows of a table, whose values in the specified Maximum-value Columns are larger than the previously-seen maxima. If that property is not provided, all rows are returned. The rows are grouped according to the value of Max Rows Per Flow File property and formatted as JSON.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value | Allowable Values     | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
|----------------------------|---------------|----------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Columns to Return          |               |                      | A comma-separated list of column names to be used in the query. If your database requires special treatment of the names (quoting, e.g.), each name should include such treatment. If no column names are supplied, all columns in the specified table will be returned. NOTE: It is important to use consistent column names for a given table for incremental fetch to work properly.                                                                      |
| **DB Controller Service**  |               |                      | Database Controller Service.                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| **Max Rows Per Flow File** | 0             |                      | The maximum number of result rows that will be included in a single FlowFile. This will allow you to break up very large result sets into multiple FlowFiles. If the value specified is zero, then all rows are returned in a single FlowFile.                                                                                                                                                                                                               |
| Maximum-value Columns      |               |                      | A comma-separated list of column names. The processor will keep track of the maximum value for each column that has been returned since the processor started running. Using multiple columns implies an order to the column list, and each column's values are expected to increase more slowly than the previous columns' values. Thus, using multiple columns implies a hierarchical structure of columns, which is usually used for partitioning tables. |
| **Output Format**          | JSON-Pretty   | JSON<br/>JSON-Pretty | Set the output format type.                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| **Table Name**             |               |                      | The name of the database table to be queried.                                                                                                                                                                                                                                                                                                                                                                                                                |
| Where Clause               |               |                      | A custom clause to be added in the WHERE condition when building SQL queries.                                                                                                                                                                                                                                                                                                                                                                                |
### Relationships

| Name    | Description                                              |
|---------|----------------------------------------------------------|
| success | Successfully created FlowFile from SQL query result set. |
### Dynamic Properties:

| Name                                | Value                                          | Description                                                                                                                                                                                                                                                                                 |
|-------------------------------------|------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| initial.maxvalue.<max_value_column> | Initial maximum value for the specified column | Specifies an initial max value for max value column(s). Properties should be added in the format `initial.maxvalue.<max_value_column>`. This value is only used the first time the table is accessed (when a Maximum Value Column is specified).<br/>**Supports Expression Language: true** |


## QuerySplunkIndexingStatus

### Description

This processor is responsible for polling Splunk server and determine if a Splunk event is acknowledged at the time of
execution. For more details about the HEC Index Acknowledgement please see
[this documentation.](https://docs.splunk.com/Documentation/Splunk/LATEST/Data/AboutHECIDXAck)

#### Input requirements
In order to work properly, the incoming flow files need to have the attributes "splunk.acknowledgement.id" and
"splunk.responded.at" filled properly. The flow file attribute "splunk.acknowledgement.id" should contain the "ackId"
which can be extracted from the response to the original Splunk put call. The flow file attribute "splunk.responded.at"
should contain the timestamp describing when the put call was answered by Splunk.
These required attributes are set by PutSplunkHTTP processor.

#### Undetermined relationship
Undetermined cases are normal in healthy environment as it is possible that minifi asks for indexing status before Splunk
finishes and acknowledges it. These cases are safe to retry, and it is suggested to loop "undetermined" relationship
back to the processor for later try. Flow files transferred into the "Undetermined" relationship are penalized.

#### Performance
Please keep Splunk channel limitations in mind: there are multiple configuration parameters in Splunk which might have direct
effect on the performance and behaviour of the QuerySplunkIndexingStatus processor. For example "max_number_of_acked_requests_pending_query"
and "max_number_of_acked_requests_pending_query_per_ack_channel" might limit the amount of ackIDs Splunk stores.

Also, it is suggested to execute the query in batches. The "Maximum Query Size" property might be used for fine tune
the maximum number of events the processor will query about in one API request. This serves as an upper limit for the
batch but the processor might execute the query with smaller number of undetermined events.


### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value | Allowable Values                   | Description                                                                                                                                                                                                                                                                           |
|----------------------------|---------------|------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Maximum Waiting Time**   | 1 hour        | &lt;duration&gt; &lt;time unit&gt; | The maximum time the processor tries to acquire acknowledgement confirmation for an index, from the point of registration.<br> After the given amount of time, the processor considers the index as not acknowledged and transfers the FlowFile to the _unacknowledged_ relationship. |
| **Maximum Query Size**     | 1000          | integers                           | The maximum number of acknowledgement identifiers the outgoing query contains in one batch.  It is recommended not to set it too low in order to reduce network communication.                                                                                                        |
| **Hostname**               |               |                                    | The ip address or hostname of the Splunk server.                                                                                                                                                                                                                                      |
| **Port**                   | 8088          |                                    | The HTTP Event Collector HTTP Port Number.                                                                                                                                                                                                                                            |
| **Token**                  |               | Splunk &lt;guid&gt;                | HTTP Event Collector token starting with the string Splunk. For example `Splunk 1234578-abcd-1234-abcd-1234abcd`                                                                                                                                                                      |
| **Splunk Request Channel** |               | &lt;guid&gt;                       | Identifier of the used request channel.                                                                                                                                                                                                                                               |
| SSL Context Service        |               |                                    | The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.                                                                                                                                                                               |

### Relationships

| Name           | Description                                                                                                                                                                                                                                                                                                                                                          |
|----------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| acknowledged   | A FlowFile is transferred to this relationship when the acknowledgement was successful.                                                                                                                                                                                                                                                                              |
| unacknowledged | A FlowFile is transferred to this relationship when the acknowledgement was not successful.<br>This can happen when the acknowledgement did not happened within the time period set for Maximum Waiting Time.<br>FlowFiles with acknowledgement id unknown for the Splunk server will be transferred to this relationship after the Maximum Waiting Time is reached. |
| undetermined   | A FlowFile is transferred to this relationship when the acknowledgement state is not determined.<br> This can happens when Splunk returns with HTTP 200 but with false response for the acknowledgement id in the flow file attribute.<br>FlowFiles transferred to this relationship might be penalized.<br>                                                         |
| failure        | A FlowFile is transferred to this relationship when the acknowledgement was not successful due to errors during the communication, or if the flowfile was missing the acknowledgement id                                                                                                                                                                             |


## ReplaceText

### Description
Updates the content of a FlowFile by replacing parts of it using various replacement strategies.

### Properties
In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                         | Default Value | Allowable Values                                                                                    | Description                                                                                                                                                                                                                                                                                                                                                                                                                                             |
|------------------------------|---------------|-----------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Evaluation Mode**          | Line-by-Line  | Entire text<br>Line-by-Line<br>                                                                     | Run the 'Replacement Strategy' against each line separately (Line-by-Line) or against the whole input treated as a single string (Entire Text).                                                                                                                                                                                                                                                                                                         |
| Line-by-Line Evaluation Mode | All           | All<br>Except-First-Line<br>Except-Last-Line<br>First-Line<br>Last-Line<br>                         | Run the 'Replacement Strategy' against each line separately (Line-by-Line) for All lines in the FlowFile, First Line (Header) only, Last Line (Footer) only, all Except the First Line (Header) or all Except the Last Line (Footer).                                                                                                                                                                                                                   |
| **Replacement Strategy**     | Regex Replace | Always Replace<br>Append<br>Literal Replace<br>Prepend<br>Regex Replace<br>Substitute Variables<br> | The strategy for how and what to replace within the FlowFile's text content. Substitute Variables replaces ${attribute_name} placeholders with the corresponding attribute's value (if an attribute is not found, the placeholder is kept as it was).                                                                                                                                                                                                   |
| **Replacement Value**        |               |                                                                                                     | The value to insert using the 'Replacement Strategy'. Using 'Regex Replace' back-references to Regular Expression capturing groups are supported: $& is the entire matched substring, $1, $2, ... are the matched capturing groups. Use $$1 for a literal $1. Back-references to non-existent capturing groups will be replaced by empty strings. Supports expression language except in Regex Replace mode.<br/>**Supports Expression Language: true** |
| Search Value                 |               |                                                                                                     | The Search Value to search for in the FlowFile content. Only used for 'Literal Replace' and 'Regex Replace' matching strategies. Supports expression language except in Regex Replace mode.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                  |

### Relationships
| Name    | Description                                                                                                                                                  |
|---------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
| failure | FlowFiles that could not be updated are routed to this relationship.                                                                                         |
| success | FlowFiles that have been successfully processed are routed to this relationship. This includes both FlowFiles that had text replaced and those that did not. |


## RetryFlowFile

### Description

FlowFiles passed to this Processor have a 'Retry Attribute' value checked against a configured 'Maximum Retries' value. If the current attribute value is below the configured maximum, the FlowFile is passed to a retry relationship. The FlowFile may or may not be penalized in that condition. If the FlowFile's attribute value exceeds the configured maximum, the FlowFile will be passed to a 'retries_exceeded' relationship. WARNING: If the incoming FlowFile has a non-numeric value in the configured 'Retry Attribute' attribute, it will be reset to '1'. You may choose to fail the FlowFile instead of performing the reset. Additional dynamic properties can be defined for any attributes you wish to add to the FlowFiles transferred to 'retries_exceeded'. These attributes support attribute expression language.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                            | Default Value      | Allowable Values                                           | Description                                                                                                                                                                                                                                                                                                                                                                                              |
|---------------------------------|--------------------|------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Retry Attribute                 | "flowfile.retries" |                                                            | The name of the attribute that contains the current retry count for the FlowFile. WARNING: If the name matches an attribute already on the FlowFile that does not contain a numerical value, the processor will either overwrite that attribute with '1' or fail based on configuration.<br/>**Supports Expression Language: true**                                                                      |
| Maximum Retries                 | 3                  |                                                            | The maximum number of times a FlowFile can be retried before being passed to the 'retries_exceeded' relationship.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                             |
| Penalize Retries                | true               |                                                            | "If set to 'true', this Processor will penalize input FlowFiles before passing them to the 'retry' relationship. This does not apply to the 'retries_exceeded' relationship.                                                                                                                                                                                                                             |
| Fail on Non-numerical Overwrite | false              |                                                            | If the FlowFile already has the attribute defined in 'Retry Attribute' that is *not* a number, fail the FlowFile instead of resetting that value to '1'.                                                                                                                                                                                                                                                 |
| Reuse Mode                      | "Fail on Reuse"    | "Fail on Reuse"<br/>"Warn on Reuse"<br/>"Reset Reuse"<br/> | Defines how the Processor behaves if the retry FlowFile has a different retry UUID than the instance that received the FlowFile. This generally means that the attribute was not reset after being successfully retried by a previous instance of this processor. Warn on reuse and Fail on Reuse both resets the retry property value to 1 and marks the flowfile to be last retried by this processor. |
### Relationships

| Name             | Description                                                                                                                                                                                                                                                                                                                    |
|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| retry            | Input FlowFile has not exceeded the configured maximum retry count, pass this relationship back to the input Processor to create a limited feedback loop.                                                                                                                                                                      |
| retries_exceeded | Input FlowFile has exceeded the configured maximum retry count, do not pass this relationship back to the input Processor to terminate the limited feedback loop.                                                                                                                                                              |
| failure          | The processor is configured such that a non-numerical value on 'Retry Attribute' results in a failure instead of resetting that value to '1'. This will immediately terminate the limited feedback loop. Might also include when 'Maximum Retries' contains attribute expression language that does not resolve to an Integer. |
### Dynamic Properties:

| Name | Value | Description |
| --- | --- | - |
|Exceeded FlowFile Attribute Key|The value of the attribute added to the FlowFile|One or more dynamic properties can be used to add attributes to FlowFiles passed to the 'retries_exceeded' relationship.<br/>**Supports Expression Language: true**|
### Writes Attributes:

| Name                  | Description                                                                                       |
|-----------------------|---------------------------------------------------------------------------------------------------|
| Retry Attribute       | User defined retry attribute is updated with the current retry count                              |
| Retry Attribute .uuid | 	User defined retry attribute with .uuid that determines what processor retried the FlowFile last |

## RouteOnAttribute

### Description

Routes FlowFiles based on their Attributes using the Attribute Expression Language.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
|------|---------------|------------------|-------------|
### Relationships

| Name      | Description                                             |
|-----------|---------------------------------------------------------|
| failure   | Failed files are transferred to failure                 |
| unmatched | Files which do not match any expression are routed here |

## RouteText

### Description
Routes textual data based on a set of user-defined rules. Each segment in an incoming FlowFile is compared against the values specified by user-defined Properties. The mechanism by which the text is compared to these user-defined properties is defined by the 'Matching Strategy'. The data is then routed according to these rules, routing each segment of the text individually.

### Properties
| Name                               | Default Value                                                                                             | Allowable Values                                | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
|------------------------------------|-----------------------------------------------------------------------------------------------------------|-------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Grouping Fallback Value            | empty string ("")                                                                                         |                                                 | If the 'Grouping Regular Expression' is specified and the matching fails, this value will be considered the group of the segment.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| Grouping Regular Expression        |                                                                                                           |                                                 | Specifies a Regular Expression to evaluate against each segment to determine which Group it should be placed in. The Regular Expression must have at least one Capturing Group that defines the segment's Group. If multiple Capturing Groups exist in the Regular Expression, the values from all Capturing Groups will be concatenated together. Two segments will not be placed into the same FlowFile unless they both have the same value for the Group (or neither matches the Regular Expression). For example, to group together all lines in a CSV File by the first column, we can set this value to `(.*?),.*` (and use \"Per Line\" segmentation). Two segments that have the same Group but different Relationships will never be placed into the same FlowFile. |
| Ignore Case                        | false                                                                                                     |                                                 | If true, capitalization will not be taken into account when comparing values. E.g., matching against 'HELLO' or 'hello' will have the same result. This property is ignored if the 'Matching Strategy' is set to 'Satisfies Expression'.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| Ignore Leading/Trailing Whitespace | true                                                                                                      |                                                 | Indicates whether or not the whitespace at the beginning and end should be ignored when evaluating a segment.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| Matching Strategy                  | Starts With<br>Ends With<br>Contains<br>Equals<br>Matches Regex<br>Contains Regex<br>Satisfies Expression |                                                 | Specifies how to evaluate each segment of incoming text against the user-defined properties.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| Routing Strategy                   | Dynamic Routing                                                                                           | Dynamic Routing<br>Route On All<br>Route On Any | Specifies how to determine which Relationship(s) to use when evaluating the segments of incoming text against the 'Matching Strategy' and user-defined properties. 'Dynamic Routing' routes to all the matching dynamic relationships (or 'unmatched' if none matches). 'Route On All' routes to 'matched' iff all dynamic relationships match. 'Route On Any' routes to 'matched' iff any of the dynamic relationships match.                                                                                                                                                                                                                                                                                                                                                |
| Segmentation Strategy              | Per Line                                                                                                  | Per Line<br>Full Text                           | Specifies what portions of the FlowFile content constitutes a single segment to be processed.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |

### Dynamic Properties

| Name | Value | Description |
| --- | --- | - |
|Relationship Name|value to match against|Routes data that matches the value specified in the Dynamic Property Value to the Relationship specified in the Dynamic Property Key.<br>**Supports Expression Language: true**|

### Relationships

| Name      | Description                                                                                      |
|-----------|--------------------------------------------------------------------------------------------------|
| matched   | Segments that satisfy the required user-defined rules will be routed to this Relationship        |
| unmatched | Segments that do not satisfy the required user-defined rules will be routed to this Relationship |
| original  | The original input file will be routed to this destination                                       |

### Writes Attributes

| Name            | Description                                                                                                                                              |
|-----------------|----------------------------------------------------------------------------------------------------------------------------------------------------------|
| RouteText.Group | The value captured by all capturing groups in the 'Grouping Regular Expression' property. If this property is not set, this attribute will not be added. |

## TailEventLog

### Description

Windows event log reader that functions as a stateful tail of the provided Windows event log name
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


## TailFile

### Description

"Tails" a file, or a list of files, ingesting data from the file as it is written to the file. The file is expected to be textual. Data is ingested only when a new line is encountered (carriage return or new-line character or combination). If the file to tail is periodically "rolled over", as is generally the case with log files, an optional Rolling Filename Pattern can be used to retrieve data from files that have rolled over, even if the rollover occurred while NiFi was not running (provided that the data still exists upon restart of NiFi). It is generally advisable to set the Run Schedule to a few seconds, rather than running with the default value of 0 secs, as this Processor will consume a lot of resources if scheduled very aggressively. At this time, this Processor does not support ingesting files that have been compressed when 'rolled over'.
### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value     | Allowable Values                                       | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
|----------------------------|-------------------|--------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Attribute Provider Service |                   |                                                        | Provides a list of key-value pair records which can be used in the Base Directory property using Expression Language. Requires Multiple file mode.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| File to Tail               |                   |                                                        | Fully-qualified filename of the file that should be tailed when using single file mode, or a file regex when using multifile mode                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| **Initial Start Position** | Beginning of File | Beginning of Time<br>Beginning of File<br>Current Time | When the Processor first begins to tail data, this property specifies where the Processor should begin reading data. Once data has been ingested from a file, the Processor will continue from the last point from which it has received data.<br>Beginning of Time: Start with the oldest data that matches the Rolling Filename Pattern and then begin reading from the File to Tail.<br>Beginning of File: Start with the beginning of the File to Tail. Do not ingest any data that has already been rolled over.<br>Current Time: Start with the data at the end of the File to Tail. Do not ingest any data that has already been rolled over or any data in the File to Tail that has already been written. |
| Input Delimiter            |                   |                                                        | Specifies the character that should be used for delimiting the data being tailedfrom the incoming file.If none is specified, data will be ingested as it becomes available.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| State File                 | TailFileState     |                                                        | Specifies the file that should be used for storing state about what data has been ingested so that upon restart NiFi can resume from where it left off                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| tail-base-directory        |                   |                                                        | Base directory used to look for files to tail. This property is required when using Multiple file mode. Can contain expression language placeholders if Attribute Provider Service is set.<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| **tail-mode**              | Single file       | Single file<br>Multiple file<br>                       | Specifies the tail file mode. In 'Single file' mode only a single file will be watched. In 'Multiple file' mode a regex may be used. Note that in multiple file mode we will still continue to watch for rollover on the initial set of watched files. The Regex used to locate multiple files will be run during the schedule phrase. Note that if rotated files are matched by the regex, those files will be tailed.                                                                                                                                                                                                                                                                                            |
| **Batch Size**             | 0                 |                                                        | Maximum number of flowfiles emitted in a single trigger. If set to 0 all new content will be processed.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
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
| failure | Failed files are transferred to failure |
| success | All files are routed to success         |
