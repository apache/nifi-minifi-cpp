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

## Filesystem Operations

### Getting files from an input directory and putting the data in an output directory

Using the [getfile_putfile_config.yml](getfile_putfile_config.yml) flow configuration MiNiFi gets all files of minimum 1MB size from the `/tmp/getfile_dir` directory and puts them in the `/tmp/out_dir` output directory.

The flow: GetFile &#10132; success &#10132; PutFile

### Tailing a single file

Using the [tailfile_config.yml](tailfile_config.yml) flow configuration MiNiFi tails a single file `/tmp/test_file.log` and creates flowfiles from every single line, then logs the flowfile attributes.

The flow: TailFile &#10132; success &#10132; LogAttribute

## HTTP operations

### HTTP POST invocation

Using the [http_post_config.yml](http_post_config.yml) flow configuration MiNiFi transfers flowfile data received from the GetFile processor by invoking an HTTP endpoint with POST method.

The flow: GetFile &#10132; success &#10132; InvokeHTTP

## Kafka operations

### Publish message to Kafka broker

Using the [publishkafka_config.yml](publishkafka_config.yml) flow configuration MiNiFi publishes data received from the GetFile processor to a configured Kafka broker's `test` topic.

The flow: GetFile &#10132; success &#10132; PublishKafka

### Publish message to Kafka broker through SSL

Using the [publishkafka_ssl_config.yml](publishkafka_ssl_config.yml) flow configuration MiNiFi can accomplish the same use case as [publishkafka_config.yml](publishkafka_config.yml) but through SSL security protocol.

The flow: GetFile &#10132; success &#10132; PublishKafka

### Consume messages from Kafka topic

Using the [consumekafka_config.yml](consumekafka_config.yml) flow configuration MiNiFi consumes messages from the configured Kafka broker's `ConsumeKafkaTest` topic from the earliest available message. The messages are forwarded to the `PutFile` processor and put in the `/tmp/output` directory

The flow: ConsumeKafka &#10132; success &#10132; PutFile

## Public Cloud Operations

### Upload blob to Azure storage

Using the [azure_storage_config.yml](azure_storage_config.yml) flow configuration MiNiFi uploads data received from the GetFile processor to Azure's blob storage container `test-container`.

The flow: GetFile &#10132; success &#10132; PutAzureBlobStorage

### Put object in AWS S3 bucket

Using the [puts3_config.yml](puts3_config.yml) flow configuration MiNiFi uploads data received from the GetFile processor to AWS S3 bucket `test_bucket`.

The flow: GetFile &#10132; success &#10132; PutS3Object

### List and fetch content from AWS S3 bucket

Using the [lists3_fetchs3_config.yml](lists3_fetchs3_config.yml) flow configuration MiNiFi lists S3 bucket `test_bucket` and fetches its contents in flowfiles then logs the attributes. The flow uses `AWSCredentialsService` controller service to provide credentials for all S3 processors. It has `Use Default Credentials` property set which retrieves credentials from AWS default credentials provider chain (environment variables, configuration file, instance profile).

The flow: ListS3 &#10132; FetchS3Object &#10132; LogAttribute

## Site to Site Operations

### Transfer data to remote Nifi instance

Using the [site_to_site_config.yml](site_to_site_config.yml) flow configuration MiNiFi transfers data received from the GetFile processor to a remote NiFi instance located at `http://nifi:8080/nifi`.

### [Site-2-Site Bi-directional Configuration](BidirectionalSiteToSite/README.md)

## Windows Specific Processors

### Consuming Windows event logs

Using the [cwel_config.yml](cwel_config.yml) flow configuration MiNiFi queries all Windows system events and puts them to the `C:\temp\` directory in flattened JSON format.

The flow: ConsumeWindowsEventLog &#10132; PutFile

### Reading system performance data

Using the [pdh_config.yml](pdh_config.yml) flow configuration MiNiFi reads CPU and Disk performance data through Windows' Performance Data Helper (PDH) component and puts the data to the `C:\temp\` directory in a compact JSON format.
