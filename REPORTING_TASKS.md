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

- [SiteToSiteProvenanceReportingTask](#SiteToSiteProvenanceReportingTask)


## SiteToSiteProvenanceReportingTask

### Description

Publishes Provenance events using the Site To Site protocol. The events are read from the agent's provenance repository and are sent in JSON format to the remote input port of a NiFi instance.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                | Default Value | Allowable Values | Description                                                                                                                               |
|---------------------|---------------|------------------|-------------------------------------------------------------------------------------------------------------------------------------------|
| **Destination URL** |               |                  | The URL of the destination NiFi instance in the format of http(s)://host:port/nifi. This is used to initiate the Site-to-Site connection. |
| **Batch Size**      | 100           |                  | The maximum number of provenance events to send to the destination in a single transaction.                                               |
| SSL Context Service |               |                  | The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.                                   |
| Port UUID           |               |                  | Specifies remote NiFi Port UUID.                                                                                                          |
| **Idle Timeout**    | 15 s          |                  | Max idle time for remote service                                                                                                          |
