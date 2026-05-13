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

### Processors

- [AsciifyGerman](#AsciifyGerman)
- [CountActualLogging](#CountActualLogging)
- [GenerateFlowFileRs](#GenerateFlowFileRs)
- [GetFileRs](#GetFileRs)
- [KamikazeProcessorRs](#KamikazeProcessorRs)
- [LogAttributeRs](#LogAttributeRs)
- [LoremIpsumCSUser](#LoremIpsumCSUser)
- [PutFileRs](#PutFileRs)
### Controller Services

- [DummyControllerService](#DummyControllerService)
- [LoremIpsumControllerService](#LoremIpsumControllerService)


## AsciifyGerman

### Description

This processor switches German characters with their ascii counterparts. (to test stream API)

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
|------|---------------|------------------|-------------|

### Relationships

| Name    | Description                             |
|---------|-----------------------------------------|
| success | All asciified flowfiles are routed here |
| failure | Non-german flowfiles are routed here    |


## CountActualLogging

### Description

For testing lazy logging

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
|------|---------------|------------------|-------------|

### Relationships

| Name | Description |
|------|-------------|


## GenerateFlowFileRs

### Description

This processor creates FlowFiles with random data or custom content. GenerateFlowFile is useful for load testing, configuration, and simulation.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                 | Default Value | Allowable Values | Description                                                                                                                                                                                                                                                                                                                      |
|----------------------|---------------|------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **File Size**        | 1 kB          |                  | The size of the file that will be used<br/>**Supports Expression Language: true**                                                                                                                                                                                                                                                |
| **Batch Size**       | 1             |                  | The number of FlowFiles to be transferred in each invocation                                                                                                                                                                                                                                                                     |
| **Data Format**      | Binary        | Text<br/>Binary  | Specifies whether the data should be Text or Binary                                                                                                                                                                                                                                                                              |
| **Unique FlowFiles** | true          | true<br/>false   | If true, each FlowFile that is generated will be unique. If false, a random value will be generated and all FlowFiles will get the same content but this offers much higher throughput (but see the description of Custom Text for special non-random use cases)                                                                 |
| Custom Text          |               |                  | If Data Format is text and if Unique FlowFiles is false, then this custom text will be used as content of the generated FlowFiles and the File Size will be ignored. Finally, if Expression Language is used, evaluation will be performed only once per batch of generated FlowFiles<br/>**Supports Expression Language: true** |

### Relationships

| Name    | Description                            |
|---------|----------------------------------------|
| success | success operational on the flow record |


## GetFileRs

### Description

Creates FlowFiles from files in a directory. MiNiFi will ignore files for which it doesn't have read permissions.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                    | Default Value | Allowable Values | Description                                                                                                                                                |
|-------------------------|---------------|------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Input Directory**     |               |                  | The input directory from which to pull files<br/>**Supports Expression Language: true**                                                                    |
| Polling Interval        |               |                  | Indicates how long to wait before performing a directory listing                                                                                           |
| Recurse Subdirectories  | true          | true<br/>false   | Indicates whether or not to pull files from subdirectories                                                                                                 |
| Keep Source File        | false         | true<br/>false   | If true, the file is not deleted after it has been copied to the Content Repository                                                                        |
| Minimum File Age        |               |                  | The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored |
| Maximum File Age        |               |                  | The maximum age that a file must be in order to be pulled;  any file older than this amount of time (according to last modification date) will be ignored  |
| Minimum File Size       |               |                  | The minimum size that a file can be in order to be pulled                                                                                                  |
| Maximum File Size       |               |                  | The maximum size that a file can be in order to be pulled                                                                                                  |
| **Ignore Hidden Files** | true          | true<br/>false   | Indicates whether or not hidden files should be ignored                                                                                                    |
| **Batch Size**          | 10            |                  | The maximum number of files to pull in each iteration                                                                                                      |

### Relationships

| Name    | Description                                  |
|---------|----------------------------------------------|
| success | FlowFiles are transferred here after logging |

### Output Attributes

| Attribute     | Relationship | Description                                                                                                                         |
|---------------|--------------|-------------------------------------------------------------------------------------------------------------------------------------|
| absolute.path | success      | The full/absolute path from where a file was picked up. The current 'path' attribute is still populated, but may be a relative path |
| filename      | success      | The filename is set to the name of the file on disk                                                                                 |


## KamikazeProcessorRs

### Description

This processor can fail or panic in on_trigger and on_schedule calls based on configuration. Only for testing purposes.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                      | Default Value | Allowable Values                                                                              | Description                              |
|---------------------------|---------------|-----------------------------------------------------------------------------------------------|------------------------------------------|
| **On Schedule Behaviour** | ReturnOk      | ReturnErr<br/>ReturnOk<br/>GetNotRegisteredProperty<br/>GetInvalidControllerService<br/>Panic | What to do during the on_schedule method |
| **On Trigger Behaviour**  | ReturnOk      | ReturnErr<br/>ReturnOk<br/>GetNotRegisteredProperty<br/>GetInvalidControllerService<br/>Panic | What to do during the on_trigger method  |

### Relationships

| Name    | Description          |
|---------|----------------------|
| success | success relationship |


## LogAttributeRs

### Description

Logs attributes of flow files in the MiNiFi application log.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                  | Default Value | Allowable Values                                                 | Description                                                                                                                                                    |
|-----------------------|---------------|------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Log Level**         | Info          | Trace<br/>Debug<br/>Info<br/>Warn<br/>Error<br/>Critical<br/>Off | The Log Level to use when logging the Attributes                                                                                                               |
| Attributes to Log     |               |                                                                  | A comma-separated list of Attributes to Log. If not specified, all attributes will be logged.                                                                  |
| Attributes to Ignore  |               |                                                                  | A comma-separated list of Attributes to ignore. If not specified, no attributes will be ignored.                                                               |
| **Log Payload**       | false         | true<br/>false                                                   | If true, the FlowFile's payload will be logged, in addition to its attributes. Otherwise, just the Attributes will be logged.                                  |
| Log Prefix            |               |                                                                  | Log prefix appended to the log lines. It helps to distinguish the output of multiple LogAttribute processors.                                                  |
| **FlowFiles To Log**  | 1             |                                                                  | Number of flow files to log. If set to zero all flow files will be logged. Please note that this may block other threads from running if not used judiciously. |
| **Hexencode Payload** | false         | true<br/>false                                                   | If true, the FlowFile's payload will be logged in a hexencoded format                                                                                          |

### Relationships

| Name    | Description                                  |
|---------|----------------------------------------------|
| success | FlowFiles are transferred here after logging |


## LoremIpsumCSUser

### Description

Processor to test Controller Service API

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                               | Default Value | Allowable Values  | Description                                |
|------------------------------------|---------------|-------------------|--------------------------------------------|
| **Lorem Ipsum Controller Service** |               |                   | Name of the lorem ipsum controller service |
| **Write Method**                   | Buffer        | Buffer<br/>Stream | Which API to test                          |

### Relationships

| Name    | Description                  |
|---------|------------------------------|
| success | All flowfile are routed here |


## PutFileRs

### Description

Writes the contents of a FlowFile to the local file system.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                             | Default Value | Allowable Values            | Description                                                                                                                                                                          |
|----------------------------------|---------------|-----------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Directory**                    | .             |                             | The output directory to which to put files<br/>**Supports Expression Language: true**                                                                                                |
| **Conflict Resolution Strategy** | fail          | fail<br/>replace<br/>ignore | Indicates what should happen when a file with the same name already exists in the output directory                                                                                   |
| **Create Missing Directories**   | true          | true<br/>false              | If true, then missing destination directories will be created. If false, flowfiles are penalized and sent to failure.                                                                |
| Maximum File Count               |               |                             | Specifies the maximum number of files that can exist in the output directory                                                                                                         |
| Permissions                      |               |                             | Sets the permissions on the output file to the value of this attribute. Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems.                                |
| Directory Permissions            |               |                             | Sets the permissions on the directories being created if 'Create Missing Directories' property is set. Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems. |

### Relationships

| Name    | Description                                                                       |
|---------|-----------------------------------------------------------------------------------|
| success | Flowfiles that are successfully written to a file are routed to this relationship |
| failure | Failed files (conflict, write failure, etc.) are transferred to failure           |


## DummyControllerService

### Description

Dummy Controller Service

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
|------|---------------|------------------|-------------|


## LoremIpsumControllerService

### Description

Simple Rusty Controller Service to test API

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name       | Default Value | Allowable Values | Description                                                 |
|------------|---------------|------------------|-------------------------------------------------------------|
| **Length** | 25            |                  | How many words to generate<br/>**Sensitive Property: true** |
