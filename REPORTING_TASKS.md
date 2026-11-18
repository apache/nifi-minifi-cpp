<!--
  SPDX-License-Identifier: Apache-2.0
  SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
  SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
