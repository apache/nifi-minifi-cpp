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
# Apache NiFi - MiNiFi - C++ Examples

The following examples show simple flow configurations for several common use cases. Apache MiNiFi C++ supports flow configurations using yaml and json formats. Use the path of these json and yaml files as the flow configuration set in the `nifi.flow.configuration.file` property of `conf/minifi.properties` file or replace the default `conf/config.yml` file to try them out.

For the json flow configuration format there are two supported schemas:
 - The default schema mimics the yaml configuration format using json syntax and json naming conventions. See the examples without the `nifi.schema.json` suffix.
 - The NiFi schema mimics NiFi's json flow configuration format, having some additional json properties added to the default schema from NiFi's json flow configuration format. See the examples with the `nifi.schema.json` suffix.

The json schema can be generated using the MiNiFi C++ binary with the `--schema` option. The generated schema can be used to validate the json flow configuration files.

- [Filesystem Operations](#filesystem-operations)
  - [Getting File Data and Putting It in an Output Directory](#getting-file-data-and-putting-it-in-an-output-directory)
  - [Tailing a Single File](#tailing-a-single-file)
- [Windows Specific Processors](#windows-specific-processors)
  - [Consuming Windows Event Logs](#consuming-windows-event-logs)
  - [Reading System Performance Data](#reading-system-performance-data)
- [Linux Specific Processors](#linux-specific-processors)
  - [Consume Systemd-Journald System Journal Messages](#consume-systemd-journald-system-journal-messages)
- [HTTP Operations](#http-operations)
  - [HTTP POST Invocation](#http-post-invocation)
- [Site to Site Operations](#site-to-site-operations)
  - [Transfer Data to Remote Nifi Instance](#transfer-data-to-remote-nifi-instance)
  - [Site-2-Site Bi-directional Configuration](#site-2-site-bi-directional-configuration)
- [Kafka Operations](#kafka-operations)
  - [Publish Message to Kafka Broker](#publish-message-to-kafka-broker)
  - [Publish Message to Kafka Broker Through SSL](#publish-message-to-kafka-broker-through-ssl)
  - [Consume Messages from Kafka](#consume-messages-from-kafka)
- [Public Cloud Operations](#public-cloud-operations)
  - [Upload Blob to Azure Storage](#upload-blob-to-azure-storage)
  - [Put Object in AWS S3 Bucket](#put-object-in-aws-s3-bucket)
  - [List and Fetch Content from AWS S3 Bucket](#list-and-fetch-content-from-aws-s3-bucket)
  - [Merge, compress and upload files to Google Cloud Storage](#merge-compress-and-upload-files-to-google-cloud-storage)
- [SQL Operations](#sql-operations)
  - [Query Database Table](#query-database-table)
- [ExecuteScript](#executescript)
  - [Reverse Content with Scripts](#reverse-content-with-scripts)
- [Grafana Loki](#grafana-loki)
  - [Send logs to Grafana Loki](#send-logs-to-grafana-loki)
- [MQTT Operations](#mqtt-operations)
  - [Publish Message to MQTT Broker](#publish-message-to-mqtt-broker)
- [Network Operations](#network-operations)
  - [Send Data to Remote Host through TCP](#send-data-to-remote-host-through-tcp)

## Filesystem Operations

### Getting File Data and Putting It in an Output Directory

Using the [getfile_putfile_config.yml](getfile_putfile_config.yml)/[getfile_putfile_config.json](getfile_putfile_config.json) flow configuration MiNiFi gets all files of minimum 1MB size from the `/tmp/getfile_dir` directory and puts them in the `/tmp/out_dir` output directory.

The flow: [GetFile](../PROCESSORS.md#getfile) &#10132; [PutFile](../PROCESSORS.md#putfile)

### Tailing a Single File

Using the [tailfile_config.yml](tailfile_config.yml)/[tailfile_config.json](tailfile_config.json) flow configuration MiNiFi tails a single file `/tmp/test_file.log` and creates flowfiles from every single line, then logs the flowfile attributes.

The flow: [TailFile](../PROCESSORS.md#tailfile) &#10132; [LogAttribute](../PROCESSORS.md#logattribute)

## Windows Specific Processors

### Consuming Windows Event Logs

Using the [cwel_config.yml](cwel_config.yml)/[cwel_config.json](cwel_config.json) flow configuration MiNiFi queries all Windows system events and puts them to the `C:\temp\` directory in flattened JSON format.

The flow: [ConsumeWindowsEventLog](../PROCESSORS.md#consumewindowseventlog) &#10132; [PutFile](../PROCESSORS.md#putfile)

### Reading System Performance Data

Using the [pdh_config.yml](pdh_config.yml)/[pdh_config.json](pdh_config.json) flow configuration MiNiFi reads CPU and Disk performance data through Windows' Performance Data Helper (PDH) component and puts the data to the `C:\temp\` directory in a compact JSON format.

The flow: [PerformanceDataMonitor](../PROCESSORS.md#performancedatamonitor) &#10132; [PutFile](../PROCESSORS.md#putfile)

## Linux Specific Processors

### Consume Systemd-Journald System Journal Messages

Using the [consumejournald_config.yml](consumejournald_config.yml)/[consumejournald_config.json](consumejournald_config.json)/[consumejournald_config.nifi.schema.json](consumejournald_config.nifi.schema.json) flow configuration MiNiFi reads systemd-journald journal messages and logs them on `info` level.

The flow: [ConsumeJournald](../PROCESSORS.md#consumejournald) &#10132; [LogAttribute](../PROCESSORS.md#logattribute)

## HTTP Operations

### HTTP POST Invocation

Using the [http_poste_config.yml](http_post_config.yml)/[http_post_config.json](http_post_config.json)/[http_post_config.nifi.schema.json](http_post_config.nifi.schema.json) flow configuration MiNiFi transfers flowfile data received from the GetFile processor by invoking an HTTP endpoint with POST method.

The flow: [GetFile](../PROCESSORS.md#getfile) &#10132; [InvokeHTTP](../PROCESSORS.md#invokehttp)

## Site to Site Operations

### Transfer Data to Remote Nifi Instance

Using the [site_to_site_config.yml](site_to_site_config.yml)/[site_to_site_config.json](site_to_site_config.json)/[site_to_site_config.nifi.schema.json](site_to_site_config.nifi.schema.json) flow configuration MiNiFi transfers data received from the GetFile processor to a remote NiFi instance located at `http://nifi:8080/nifi`.

### [Site-2-Site Bi-directional Configuration](BidirectionalSiteToSite/README.md)

## Kafka Operations

### Publish Message to Kafka Broker

Using the [publishkafka_config.yml](publishkafka_config.yml)/[publishkafka_config.json](publishkafka_config.json) flow configuration MiNiFi publishes data received from the GetFile processor to a configured Kafka broker's `test` topic.

The flow: [GetFile](../PROCESSORS.md#getfile) &#10132; [PublishKafka](../PROCESSORS.md#publishkafka)

### Publish Message to Kafka Broker Through SSL

Using the [publishkafka_ssl_config.yml](publishkafka_ssl_config.yml)/[publishkafka_ssl_config.json](publishkafka_ssl_config.json) flow configuration MiNiFi publishes data received from the GetFile processor to a configured Kafka broker's `test` topic through SSL connection.

The flow: [GetFile](../PROCESSORS.md#getfile) &#10132; [PublishKafka](../PROCESSORS.md#publishkafka)

### Consume Messages from Kafka

Using the [consumekafka_config.yml](consumekafka_config.yml)/[consumekafka_config.json](consumekafka_config.json) flow configuration MiNiFi consumes messages from the configured Kafka broker's `ConsumeKafkaTest` topic from the earliest available message. The messages are forwarded to the `PutFile` processor and put in the `/tmp/output` directory.

The flow: [ConsumeKafka](../PROCESSORS.md#consumekafka) &#10132; [PutFile](../PROCESSORS.md#putfile)

## Public Cloud Operations

### Upload Blob to Azure Storage

Using the [azure_storage_config.yml](azure_storage_config.yml)/[azure_storage_config.json](azure_storage_config.json)/[azure_storage_config.nifi.schema.json](azure_storage_config.nifi.schema.json) flow configuration MiNiFi uploads data received from the GetFile processor to Azure's blob storage container `test-container`.

The flow: [GetFile](../PROCESSORS.md#getfile) &#10132; [PutAzureBlobStorage](../PROCESSORS.md#putAzureblobstorage)

### Put Object in AWS S3 Bucket

Using the [puts3_config.yml](puts3_config.yml)/[puts3_config.json](puts3_config.json) flow configuration MiNiFi uploads data received from the GetFile processor to AWS S3 bucket `test_bucket`.

The flow: [GetFile](../PROCESSORS.md#getfile) &#10132; [PutS3Object](../PROCESSORS.md#puts3object)

### List and Fetch Content from AWS S3 Bucket

Using the [lists3_fetchs3_config.yml](lists3_fetchs3_config.yml)/[lists3_fetchs3_config.json](lists3_fetchs3_config.json)/[lists3_fetchs3_config.nifi.schema.json](lists3_fetchs3_config.nifi.schema.json) flow configuration MiNiFi lists S3 bucket `test_bucket` and fetches its contents in flowfiles then logs the attributes. The flow uses `AWSCredentialsService` controller service to provide credentials for all S3 processors. It has `Use Default Credentials` property set which retrieves credentials from AWS default credentials provider chain (environment variables, configuration file, instance profile).

The flow: [ListS3](../PROCESSORS.md#lists3) &#10132; [FetchS3Object](../PROCESSORS.md#fetchs3object) &#10132; [LogAttribute](../PROCESSORS.md#logattribute)

### Merge, compress and upload files to Google Cloud Storage

Using the [merge_compress_and_upload_to_gcs_config.yml](merge_compress_and_upload_to_gcs_config.yml)/[merge_compress_and_upload_to_gcs_config.json](merge_compress_and_upload_to_gcs_config.json) flow configuration MiNiFi tails a file, creates a flow file with an added `google_cloud_storage` attrbute from every new line, then merges every 10 lines, compresses the merged content in gzip format, finally uploads it to Google Cloud Storage. The flow uses `GCSStorageService` controller service to provide credentials for all GCS processors.

The flow: [TailFile](../PROCESSORS.md#tailfile) &#10132; [UpdateAttribute](../PROCESSORS.md#updateattribute) &#10132; [MergeContent](../PROCESSORS.md#mergecontent) &#10132; [CompressContent](../PROCESSORS.md#compresscontent) &#10132; [PutGCSObject](../PROCESSORS.md#putgcsobject)

## SQL Operations

### Query Database Table

Using the [querydbtable_config.yml](querydbtable_config.yml)/[querydbtable_config.json](querydbtable_config.json)/[querydbtable_config.nifi.schema.json](querydbtable_config.nifi.schema.json) flow configuration MiNiFi queries the `id` and `name` columns of the `users` table with a `where` clause and the results are put in the `/tmp/output` directory. The database connection data is set in the `ODBCService` controller service.

The flow: [QueryDatabaseTable](../PROCESSORS.md#querydatabasetable) &#10132; [PutFile](../PROCESSORS.md#putfile)

## ExecuteScript

ExecuteScript supports [Lua](https://www.lua.org/) and [Python](https://www.python.org/)

### Reverse Content with Scripts

Using the [process_data_with_scripts.yml](process_data_with_scripts.yml)/[process_data_with_scripts.json](process_data_with_scripts.json) flow configuration MiNiFi generates a flowfile
then reverses its content with [reverse_flow_file_content.py](scripts/python/reverse_flow_file_content.py) or [reverse_flow_file_content.lua](scripts/lua/reverse_flow_file_content.lua)
then writes the result to _./reversed_flow_files/_

The flow: [GenerateFlowFile](../PROCESSORS.md#generateflowfile) &#10132; [ExecuteScript](../PROCESSORS.md#executescript) &#10132;  [PutFile](../PROCESSORS.md#putfile)

Additional script examples can be found [here](scripts/README.md).

## Grafana Loki

### Send logs to Grafana Loki

Using the [grafana_loki_config.yml](grafana_loki_config.yml)/[grafana_loki_config.json](grafana_loki_config.json) flow configuration MiNiFi tails a log file and sends the log lines to the Grafana Loki server configured on the localhost on the port 3100, with the job=minifi and id=logs labels using Grafana Loki's REST API.

The flow: [TailFile](../PROCESSORS.md#tailfile) &#10132; [PushGrafanaLokiREST](../PROCESSORS.md#pushgrafanalokirest)

## MQTT Operations

### Publish Message to MQTT Broker

Using the [mqtt_config.yml](mqtt_config.yml)/[mqtt_config.json](mqtt_config.json) flow configuration MiNiFi publishes the data from the files located under the `/tmp/input` directory to the `testtopic` topic of the MQTT broker configured on localhost on the port 1883.

The flow: [GetFile](../PROCESSORS.md#getfile) &#10132; [PublishMQTT](../PROCESSORS.md#publishmqtt)

## Network Operations

### Send Data to Remote Host through TCP

Using the [splittext_puttcp_config.yml](splittext_puttcp_config.yml)/[splittext_puttcp_config.json](splittext_puttcp_config.json) flow configuration MiNiFi splits the content of the files located under the `/tmp/input` directory into separate lines, while skipping the first 3 header lines, and sends them to the remote address 192.168.1.5 on the port 8081 through TCP.

The flow: [GetFile](../PROCESSORS.md#getfile) &#10132; [SplitText](../PROCESSORS.md#splittext) &#10132; [PutTCP](../PROCESSORS.md#puttcp)
