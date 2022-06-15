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

# Controller Services

## Table of Contents

- [AWSCredentialsService](#AWSCredentialsService)
- [AzureStorageCredentialsService](#AzureStorageCredentialsService)
- [GCPCredentialsControllerService](#GCPCredentialsControllerService)
- [ElasticsearchCredentialsControllerService](#ElasticsearchCredentialsControllerService)
- [KubernetesControllerService](#kubernetesControllerService)

## AWSCredentialsService

### Description

Manages the Amazon Web Services (AWS) credentials for an AWS account. This allows for multiple
AWS credential services to be defined. This also allows for multiple AWS related processors to reference this single
controller service so that AWS credentials can be managed and controlled in a central location.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional.

| Name                        | Default Value | Allowable Values | Description                                                                                                                                 |
|-----------------------------|---------------|------------------|---------------------------------------------------------------------------------------------------------------------------------------------|
| **Use Default Credentials** | false         |                  | If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc. |
| Access Key                  |               |                  | Specifies the AWS Access Key                                                                                                                |
| Secret Key                  |               |                  | Specifies the AWS Secret Key                                                                                                                |
| Credentials File            |               |                  | Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey                 |


## AzureStorageCredentialsService

### Description

Manages the credentials for an Azure Storage account. This allows for multiple Azure Storage related processors to reference this single
controller service so that Azure storage credentials can be managed and controlled in a central location.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional.

| Name                                   | Default Value | Allowable Values | Description                                                                                                                                                                                                                 |
|----------------------------------------|---------------|------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Storage Account Name                   |               |                  | The storage account name.                                                                                                                                                                                                   |
| Storage Account Key                    |               |                  | The storage account key. This is an admin-like password providing access to every container in this account. It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies. |
| SAS Token                              |               |                  | Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Managed Identity is not used.                                                            |
| Common Storage Account Endpoint Suffix |               |                  | Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a different suffix in certain circumstances (like Azure Stack or non-public Azure regions).                            |
| Connection String                      |               |                  | Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Managed Identity is not used.                                                                             |
| **Use Managed Identity Credentials**   | false         |                  | Connection string used to connect to Azure Storage service. This overrides all other set credential properties.                                                                                                             |

## GCPCredentialsControllerService

### Description

Manages the credentials for Google Cloud Platform. This allows for multiple Google Cloud Platform related processors to reference this single
controller service so that Google Cloud Platform credentials can be managed and controlled in a central location.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional.


| Name                      | Default Value                          | Allowable Values                                                                                                                                            | Description                                                          |
|---------------------------|----------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------|
| **Credentials Location**  | Google Application Default Credentials | Google Application Default Credentials<br> Use Compute Engine Credentials<br>Service Account JSON File<br>Service Account JSON<br>Use Anonymous credentials | The location of the credentials.                                     |
| Service Account JSON File |                                        |                                                                                                                                                             | Path to a file containing a Service Account key file in JSON format. |
| Service Account JSON      |                                        |                                                                                                                                                             | The raw JSON containing a Service Account keyfile.                   |


## ElasticsearchCredentialsControllerService

### Description

Elasticsearch Credentials Controller Service

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional.


| Name                   | Default Value | Allowable Values                 | Description                           |
|------------------------|---------------|----------------------------------|---------------------------------------|
| Username               |               |                                  | The username for basic authentication |
| Password               |               |                                  | The password for basic authentication |
| API Key                |               |                                  | The API Key to use                    |


## KubernetesControllerService

### Description

Controller service that provides access to the Kubernetes API.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional.

| Name                  | Default Value | Allowable Values | Description                                                                                  |
|-----------------------|---------------|------------------|----------------------------------------------------------------------------------------------|
| Namespace Filter      | default       |                  | Limit the output to pods in namespaces which match this regular expression                   |
| Pod Name Filter       |               |                  | If present, limit the output to pods the name of which matches this regular expression       |
| Container Name Filter |               |                  | If present, limit the output to containers the name of which matches this regular expression |
