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

- [AWSCredentialsService](#AWSCredentialsService)
- [AzureStorageCredentialsService](#AzureStorageCredentialsService)
- [ElasticsearchCredentialsControllerService](#ElasticsearchCredentialsControllerService)
- [ExecuteJavaControllerService](#ExecuteJavaControllerService)
- [GCPCredentialsControllerService](#GCPCredentialsControllerService)
- [JavaControllerService](#JavaControllerService)
- [KubernetesControllerService](#KubernetesControllerService)
- [LinuxPowerManagerService](#LinuxPowerManagerService)
- [NetworkPrioritizerService](#NetworkPrioritizerService)
- [ODBCService](#ODBCService)
- [PersistentMapStateStorage](#PersistentMapStateStorage)
- [RocksDbStateStorage](#RocksDbStateStorage)
- [SmbConnectionControllerService](#SmbConnectionControllerService)
- [SSLContextService](#SSLContextService)
- [UpdatePolicyControllerService](#UpdatePolicyControllerService)
- [VolatileMapStateStorage](#VolatileMapStateStorage)


## AWSCredentialsService

### Description

Manages the Amazon Web Services (AWS) credentials for an AWS account. This allows for multiple AWS credential services to be defined. This also allows for multiple AWS related processors to reference this single controller service so that AWS credentials can be managed and controlled in a central location.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                        | Default Value | Allowable Values | Description                                                                                                                                 |
|-----------------------------|---------------|------------------|---------------------------------------------------------------------------------------------------------------------------------------------|
| **Use Default Credentials** | false         | true<br/>false   | If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc. |
| Access Key                  |               |                  | Specifies the AWS Access Key.                                                                                                               |
| Secret Key                  |               |                  | Specifies the AWS Secret Key.                                                                                                               |
| Credentials File            |               |                  | Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey                 |


## AzureStorageCredentialsService

### Description

Manages the credentials for an Azure Storage account. This allows for multiple Azure Storage related processors to reference this single controller service so that Azure storage credentials can be managed and controlled in a central location.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                   | Default Value | Allowable Values | Description                                                                                                                                                                                                                 |
|----------------------------------------|---------------|------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Storage Account Name                   |               |                  | The storage account name.                                                                                                                                                                                                   |
| Storage Account Key                    |               |                  | The storage account key. This is an admin-like password providing access to every container in this account. It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies. |
| SAS Token                              |               |                  | Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Managed Identity is not used.                                                            |
| Common Storage Account Endpoint Suffix |               |                  | Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a different suffix in certain circumstances (like Azure Stack or non-public Azure regions).                            |
| Connection String                      |               |                  | Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Managed Identity is not used.                                                                             |
| **Use Managed Identity Credentials**   | false         | true<br/>false   | If true Managed Identity credentials will be used together with the Storage Account Name for authentication.                                                                                                                |


## ElasticsearchCredentialsControllerService

### Description

Elasticsearch/Opensearch Credentials Controller Service

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name     | Default Value | Allowable Values | Description                                                                      |
|----------|---------------|------------------|----------------------------------------------------------------------------------|
| Username |               |                  | The username for basic authentication<br/>**Supports Expression Language: true** |
| Password |               |                  | The password for basic authentication<br/>**Supports Expression Language: true** |
| API Key  |               |                  | The API Key to use                                                               |


## ExecuteJavaControllerService

### Description

ExecuteJavaClass runs NiFi Controller services given a provided system path

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                        | Default Value | Allowable Values | Description                                     |
|-----------------------------|---------------|------------------|-------------------------------------------------|
| **NiFi Controller Service** |               |                  | Name of NiFi Controller Service to load and run |


## GCPCredentialsControllerService

### Description

Manages the credentials for Google Cloud Platform. This allows for multiple Google Cloud Platform related processors to reference this single controller service so that Google Cloud Platform credentials can be managed and controlled in a central location.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                      | Default Value                          | Allowable Values                                                                                                                                               | Description                                                          |
|---------------------------|----------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------|
| **Credentials Location**  | Google Application Default Credentials | Google Application Default Credentials<br/>Use Compute Engine Credentials<br/>Service Account JSON File<br/>Service Account JSON<br/>Use Anonymous credentials | The location of the credentials.                                     |
| Service Account JSON File |                                        |                                                                                                                                                                | Path to a file containing a Service Account key file in JSON format. |
| Service Account JSON      |                                        |                                                                                                                                                                | The raw JSON containing a Service Account keyfile.                   |


## JavaControllerService

### Description

Allows specification of nars to be used within referenced processors.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                         | Default Value | Allowable Values | Description                                   |
|------------------------------|---------------|------------------|-----------------------------------------------|
| **Nar Directory**            |               |                  | Directory containing the nars to deploy       |
| **Nar Deployment Directory** |               |                  | Directory in which nars will be deployed      |
| **Nar Document Directory**   |               |                  | Directory in which documents will be deployed |


## KubernetesControllerService

### Description

Controller service that provides access to the Kubernetes API

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                  | Default Value | Allowable Values | Description                                                                                  |
|-----------------------|---------------|------------------|----------------------------------------------------------------------------------------------|
| Namespace Filter      | default       |                  | Limit the output to pods in namespaces which match this regular expression                   |
| Pod Name Filter       |               |                  | If present, limit the output to pods the name of which matches this regular expression       |
| Container Name Filter |               |                  | If present, limit the output to containers the name of which matches this regular expression |


## LinuxPowerManagerService

### Description

Linux power management service that enables control of power usage in the agent through Linux power management information. Use name "ThreadPoolManager" to throttle battery consumption

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                         | Default Value                         | Allowable Values | Description                                                                                |
|------------------------------|---------------------------------------|------------------|--------------------------------------------------------------------------------------------|
| **Battery Capacity Path**    | /sys/class/power_supply/BAT0/capacity |                  | Path to the battery level                                                                  |
| **Battery Status Path**      | /sys/class/power_supply/BAT0/status   |                  | Path to the battery status ( Discharging/Battery )                                         |
| **Battery Status Discharge** | Discharging                           |                  | Keyword to identify if battery is discharging                                              |
| **Trigger Threshold**        | 75                                    |                  | Battery threshold before which we consider a slow reduction. Should be a number from 1-100 |
| **Low Battery Threshold**    | 50                                    |                  | Battery threshold before which we will aggressively reduce. Should be a number from 1-100  |
| **Wait Period**              | 100 ms                                |                  | Decay between checking threshold and determining if a reduction is needed                  |


## NetworkPrioritizerService

### Description

Enables selection of networking interfaces on defined parameters to include output and payload size

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                  | Default Value | Allowable Values | Description                                                                           |
|-----------------------|---------------|------------------|---------------------------------------------------------------------------------------|
| Network Controllers   |               |                  | Comma separated list of network controllers in order of priority for this prioritizer |
| **Max Throughput**    | 1 MB          |                  | Max throughput ( per second ) for these network controllers                           |
| **Max Payload**       | 1 GB          |                  | Maximum payload for these network controllers                                         |
| **Verify Interfaces** | true          | true<br/>false   | Verify that interfaces are operational                                                |
| Default Prioritizer   | false         | true<br/>false   | Sets this controller service as the default prioritizer for all comms                 |


## ODBCService

### Description

Controller service that provides ODBC database connection

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                  | Default Value | Allowable Values | Description                |
|-----------------------|---------------|------------------|----------------------------|
| **Connection String** |               |                  | Database Connection String |


## PersistentMapStateStorage

### Description

A persistable state storage service implemented by a locked std::unordered_map<std::string, std::string> and persisted into a file

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                      | Default Value | Allowable Values | Description                                                                                                                                            |
|---------------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------|
| Always Persist            | false         | true<br/>false   | Persist every change instead of persisting it periodically.                                                                                            |
| Auto Persistence Interval | 1 min         |                  | The interval of the periodic task persisting all values. Only used if Always Persist is false. If set to 0 seconds, auto persistence will be disabled. |
| **File**                  |               |                  | Path to a file to store state                                                                                                                          |


## RocksDbStateStorage

### Description

A state storage service implemented by RocksDB

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                      | Default Value | Allowable Values | Description                                                                                                                                            |
|---------------------------|---------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------|
| Always Persist            | false         | true<br/>false   | Persist every change instead of persisting it periodically.                                                                                            |
| Auto Persistence Interval | 1 min         |                  | The interval of the periodic task persisting all values. Only used if Always Persist is false. If set to 0 seconds, auto persistence will be disabled. |
| **Directory**             |               |                  | Path to a directory for the database                                                                                                                   |


## SmbConnectionControllerService

### Description

SMB Connection Controller Service

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name         | Default Value | Allowable Values | Description                                                                                                                     |
|--------------|---------------|------------------|---------------------------------------------------------------------------------------------------------------------------------|
| **Hostname** |               |                  | The network host to which files should be written.                                                                              |
| **Share**    |               |                  | The network share to which files should be written. This is the "first folder" after the hostname: \\hostname\[share]\dir1\dir2 |
| Domain       |               |                  | The domain used for authentication. Optional, in most cases username and password is sufficient.                                |
| Username     |               |                  | The username used for authentication. If no username is set then anonymous authentication is attempted.                         |
| Password     |               |                  | The password used for authentication. Required if Username is set.                                                              |


## SSLContextService

### Description

Controller service that provides SSL/TLS capabilities to consuming interfaces

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                       | Default Value         | Allowable Values                                                                                                                                         | Description                                                                                                                              |
|----------------------------|-----------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------|
| Certificate Store Location | LocalMachine          | CurrentUser<br/>LocalMachine<br/>CurrentService<br/>Services<br/>Users<br/>CurrentUserGroupPolicy<br/>LocalMachineGroupPolicy<br/>LocalMachineEnterprise | One of the Windows certificate store locations, eg. LocalMachine or CurrentUser (Windows only)                                           |
| Server Cert Store          | ROOT                  |                                                                                                                                                          | The name of the certificate store which contains the server certificate (Windows only)                                                   |
| Client Cert Store          | MY                    |                                                                                                                                                          | The name of the certificate store which contains the client certificate (Windows only)                                                   |
| Client Cert CN             |                       |                                                                                                                                                          | The CN that the client certificate is required to match; default: use the first available client certificate in the store (Windows only) |
| Client Cert Key Usage      | Client Authentication |                                                                                                                                                          | Comma-separated list of enhanced key usage values that the client certificate is required to have (Windows only)                         |
| Client Certificate         |                       |                                                                                                                                                          | Client Certificate                                                                                                                       |
| Private Key                |                       |                                                                                                                                                          | Private Key file                                                                                                                         |
| Passphrase                 |                       |                                                                                                                                                          | Client passphrase. Either a file or unencrypted text                                                                                     |
| CA Certificate             |                       |                                                                                                                                                          | CA certificate file                                                                                                                      |
| Use System Cert Store      | false                 | true<br/>false                                                                                                                                           | Whether to use the certificates in the OS's certificate store                                                                            |


## UpdatePolicyControllerService

### Description

UpdatePolicyControllerService allows a flow specific policy on allowing or disallowing updates. Since the flow dictates the purpose of a device it will also be used to dictate updates to specific components.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                  | Default Value | Allowable Values | Description                                                           |
|-----------------------|---------------|------------------|-----------------------------------------------------------------------|
| Allow All Properties  | false         | true<br/>false   | Allows all properties, which are also not disallowed, to be updated   |
| Persist Updates       | false         | true<br/>false   | Property that dictates whether updates should persist after a restart |
| Allowed Properties    |               |                  | Properties for which we will allow updates                            |
| Disallowed Properties |               |                  | Properties for which we will not allow updates                        |


## VolatileMapStateStorage

### Description

A key-value service implemented by a locked std::unordered_map<std::string, std::string>

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name            | Default Value | Allowable Values | Description                    |
|-----------------|---------------|------------------|--------------------------------|
| Linked Services |               |                  | Referenced Controller Services |
