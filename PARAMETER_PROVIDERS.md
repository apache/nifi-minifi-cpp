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

- [EnvironmentVariableParameterProvider](#EnvironmentVariableParameterProvider)


## EnvironmentVariableParameterProvider

### Description

Fetches parameters from environment variables.

This provider generates a single Parameter Context with the name specified in the `Parameter Group Name` property, if it doesn't exist yet. The parameters generated match the name of the environment variables that are included.

### Properties

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name                                        | Default Value | Allowable Values                                       | Description                                                                                                                                                                               |
|---------------------------------------------|---------------|--------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Sensitive Parameter Scope**               | none          | none<br/>all<br/>selected                              | Define which parameters are considered sensitive. If 'selected' is chosen, the 'Sensitive Parameter List' property must be set.                                                           |
| Sensitive Parameter List                    |               |                                                        | List of sensitive parameters, if 'Sensitive Parameter Scope' is set to 'selected'.                                                                                                        |
| **Environment Variable Inclusion Strategy** | Include All   | Include All<br/>Comma-Separated<br/>Regular Expression | Indicates how Environment Variables should be included                                                                                                                                    |
| Include Environment Variables               |               |                                                        | Specifies comma separated environment variable names or regular expression (depending on the Environment Variable Inclusion Strategy) that should be used to fetch environment variables. |
| **Parameter Group Name**                    |               |                                                        | The name of the parameter group that will be fetched. This indicates the name of the Parameter Context that may receive the fetched parameters.                                           |
