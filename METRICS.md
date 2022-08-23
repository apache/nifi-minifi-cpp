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

- [Description](#description)
- [Configuration](#configuration)
- [Metrics](#metrics)

## Description

Apache NiFi MiNiFi C++ can communicate metrics about the agent's status, that can be a system level or component level metric.
These metrics are exposed through the agent implemented metric publishers that can be configured in the minifi.properties.
Aside from the publisher exposed metrics, metrics are also sent through C2 protocol of which there is more information in the
[C2 documentation](C2.md#metrics).

## Configuration

To configure the a metrics publisher first we have to set which publisher class should be used:

	# in minifi.properties

	nifi.metrics.publisher.class=PrometheusMetricsPublisher

Currently PrometheusMetricsPublisher is the only available publisher in MiNiFi C++ which publishes metrics to a Prometheus server.
To use the publisher a port should also be configured where the metrics will be available to be scraped through:

	# in minifi.properties

	nifi.metrics.publisher.PrometheusMetricsPublisher.port=9936

The following option defines which metric classes should be exposed through the metrics publisher in configured with a comma separated value:

	# in minifi.properties

	nifi.metrics.publisher.metrics=QueueMetrics,RepositoryMetrics,GetFileMetrics,DeviceInfoNode,FlowInformation

An agent identifier should also be defined to identify which agent the metric is exposed from. If not set, the hostname is used as the identifier.

	# in minifi.properties

	nifi.metrics.publisher.agent.identifier=Agent1

## Metrics

The following section defines the currently available metrics to be published by the MiNiFi C++ agent.

NOTE: In Prometheus all metrics are extended with a `minifi_` prefix to mark the domain of the metric. For example the `connection_name` metric is published as `minifi_connection_name` in Prometheus.

## Generic labels

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

RepositoryMetrics is a system level metric that reports metrics for the registered repositories (by default flowfile and provenance repository)

| Metric name          | Labels          | Description                           |
|----------------------|-----------------|---------------------------------------|
| is_running           | repository_name | Is the repository running (1 or 0)    |
| is_full              | repository_name | Is the repository full (1 or 0)       |
| repository_size      | repository_name | Current size of the repository        |

| Label                    | Description                                                     |
|--------------------------|-----------------------------------------------------------------|
| repository_name          | Name of the reported repository                                 |

### GetFileMetrics

Processor level metric that reports metrics for the GetFile processor if defined in the flow configuration

| Metric name           | Labels                         | Description                                    |
|-----------------------|--------------------------------|------------------------------------------------|
| onTrigger_invocations | processor_name, processor_uuid | Number of times the processor was triggered    |
| accepted_files        | processor_name, processor_uuid | Number of files that matched the set criterias |
| input_bytes           | processor_name, processor_uuid | Sum of file sizes processed                    |

| Label          | Description                                                    |
|----------------|----------------------------------------------------------------|
| processor_name | Name of the processor                                          |
| processor_uuid | UUID of the processor                                          |

### GetTCPMetrics

Processor level metric that reports metrics for the GetTCPMetrics processor if defined in the flow configuration

| Metric name           | Labels                         | Description                                    |
|-----------------------|--------------------------------|------------------------------------------------|
| onTrigger_invocations | processor_name, processor_uuid | Number of times the processor was triggered    |

| Label          | Description                                                    |
|----------------|----------------------------------------------------------------|
| processor_name | Name of the processor                                          |
| processor_uuid | UUID of the processor                                          |

### DeviceInfoNode

DeviceInfoNode is a system level metric that reports metrics about the system resources used and available

| Metric name     | Labels       | Description               |
|-----------------|--------------|---------------------------|
| physical_mem    | -            | Physical memory available |
| memory_usage    | -            | Memory used by the agent  |
| cpu_utilization | -            | CPU utilized by the agent |

### FlowInformation

DeviceInfoNode is a system level metric that reports metrics about the system resources used and available

| Metric name          | Labels                           | Description                                |
|----------------------|----------------------------------|--------------------------------------------|
| queue_data_size      | connection_uuid, connection_name | Current queue data size                    |
| queue_data_size_max  | connection_uuid, connection_name | Max queue data size to apply back pressure |
| queue_size           | connection_uuid, connection_name | Current queue size                         |
| queue_size_max       | connection_uuid, connection_name | Max queue size to apply back pressure      |
| is_running           | component_uuid, component_name   | Check if the component is running (1 or 0) |

| Label           | Description                                                  |
|-----------------|--------------------------------------------------------------|
| connection_uuid | UUID of the connection defined in the flow configuration     |
| connection_name | Name of the connection defined in the flow configuration     |
| component_uuid  | UUID of the component                                        |
| component_name  | Name of the component                                        |
