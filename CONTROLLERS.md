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

- [AWSCredentialsService](#awsCredentialsService)

## AWSCredentialsService

### Description

Manages the Amazon Web Services (AWS) credentials for an AWS account. This allows for multiple
AWS credential services to be defined. This also allows for multiple AWS related processors to reference this single
controller service so that AWS credentials can be managed and controlled in a central location.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Expression Language Supported? | Description |
| - | - | - | - | - |
|**Use Default Credentials**|false||No|If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc.|
|Access Key|||Yes|Specifies the AWS Access Key|
|Secret Key|||Yes|Specifies the AWS Secret Key|
|Credentials File|||No|Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey|
