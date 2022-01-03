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

The following examples show simple flow configurations for several common use cases. Use the path of these yaml files as the flow configuration set in the `nifi.flow.configuration.file` property of `conf/minifi.properties` file or replace the default `conf/config.yml` file to try them out.

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
- [SQL Operations](#sql-operations)
  - [Query Database Table](#query-database-table)
- [ExecuteScript](#ExecuteScript)
  - [Reverse flowfile's content with lua](#reverse-content-with-lua)
  - [Reverse flowfile's content with python](#reverse-content-with-python)

## Filesystem Operations

### Getting File Data and Putting It in an Output Directory

Using the [getfile_putfile_config.yml](getfile_putfile_config.yml) flow configuration MiNiFi gets all files of minimum 1MB size from the `/tmp/getfile_dir` directory and puts them in the `/tmp/out_dir` output directory.

The flow: GetFile &#10132; success &#10132; PutFile

### Tailing a Single File

Using the [tailfile_config.yml](tailfile_config.yml) flow configuration MiNiFi tails a single file `/tmp/test_file.log` and creates flowfiles from every single line, then logs the flowfile attributes.

The flow: TailFile &#10132; success &#10132; LogAttribute

## Windows Specific Processors

### Consuming Windows Event Logs

Using the [cwel_config.yml](cwel_config.yml) flow configuration MiNiFi queries all Windows system events and puts them to the `C:\temp\` directory in flattened JSON format.

The flow: ConsumeWindowsEventLog &#10132; PutFile

### Reading System Performance Data

Using the [pdh_config.yml](pdh_config.yml) flow configuration MiNiFi reads CPU and Disk performance data through Windows' Performance Data Helper (PDH) component and puts the data to the `C:\temp\` directory in a compact JSON format.

The flow: PerformanceDataMonitor &#10132; PutFile

## Linux Specific Processors

### Consume Systemd-Journald System Journal Messages

Using the [consumejournald_config.yml](consumejournald_config.yml) flow configuration MiNiFi reads systemd-journald journal messages and logs them on `info` level.

The flow: ConsumeJournald &#10132; LogAttribute

## HTTP Operations

### HTTP POST Invocation

Using the [http_post_config.yml](http_post_config.yml) flow configuration MiNiFi transfers flowfile data received from the GetFile processor by invoking an HTTP endpoint with POST method.

The flow: GetFile &#10132; success &#10132; InvokeHTTP

## Site to Site Operations

### Transfer Data to Remote Nifi Instance

Using the [site_to_site_config.yml](site_to_site_config.yml) flow configuration MiNiFi transfers data received from the GetFile processor to a remote NiFi instance located at `http://nifi:8080/nifi`.

### [Site-2-Site Bi-directional Configuration](BidirectionalSiteToSite/README.md)

## Kafka Operations

### Publish Message to Kafka Broker

Using the [publishkafka_config.yml](publishkafka_config.yml) flow configuration MiNiFi publishes data received from the GetFile processor to a configured Kafka broker's `test` topic.

The flow: GetFile &#10132; success &#10132; PublishKafka

### Publish Message to Kafka Broker Through SSL

Using the [publishkafka_ssl_config.yml](publishkafka_ssl_config.yml) flow configuration MiNiFi publishes data received from the GetFile processor to a configured Kafka broker's `test` topic through SSL connection.

The flow: GetFile &#10132; success &#10132; PublishKafka

### Consume Messages from Kafka

Using the [consumekafka_config.yml](consumekafka_config.yml) flow configuration MiNiFi consumes messages from the configured Kafka broker's `ConsumeKafkaTest` topic from the earliest available message. The messages are forwarded to the `PutFile` processor and put in the `/tmp/output` directory.

The flow: ConsumeKafka &#10132; success &#10132; PutFile

## Public Cloud Operations

### Upload Blob to Azure Storage

Using the [azure_storage_config.yml](azure_storage_config.yml) flow configuration MiNiFi uploads data received from the GetFile processor to Azure's blob storage container `test-container`.

The flow: GetFile &#10132; success &#10132; PutAzureBlobStorage

### Put Object in AWS S3 Bucket

Using the [puts3_config.yml](puts3_config.yml) flow configuration MiNiFi uploads data received from the GetFile processor to AWS S3 bucket `test_bucket`.

The flow: GetFile &#10132; success &#10132; PutS3Object

### List and Fetch Content from AWS S3 Bucket

Using the [lists3_fetchs3_config.yml](lists3_fetchs3_config.yml) flow configuration MiNiFi lists S3 bucket `test_bucket` and fetches its contents in flowfiles then logs the attributes. The flow uses `AWSCredentialsService` controller service to provide credentials for all S3 processors. It has `Use Default Credentials` property set which retrieves credentials from AWS default credentials provider chain (environment variables, configuration file, instance profile).

The flow: ListS3 &#10132; FetchS3Object &#10132; LogAttribute

## SQL Operations

### Query Database Table

Using the [querydbtable_config.yml](querydbtable_config.yml) flow configuration MiNiFi queries the `id` and `name` columns of the `users` table with a `where` clause and the results are put in the `/tmp/output` directory. The database connection data is set in the `ODBCService` controller service.

The flow: QueryDatabaseTable &#10132; PutFile

## ExecuteScript

ExecuteScript supports [Lua](https://www.lua.org/) and [Python](https://www.python.org/)

### Reverse Content with Scripts

Using the [process_data_with_scripts.yml](process_data_with_scripts.yml) flow configuration MiNiFi generates a flowfile
then reverses its content with [reverse_flow_file_content.py](scripts/python/reverse_flow_file_content.py) or [reverse_flow_file_content.lua](scripts/lua/reverse_flow_file_content.lua) 
then writes the result to _./reversed_flow_files/_

The flow: [GenerateFlowFile](../PROCESSORS.md#generateflowfile) &#10132; [ExecuteScript](../PROCESSORS.md#executescript) &#10132;  [PutFile](../PROCESSORS.md#putfile) 

Additional script examples can be found [here](scripts/README.md).
