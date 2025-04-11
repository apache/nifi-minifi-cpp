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

# Apache NiFi - MiNiFi - C++ Metrics Readme.


This readme defines the metrics published by Apache NiFi. All options defined are located in minifi.properties.

## Table of Contents

- [Apache NiFi - MiNiFi - C++ Metrics Readme.](#apache-nifi---minifi---c-metrics-readme)
  - [Table of Contents](#table-of-contents)
  - [Description](#description)
  - [Configuration](#configuration)
  - [System Metrics](#system-metrics)
    - [QueueMetrics](#queuemetrics)
    - [RepositoryMetrics](#repositorymetrics)
    - [DeviceInfoNode](#deviceinfonode)
    - [FlowInformation](#flowinformation)
  - [Processor Metrics](#processor-metrics)
    - [General Metrics](#general-metrics)
    - [GetFileMetrics](#getfilemetrics)
    - [RunLlamaCppInferenceMetrics](#runllamacppinferencemetrics)

## Description

Apache NiFi MiNiFi C++ can communicate metrics about the agent's status, that can be a system level or component level metric.
These metrics are exposed through the agent implemented metric publishers that can be configured in the minifi.properties.
Aside from the publisher exposed metrics, metrics are also sent through C2 protocol of which there is more information in the
[C2 documentation](C2.md#metrics).

## Configuration

Currently LogMetricsPublisher and PrometheusMetricsPublisher are available that can be configured as metrics publishers. C2 metrics are published through C2 specific properties, see [C2 documentation](C2.md) for more information on that.

The LogMetricsPublisher serializes all the configured metrics into a json output and writes the json to the MiNiFi logs periodically. LogMetricsPublisher follows the conventions of the C2 metrics, and all information that is present in those metrics, including string data, is present in the log metrics as well. An example log entry may look like the following:

    [2023-03-09 15:04:32.268] [org::apache::nifi::minifi::state::LogMetricsPublisher] [info] {"LogMetrics":{"RepositoryMetrics":{"flowfile":{"running":"true","full":"false","size":"0"},"provenance":{"running":"true","full":"false","size":"0"}}}}

PrometheusMetricsPublisher publishes only numerical metrics to a Prometheus server in Prometheus specific format. This is different from the json format of the C2 and LogMetricsPublisher.

### Common configuration properties

To configure the a publisher first we have to specify the class in the properties. One or multiple publisher can be defined in comma separated format:

    # in minifi.properties

    nifi.metrics.publisher.class=LogMetricsPublisher

    # alternatively

    nifi.metrics.publisher.class=LogMetricsPublisher,PrometheusMetricsPublisher

To define which metrics should be published either the generic or the publisher specific metrics property should be used. The generic metrics are applied to all publishers if no publisher specific metric is specified.

    # in minifi.properties

    # define generic metrics for all selected publisher classes

    nifi.metrics.publisher.metrics=QueueMetrics,RepositoryMetrics,GetFileMetrics,DeviceInfoNode,FlowInformation,processorMetrics/Tail.*

    # alternatively LogMetricsPublisher will only use the following metrics

    nifi.metrics.publisher.LogMetricsPublisher.metrics=QueueMetrics,RepositoryMetrics

Additional configuration properties may be required by specific publishers, these are listed below.

### LogMetricsPublisher

LogMetricsPublisher requires a logging interval to be configured which states how often the selected metrics should be logged

    # in minifi.properties

    # log the metrics in MiNiFi app logs every 30 seconds

    nifi.metrics.publisher.LogMetricsPublisher.logging.interval=30s

Optionally LogMetricsPublisher can be configured which log level should the publisher use. The default log level is INFO

    # in minifi.properties

    # change log level to debug

    nifi.metrics.publisher.LogMetricsPublisher.log.level=DEBUG

### PrometheusMetricsPublisher

PrometheusMetricsPublisher requires a port to be configured where the metrics will be available to be scraped from:

    # in minifi.properties

    nifi.metrics.publisher.PrometheusMetricsPublisher.port=9936

An agent identifier should also be defined to identify which agent the metric is exposed from. If not set, the hostname is used as the identifier.

    # in minifi.properties

    nifi.metrics.publisher.agent.identifier=Agent1

### Configure Prometheus metrics publisher with SSL

The communication between MiNiFi and Prometheus can be encrypted using SSL. This can be achieved by adding the SSL certificate path (a single file containing both the MiNiFi certificate and the MiNiFi SSL key) and optionally adding the root CA path if Prometheus uses a self-signed certificate, to the minifi.properties file. Here is an example with the SSL properties:

    # in minifi.properties

    nifi.metrics.publisher.PrometheusMetricsPublisher.certificate=/tmp/certs/prometheus-publisher/minifi-cpp.crt
    nifi.metrics.publisher.PrometheusMetricsPublisher.ca.certificate=/tmp/certs/prometheus-publisher/root-ca.pem

## System Metrics

The following section defines the currently available metrics to be published by the MiNiFi C++ agent.

NOTE: In Prometheus all metrics are extended with a `minifi_` prefix to mark the domain of the metric. For example the `connection_name` metric is published as `minifi_connection_name` in Prometheus.

### Generic labels

The following labels are set for every single metric and are not listed separately in the labels of the metrics below.

| Label                    | Description                                                                                                                             |
|--------------------------|-----------------------------------------------------------------------------------------------------------------------------------------|
| metric_class             | Class name to filter for this metric, set to the name of the metric e.g. QueueMetrics, GetFileMetrics                                   |
| agent_identifier         | Set to the identifier set in minifi.properties in the nifi.metrics.publisher.agent.identifier property. If not set the hostname is used |

### QueueMetrics

QueueMetrics is a system level metric that reports queue metrics for every connection in the flow.

| Metric name          | Labels                           | Description                                |
|----------------------|----------------------------------|--------------------------------------------|
| queue_data_size      | connection_uuid, connection_name | Current queue data size                    |
| queue_data_size_max  | connection_uuid, connection_name | Max queue data size to apply back pressure |
| queue_size           | connection_uuid, connection_name | Current queue size                         |
| queue_size_max       | connection_uuid, connection_name | Max queue size to apply back pressure      |

| Label                    | Description                                                |
|--------------------------|------------------------------------------------------------|
| connection_uuid          | UUID of the connection defined in the flow configuration   |
| connection_name          | Name of the connection defined in the flow configuration   |

### RepositoryMetrics

RepositoryMetrics is a system level metric that reports metrics for the registered repositories (by default flowfile, content, and provenance repositories)

| Metric name                          | Labels          | Description                                                                                                      |
|--------------------------------------|-----------------|------------------------------------------------------------------------------------------------------------------|
| is_running                           | repository_name | Is the repository running (1 or 0)                                                                               |
| is_full                              | repository_name | Is the repository full (1 or 0)                                                                                  |
| repository_size_bytes                | repository_name | Current size of the repository                                                                                   |
| max_repository_size_bytes            | repository_name | Maximum size of the repository (0 if unlimited)                                                                  |
| repository_entry_count               | repository_name | Current number of entries in the repository                                                                      |
| rocksdb_table_readers_size_bytes     | repository_name | RocksDB's estimated memory used for reading SST tables (only present if repository uses RocksDB)                 |
| rocksdb_all_memory_tables_size_bytes | repository_name | RocksDB's approximate size of active and unflushed immutable memtables (only present if repository uses RocksDB) |

| Label                    | Description                                                                                                                            |
|--------------------------|----------------------------------------------------------------------------------------------------------------------------------------|
| repository_name          | Name of the reported repository. There are three repositories present with the following names: `flowfile`, `content` and `provenance` |

### DeviceInfoNode

DeviceInfoNode is a system level metric that reports metrics about the system resources used and available

| Metric name      | Labels       | Description                                                                                                              |
|------------------|--------------|--------------------------------------------------------------------------------------------------------------------------|
| physical_mem     | -            | Physical memory available                                                                                                |
| memory_usage     | -            | Physical memory usage of the system                                                                                      |
| cpu_utilization  | -            | CPU utilized by the system                                                                                               |
| cpu_load_average | -            | The number of processes in the system run queue averaged over the last minute. This metrics is not available on Windows. |

### FlowInformation

FlowInformation is a system level metric that reports component and queue related metrics.

| Metric name          | Labels                           | Description                                                                |
|----------------------|----------------------------------|----------------------------------------------------------------------------|
| queue_data_size      | connection_uuid, connection_name | Current queue data size                                                    |
| queue_data_size_max  | connection_uuid, connection_name | Max queue data size to apply back pressure                                 |
| queue_size           | connection_uuid, connection_name | Current queue size                                                         |
| queue_size_max       | connection_uuid, connection_name | Max queue size to apply back pressure                                      |
| is_running           | component_uuid, component_name   | Check if the component is running (1 or 0)                                 |
| bytes_read           | processor_uuid, processor_name   | Number of bytes read by the processor                                      |
| bytes_written        | processor_uuid, processor_name   | Number of bytes written by the processor                                   |
| flow_files_in        | processor_uuid, processor_name   | Number of flow files from the incoming queue processed by the processor    |
| flow_files_out       | processor_uuid, processor_name   | Number of flow files transferred to outgoing relationship by the processor |
| bytes_in             | processor_uuid, processor_name   | Sum of data from the incoming queue processed by the processor             |
| bytes_out            | processor_uuid, processor_name   | Sum of data transferred to outgoing relationship by the processor          |
| invocations          | processor_uuid, processor_name   | Number of times the processor was triggered                                |
| processing_nanos     | processor_uuid, processor_name   | Sum of the runtime spent in the processor in nanoseconds                   |


| Label           | Description                                                  |
|-----------------|--------------------------------------------------------------|
| connection_uuid | UUID of the connection defined in the flow configuration     |
| connection_name | Name of the connection defined in the flow configuration     |
| component_uuid  | UUID of the component                                        |
| component_name  | Name of the component                                        |
| processor_uuid  | UUID of the processor                                        |
| processor_name  | Name of the processor                                        |

### AgentStatus

AgentStatus is a system level metric that defines current agent status including repository, component and resource usage information.

| Metric name                          | Labels                         | Description                                                                                                      |
|--------------------------------------|--------------------------------|------------------------------------------------------------------------------------------------------------------|
| is_running                           | repository_name                | Is the repository running (1 or 0)                                                                               |
| is_full                              | repository_name                | Is the repository full (1 or 0)                                                                                  |
| repository_size_bytes                | repository_name                | Current size of the repository                                                                                   |
| max_repository_size_bytes            | repository_name                | Maximum size of the repository (0 if unlimited)                                                                  |
| repository_entry_count               | repository_name                | Current number of entries in the repository                                                                      |
| rocksdb_table_readers_size_bytes     | repository_name                | RocksDB's estimated memory used for reading SST tables (only present if repository uses RocksDB)                 |
| rocksdb_all_memory_tables_size_bytes | repository_name                | RocksDB's approximate size of active and unflushed immutable memtables (only present if repository uses RocksDB) |
| uptime_milliseconds                  | -                              | Agent uptime in milliseconds                                                                                     |
| is_running                           | component_uuid, component_name | Check if the component is running (1 or 0)                                                                       |
| agent_memory_usage_bytes             | -                              | Memory used by the agent process in bytes                                                                        |
| agent_cpu_utilization                | -                              | CPU utilization of the agent process (between 0 and 1). In case of a query error the returned value is -1.       |

| Label           | Description                                              |
|-----------------|----------------------------------------------------------|
| repository_name | Name of the reported repository                          |
| connection_uuid | UUID of the connection defined in the flow configuration |
| connection_name | Name of the connection defined in the flow configuration |
| component_uuid  | UUID of the component                                    |
| component_name  | Name of the component                                    |


## Processor Metrics

Processor level metrics can be accessed for any processor provided by MiNiFi. These metrics correspond to the name of the processor appended by the "Metrics" suffix (e.g. GetFileMetrics, TailFileMetrics, etc.).

Besides configuring processor metrics directly, they can also be configured using regular expressions with the `processorMetrics/` prefix.

All available processor metrics can be requested in the `minifi.properties` by using the following configuration:

    nifi.metrics.publisher.metrics=processorMetrics/.*

Regular expressions can also be used for requesting multiple processor metrics at once, like GetFileMetrics and GetTCPMetrics with the following configuration:

    nifi.metrics.publisher.metrics=processorMetrics/Get.*Metrics

### General Metrics

There are general metrics that are available for all processors. Besides these metrics processors can implement additional metrics that are speicific to that processor.

| Metric name                                 | Labels                                       | Description                                                                              |
|---------------------------------------------|----------------------------------------------|------------------------------------------------------------------------------------------|
| onTrigger_invocations                       | metric_class, processor_name, processor_uuid | The number of processor onTrigger calls                                                  |
| average_onTrigger_runtime_milliseconds      | metric_class, processor_name, processor_uuid | The average runtime in milliseconds of the last 10 onTrigger calls of the processor      |
| last_onTrigger_runtime_milliseconds         | metric_class, processor_name, processor_uuid | The runtime in milliseconds of the last onTrigger call of the processor                  |
| average_session_commit_runtime_milliseconds | metric_class, processor_name, processor_uuid | The average runtime in milliseconds of the last 10 session commit calls of the processor |
| last_session_commit_runtime_milliseconds    | metric_class, processor_name, processor_uuid | The runtime in milliseconds of the last session commit call of the processor             |
| transferred_flow_files                      | metric_class, processor_name, processor_uuid | Number of flow files transferred to a relationship                                       |
| transferred_bytes                           | metric_class, processor_name, processor_uuid | Number of bytes transferred to a relationship                                            |
| transferred_to_\<relationship\>             | metric_class, processor_name, processor_uuid | Number of flow files transferred to a specific relationship                              |
| incoming_flow_files                         | metric_class, processor_name, processor_uuid | Number of flow files from the incoming queue processed by the processor                  |
| incoming_bytes                              | metric_class, processor_name, processor_uuid | Sum of data from the incoming queue processed by the processor                           |
| bytes_read                                  | metric_class, processor_name, processor_uuid | Number of bytes read by the processor                                                    |
| bytes_written                               | metric_class, processor_name, processor_uuid | Number of bytes written by the processor                                                 |
| processing_nanos                            | metric_class, processor_name, processor_uuid | Sum of the runtime spent in the processor in nanoseconds                                 |

| Label          | Description                                                            |
|----------------|------------------------------------------------------------------------|
| metric_class   | Class name to filter for this metric, set to \<processor type\>Metrics |
| processor_name | Name of the processor                                                  |
| processor_uuid | UUID of the processor                                                  |

### GetFileMetrics

Processor level metric that reports metrics for the GetFile processor if defined in the flow configuration.

| Metric name           | Labels                                       | Description                                    |
|-----------------------|----------------------------------------------|------------------------------------------------|
| accepted_files        | metric_class, processor_name, processor_uuid | Number of files that matched the set criterias |
| input_bytes           | metric_class, processor_name, processor_uuid | Sum of file sizes processed                    |

| Label          | Description                                                    |
|----------------|----------------------------------------------------------------|
| metric_class   | Class name to filter for this metric, set to GetFileMetrics    |
| processor_name | Name of the processor                                          |
| processor_uuid | UUID of the processor                                          |

### RunLlamaCppInferenceMetrics

Processor level metric that reports metrics for the RunLlamaCppInference processor if defined in the flow configuration.

| Metric name           | Labels                                       | Description                                                                |
|-----------------------|----------------------------------------------|----------------------------------------------------------------------------|
| tokens_in             | metric_class, processor_name, processor_uuid | Number of tokens parsed from the input prompts in the processor's lifetime |
| tokens_out            | metric_class, processor_name, processor_uuid | Number of tokens generated in the completion in the processor's lifetime   |

| Label          | Description                                                              |
|----------------|--------------------------------------------------------------------------|
| metric_class   | Class name to filter for this metric, set to RunLlamaCppInferenceMetrics |
| processor_name | Name of the processor                                                    |
| processor_uuid | UUID of the processor                                                    |
