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
- [CouchbaseClusterService](#CouchbaseClusterService)
- [ElasticsearchCredentialsControllerService](#ElasticsearchCredentialsControllerService)
- [GCPCredentialsControllerService](#GCPCredentialsControllerService)
- [JsonTreeReader](#JsonTreeReader)
- [JsonRecordSetWriter](#JsonRecordSetWriter)
- [KubernetesControllerService](#KubernetesControllerService)
- [LinuxPowerManagerService](#LinuxPowerManagerService)
- [NetworkPrioritizerService](#NetworkPrioritizerService)
- [ODBCService](#ODBCService)
- [PersistentMapStateStorage](#PersistentMapStateStorage)
- [ProxyConfigurationService](#ProxyConfigurationService)
- [RocksDbStateStorage](#RocksDbStateStorage)
- [SmbConnectionControllerService](#SmbConnectionControllerService)
- [SSLContextService](#SSLContextService)
- [UpdatePolicyControllerService](#UpdatePolicyControllerService)
- [VolatileMapStateStorage](#VolatileMapStateStorage)
- [XMLReader](#XMLReader)
- [XMLRecordSetWriter](#XMLRecordSetWriter)


## AWSCredentialsService

### Description

Manages the Amazon Web Services (AWS) credentials for an AWS account. This allows for multiple AWS credential services to be defined. This also allows for multiple AWS related processors to reference this single controller service so that AWS credentials can be managed and controlled in a central location.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                        | Default Value | Allowable Values | Description                                                                                                                                 |
|-----------------------------|---------------|------------------|---------------------------------------------------------------------------------------------------------------------------------------------|
| **Use Default Credentials** | false         | true<br/>false   | If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc. |
| Access Key                  |               |                  | Specifies the AWS Access Key.                                                                                                               |
| Secret Key                  |               |                  | Specifies the AWS Secret Key.<br/>**Sensitive Property: true**                                                                              |
| Credentials File            |               |                  | Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey                 |


## AzureStorageCredentialsService

### Description

Manages the credentials for an Azure Storage account. This allows for multiple Azure Storage related processors to reference this single controller service so that Azure storage credentials can be managed and controlled in a central location.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                   | Default Value   | Allowable Values                                                                  | Description                                                                                                                                                                                                                                                                                                                                                         |
|----------------------------------------|-----------------|-----------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Storage Account Name                   |                 |                                                                                   | The storage account name.                                                                                                                                                                                                                                                                                                                                           |
| Storage Account Key                    |                 |                                                                                   | The storage account key. This is an admin-like password providing access to every container in this account. It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies if Credential Configuration Strategy is set to From Properties. If set, SAS Token must be empty.<br/>**Sensitive Property: true**        |
| SAS Token                              |                 |                                                                                   | Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Credential Configuration Strategy is set to From Properties. If set, Storage Account Key must be empty.<br/>**Sensitive Property: true**                                                                                         |
| Common Storage Account Endpoint Suffix |                 |                                                                                   | Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a different suffix in certain circumstances (like Azure Stack or non-public Azure regions).                                                                                                                                                                    |
| Connection String                      |                 |                                                                                   | Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Credential Configuration Strategy is set to From Properties.                                                                                                                                                                                      |
| **Credential Configuration Strategy**  | From Properties | From Properties<br/>Default Credential<br/>Managed Identity<br/>Workload Identity | The strategy to use for credential configuration. If set to From Properties, the credentials are parsed from the SAS Token, Storage Account Key, and Connection String properties. In other cases, the selected Azure identity source is used.                                                                                                                      |
| Managed Identity Client ID             |                 |                                                                                   | Client ID of the managed identity. The property is required when User Assigned Managed Identity is used for authentication and multiple user-assigned identities are added to the resource. It must be empty in case of System Assigned Managed Identity and can also be left empty if only one user-assigned identity is present.<br/>**Sensitive Property: true** |


## CouchbaseClusterService

### Description

Provides a centralized Couchbase connection and bucket passwords management. Bucket passwords can be specified via dynamic properties.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                  | Default Value | Allowable Values | Description                                                                                                                                                      |
|-----------------------|---------------|------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Connection String** |               |                  | The hostnames or ip addresses of the bootstraping nodes and optional parameters. Syntax: couchbase://node1,node2,nodeN?param1=value1&param2=value2&paramN=valueN |
| User Name             |               |                  | The user name to authenticate MiNiFi as a Couchbase client.                                                                                                      |
| User Password         |               |                  | The user password to authenticate MiNiFi as a Couchbase client.<br/>**Sensitive Property: true**                                                                 |


## ElasticsearchCredentialsControllerService

### Description

Elasticsearch/Opensearch Credentials Controller Service

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name     | Default Value | Allowable Values | Description                                                                                                       |
|----------|---------------|------------------|-------------------------------------------------------------------------------------------------------------------|
| Username |               |                  | The username for basic authentication<br/>**Supports Expression Language: true**                                  |
| Password |               |                  | The password for basic authentication<br/>**Sensitive Property: true**<br/>**Supports Expression Language: true** |
| API Key  |               |                  | The API Key to use<br/>**Sensitive Property: true**                                                               |


## GCPCredentialsControllerService

### Description

Manages the credentials for Google Cloud Platform. This allows for multiple Google Cloud Platform related processors to reference this single controller service so that Google Cloud Platform credentials can be managed and controlled in a central location.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                      | Default Value                          | Allowable Values                                                                                                                                               | Description                                                                         |
|---------------------------|----------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------|
| **Credentials Location**  | Google Application Default Credentials | Google Application Default Credentials<br/>Use Compute Engine Credentials<br/>Service Account JSON File<br/>Service Account JSON<br/>Use Anonymous credentials | The location of the credentials.                                                    |
| Service Account JSON File |                                        |                                                                                                                                                                | Path to a file containing a Service Account key file in JSON format.                |
| Service Account JSON      |                                        |                                                                                                                                                                | The raw JSON containing a Service Account keyfile.<br/>**Sensitive Property: true** |


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


## JsonTreeReader

### Description

Parses JSON into individual Record objects. While the reader expects each record to be well-formed JSON, the content of a FlowFile may consist of many records, each as a well-formed JSON array or JSON object with optional whitespace between them, such as the common 'JSON-per-line' format. If an array is encountered, each element in that array will be treated as a separate record. If the schema that is configured contains a field that is not present in the JSON, a null value will be used. If the JSON contains a field that is not present in the schema, that field will be skipped.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
|------|---------------|------------------|-------------|


## JsonRecordSetWriter

### Description

Writes the results of a RecordSet as either a JSON Array or one JSON object per line. If using Array output, then even if the RecordSet consists of a single row, it will be written as an array with a single element. If using One Line Per Object output, the JSON objects cannot be pretty-printed.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                | Default Value | Allowable Values              | Description                                                                                                                                 |
|---------------------|---------------|-------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------|
| **Output Grouping** | Array         | Array<br/>One Line Per Object | Specifies how the writer should output the JSON records. Note that if 'One Line Per Object' is selected, then Pretty Print JSON is ignored. |
| Pretty Print JSON   | false         | true<br/>false                | Specifies whether or not the JSON should be pretty printed (only used when Array output is selected)                                        |


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

| Name                  | Default Value | Allowable Values | Description                                                 |
|-----------------------|---------------|------------------|-------------------------------------------------------------|
| **Connection String** |               |                  | Database Connection String<br/>**Sensitive Property: true** |


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


## ProxyConfigurationService

### Description

Provides a set of configurations for different MiNiFi C++ components to use a proxy server. Currently these properties can only be used for HTTP proxy configuration, not other protocols are supported at this time.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                  | Default Value | Allowable Values | Description                                                                                |
|-----------------------|---------------|------------------|--------------------------------------------------------------------------------------------|
| **Proxy Server Host** |               |                  | Proxy server hostname or ip-address.                                                       |
| Proxy Server Port     |               |                  | Proxy server port number.                                                                  |
| Proxy User Name       |               |                  | The name of the proxy client for user authentication.                                      |
| Proxy User Password   |               |                  | The password of the proxy client for user authentication.<br/>**Sensitive Property: true** |


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
| Password     |               |                  | The password used for authentication. Required if Username is set.<br/>**Sensitive Property: true**                             |


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
| Passphrase                 |                       |                                                                                                                                                          | Client passphrase. Either a file or unencrypted text<br/>**Sensitive Property: true**                                                    |
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


## XMLReader

### Description

Reads XML content and creates Record objects. Records are expected in the second level of XML data, embedded in an enclosing root tag. Types for records are inferred automatically based on the content of the XML tags. For timestamps, the format is expected to be ISO 8601 compliant.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                        | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
|-----------------------------|---------------|------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Field Name for Content      |               |                  | If tags with content (e. g. <field>content</field>) are defined as nested records in the schema, the name of the tag will be used as name for the record and the value of this property will be used as name for the field. If the tag contains subnodes besides the content (e.g. <field>content<subfield>subcontent</subfield></field>), or a node attribute is present, we need to define a name for the text content, so that it can be distinguished from the subnodes. If this property is not set, the default name 'value' will be used for the text content of the tag in this case. |
| **Parse XML Attributes**    | false         | true<br/>false   | When this property is 'true' then XML attributes are parsed and added to the record as new fields, otherwise XML attributes and their values are ignored.                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| Attribute Prefix            |               |                  | If this property is set, the name of attributes will be prepended with a prefix when they are added to a record.                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| **Expect Records as Array** | false         | true<br/>false   | This property defines whether the reader expects a FlowFile to consist of a single Record or a series of Records with a "wrapper element". Because XML does not provide for a way to read a series of XML documents from a stream directly, it is common to combine many XML documents by concatenating them and then wrapping the entire XML blob with a "wrapper element". This property dictates whether the reader expects a FlowFile to consist of a single Record or a series of Records with a "wrapper element" that will be ignored.                                                 |


## XMLRecordSetWriter

### Description

Writes a RecordSet to XML. The records are wrapped by a root tag.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                        | Default Value | Allowable Values                                                      | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
|-----------------------------|---------------|-----------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Array Tag Name              |               |                                                                       | Name of the tag used by property "Wrap Elements of Arrays" to write arrays                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| **Wrap Elements of Arrays** | No Wrapping   | Use Property as Wrapper<br/>Use Property for Elements<br/>No Wrapping | Specifies how the writer wraps elements of fields of type array. If 'Use Property as Wrapper' is set, the property "Array Tag Name" will be used as the tag name to wrap elements of an array. The field name of the array field will be used for the tag name of the elements. If 'Use Property for Elements' is set, the property "Array Tag Name" will be used for the tag name of the elements of an array. The field name of the array field will be used as the tag name to wrap elements. If 'No Wrapping' is set, the elements of an array will not be wrapped. |
| **Omit XML Declaration**    | false         | true<br/>false                                                        | Specifies whether or not to include XML declaration                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| **Pretty Print XML**        | false         | true<br/>false                                                        | Specifies whether or not the XML should be pretty printed                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| **Name of Record Tag**      |               |                                                                       | Specifies the name of the XML record tag wrapping the record fields.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| **Name of Root Tag**        |               |                                                                       | Specifies the name of the XML root tag wrapping the record set.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |

