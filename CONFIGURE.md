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

# Apache NiFi -  Configuring MiNiFi - C++

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Configuring](#configuring)
  - [Parameter Contexts](#parameter-contexts)
  - [Parameter Providers](#parameter-providers)
  - [Configuring flow configuration format](#configuring-flow-configuration-format)
  - [Scheduling strategies](#scheduling-strategies)
  - [Configuring encryption for flow configuration](#configuring-encryption-for-flow-configuration)
  - [Configuring additional sensitive properties](#configuring-additional-sensitive-properties)
  - [Backup previous flow configuration on flow update](#backup-previous-flow-configuration-on-flow-update)
  - [Set number of flow threads](#set-number-of-flow-threads)
  - [OnTrigger runtime alert](#ontrigger-runtime-alert)
  - [Event driven processor time slice](#event-driven-processor-time-slice)
  - [Administrative yield duration](#administrative-yield-duration)
  - [Bored yield duration](#bored-yield-duration)
  - [Graceful shutdown period](#graceful-shutdown-period)
  - [FlowController drain timeout](#flowcontroller-drain-timeout)
  - [SiteToSite Security Configuration](#sitetosite-security-configuration)
  - [HTTP SiteToSite Configuration](#http-sitetosite-configuration)
  - [HTTP SiteToSite Proxy Configuration](#http-sitetosite-proxy-configuration)
  - [Command and Control Configuration](#command-and-control-configuration)
  - [State Storage](#state-storage)
  - [Configuring Repositories](#configuring-repositories)
  - [Configuring Volatile Repositories](#configuring-volatile-repositories)
  - [Configuring Repository storage locations](#configuring-repository-storage-locations)
  - [Configuring compression for rocksdb database](#configuring-compression-for-rocksdb-database)
  - [Configuring compaction for rocksdb database](#configuring-compaction-for-rocksdb-database)
  - [Configuring synchronous or asynchronous writes for RocksDB content repository](#configuring-synchronous-or-asynchronous-writes-for-rocksdb-content-repository)
  - [Configuring checksum verification for RocksDB reads](#configuring-checksum-verification-for-rocksdb-reads)
  - [Global RocksDB options](#global-rocksdb-options)
    - [Shared database](#shared-database)
  - [Configuring Repository encryption](#configuring-repository-encryption)
    - [Mixing encryption with shared backend](#mixing-encryption-with-shared-backend)
  - [Configuring Repository Cleanup](#configuring-repository-cleanup)
    - [Caveats](#caveats)
  - [Configuring provenance repository storage](#configuring-provenance-repository-storage)
  - [Provenance Reporter](#provenance-reporter)
  - [REST API access](#rest-api-access)
  - [UID generation](#uid-generation)
  - [Asset directory](#asset-directory)
  - [Controller Services](#controller-services)
  - [Linux Power Manager Controller Service](#linux-power-manager-controller-service)
  - [MQTT Controller service](#mqtt-controller-service)
  - [Network Prioritizer Controller Service](#network-prioritizer-controller-service)
  - [Disk space watchdog](#disk-space-watchdog)
  - [Extension configuration](#extension-configuration)
  - [Python processors](#python-processors)
  - [Enabling FIPS support](#enabling-fips-support)
    - [Generating the fipsmodule.cnf file automatically](#generating-the-fipsmodulecnf-file-automatically)
    - [Generating the fipsmodule.cnf file manually](#generating-the-fipsmodulecnf-file-manually)
- [Log configuration](#log-configuration)
  - [Log appenders](#log-appenders)
  - [Log levels](#log-levels)
  - [Log pattern](#log-pattern)
  - [Log compression](#log-compression)
  - [Shortening log messages](#shortening-log-messages)
- [Recommended Antivirus Exclusions](#recommended-antivirus-exclusions)

## Linux Installation types

### Self-contained installation (from a .tar.gz archive)
The `MINIFI_HOME` environment variable should point to the installation directory, if `MINIFI_HOME` is not defined, MiNiFi will try to infer it from binary's location.

### Filesystem Hierarchy Standard installation (from .rpm package)
The `MINIFI_INSTALLATION_TYPE` environment variable should be set to `FHS`. If `MINIFI_HOME` and `MINIFI_INSTALLATION_TYPE` are both undefined but the binary is in the `/usr/bin` directory it will try to run as an FHS application.


## Configuring
The 'conf' directory in the root contains a template config.yml document.

This is partly compatible with the format used with the Java MiNiFi application. MiNiFi C++ is currently compatible with version 1 of the MiNiFi YAML schema.
Additional information on the YAML format for the config.yml and schema versioning can be found in the [MiNiFi System Administrator Guide](https://nifi.apache.org/minifi/system-admin-guide.html).

MiNiFi Toolkit Converter (version 0.0.1 - schema version 1) is considered as deprecated from MiNiFi C++ 0.7.0. It was to aid in creating a flow configuration from a generated template exported from a NiFi instance. The MiNiFi Toolkit Converter tool can be downloaded from http://nifi.apache.org/minifi/download.html under the `MiNiFi Toolkit Binaries` section.  Information on its usage is available at https://nifi.apache.org/minifi/minifi-toolkit.html. Using the toolkit is no longer supported and maintained.

It's recommended to create your configuration in YAML format or configure the agent via Command and Control protocol (see below)

```yaml
Flow Controller:
  id: 471deef6-2a6e-4a7d-912a-81cc17e3a205
  name: MiNiFi Flow

Processors:
  - name: GetFile
    id: 471deef6-2a6e-4a7d-912a-81cc17e3a206
    class: org.apache.nifi.processors.standard.GetFile
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 1 sec
    penalization period: 30 sec
    yield period: 1 sec
    run duration nanos: 0
    auto-terminated relationships list:
    Properties:
      Input Directory: /tmp/getfile
      Keep Source File: true

Connections:
  - name: TransferFilesToRPG
    id: 471deef6-2a6e-4a7d-912a-81cc17e3a207
    source name: GetFile
    source id: 471deef6-2a6e-4a7d-912a-81cc17e3a206
    source relationship name: success
    destination id: 471deef6-2a6e-4a7d-912a-81cc17e3a204
    max work queue size: 0
    max work queue data size: 1 MB
    flowfile expiration: 60 sec
    drop empty: false

Remote Processing Groups:
  - name: NiFi Flow
    id: 471deef6-2a6e-4a7d-912a-81cc17e3a208
    url: http://localhost:8080/nifi
    timeout: 30 secs
    yield period: 10 sec
    Input Ports:
      - id: 471deef6-2a6e-4a7d-912a-81cc17e3a204
        name: From Node A
        max concurrent tasks: 1
        Properties:
```

Besides YAML configuration format, MiNiFi C++ also supports JSON configuration. To see different uses cases in both formats, please refer to the [examples page](examples/README.md) for flow config examples.

**NOTE:** Make sure to specify id for each component (Processor, Connection, Controller, RPG etc.) to make sure that Apache MiNiFi C++ can reload the state after a process restart. The id should be unique in the flow configuration.

### Parameter Contexts

Processor properties in flow configurations can be parameterized using parameters defined in parameter contexts. Flow configurations can define parameter contexts that define parameter-value pairs to be reused in the flow configuration as per the following rules:
 - Parameters within a process group can only be used if the parameter context is assigned to that process group.
 - Each process group can be assigned only one parameter context.
 - Parameter contexts can be assigned to multiple process groups.
 - The assigned parameter context has the scope of the process group, but not its child process groups.
 - The parameters can be used in the flow configuration by using the `#{parameterName}` syntax for processor properties.
 - Only alpha-numeric characters (a-z, A-Z, 0-9), hyphens ( - ), underscores ( _ ), periods ( . ), and spaces are allowed in parameter name.
 - `#` character can be used to escape the parameter syntax. E.g. if the `parameterName` parameter's value is `xxx` then `#{parameterName}` will be replaced with `xxx`, `##{parameterName}` will be replaced with `#{parameterName}`, and `#####{parameterName}` will be replaced with `##xxx`.
 - Sensitive parameters can only be assigned to sensitive properties and non-sensitive parameters can only be assigned to non-sensitive properties.

Parameter contexts can be inherited from multiple other parameter contexts. The order of the parameter inheritance matters in the way that if a parameter is present in multiple parameter contexts, the parameter value assigned to the first parameter in the inheritance order will be used. No circular inheritance is allowed.

An example for using parameters in a JSON configuration file:

```json
{
    "parameterContexts": [
        {
            "identifier": "235e6b47-ea22-45cd-a472-545801db98e6",
            "name": "common-parameter-context",
            "description": "Common parameter context",
            "parameters": [
                {
                    "name": "common_timeout",
                    "description": "Common timeout seconds",
                    "sensitive": false,
                    "value": "30"
                }
            ],
        },
        {
            "identifier": "804e6b47-ea22-45cd-a472-545801db98e6",
            "name": "root-process-group-context",
            "description": "Root process group parameter context",
            "parameters": [
                {
                    "name": "tail_base_dir",
                    "description": "Base dir of tailed files",
                    "sensitive": false,
                    "value": "/tmp/tail/file/path"
                }
            ],
            "inheritedParameterContexts": ["common-parameter-context"]
        }
    ],
    "rootGroup": {
        "name": "MiNiFi Flow",
        "processors": [
            {
                "name": "Tail test_file1.log",
                "identifier": "83b58f9f-e661-4634-96fb-0e82b92becdf",
                "type": "org.apache.nifi.minifi.processors.TailFile",
                "schedulingStrategy": "TIMER_DRIVEN",
                "schedulingPeriod": "1000 ms",
                "properties": {
                    "File to Tail": "#{tail_base_dir}/test_file1.log"
                }
            },
            {
                "name": "Tail test_file2.log",
                "identifier": "8a772a10-7c34-48e7-b152-b1a32c5db83e",
                "type": "org.apache.nifi.minifi.processors.TailFile",
                "schedulingStrategy": "TIMER_DRIVEN",
                "schedulingPeriod": "1000 ms",
                "properties": {
                    "File to Tail": "#{tail_base_dir}/test_file2.log"
                }
            }
        ],
        "parameterContextName": "root-process-group-context"
    }
}
```

An example for using parameters in a YAML configuration file:

```yaml
MiNiFi Config Version: 3
Flow Controller:
  name: MiNiFi Flow
Parameter Contexts:
  - id: 235e6b47-ea22-45cd-a472-545801db98e6
    name: common-parameter-context
    description: Common parameter context
    Parameters:
    - name: common_timeout
      description: 'Common timeout seconds'
      sensitive: false
      value: 30
  - id: 804e6b47-ea22-45cd-a472-545801db98e6
    name: root-process-group-context
    description: Root process group parameter context
    Parameters:
    - name: tail_base_dir
      description: 'Base dir of tailed files'
      sensitive: false
      value: /tmp/tail/file/path
    Inherited Parameter Contexts:
    - common-parameter-context
Processors:
- name: Tail test_file1.log
  id: 83b58f9f-e661-4634-96fb-0e82b92becdf
  class: org.apache.nifi.minifi.processors.TailFile
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1000 ms
  Properties:
    File to Tail: "#{tail_base_dir}/test_file1.log"
- name: Tail test_file2.log
  id: 8a772a10-7c34-48e7-b152-b1a32c5db83e
  class: org.apache.nifi.minifi.processors.TailFile
  scheduling strategy: TIMER_DRIVEN
  scheduling period: 1000 ms
  Properties:
    File to Tail: "#{tail_base_dir}/test_file2.log"
Parameter Context Name: root-process-group-context
```

### Parameter Providers

Parameter contexts can be generated by Parameter Providers. Parameter Providers can be added to the flow configuration, after which parameter contexts and parameters generated by these providers can be referenced in the properties. The parameter contexts generated are persisted in the flow configuration file and are only regenerated on MiNiFi C++ restart if the context is removed from the flow configuration. Other parameter contexts can be also inherited from provider generated parameter contexts.

The following properties can be set for all parameter providers:

- `Sensitive Parameter Scope`: This property can be set to `none`, `selected` or `all`. If set to `All`, all parameters generated by the provider will be marked as sensitive. If set to `none`, all parameters generated by the provider will be marked as non-sensitive. If set to `selected`, the `Sensitive Parameter List` property should be set to a list of parameter names that should be marked as sensitive.
- `Sensitive Parameter List`: This property should be set to a comma-separated list of parameter names that should be marked as sensitive. This property is only used if the `Sensitive Parameter Scope` property is set to `selected`.
- `Reload Values On Restart`: If set to `true`, parameter contexts and their values generated by the parameter provider will be reloaded on MiNiFi C++ restart even if the parameter context already exists in the flow configuration.

An example for using parameter providers in a JSON configuration file:

```json
{
    "parameterProviders": [
        {
            "identifier": "d26ee5f5-0192-1000-0482-4e333725e089",
            "name": "EnvironmentVariableParameterProvider",
            "type": "EnvironmentVariableParameterProvider",
            "properties": {
                "Parameter Group Name": "environment-variable-parameter-context",
                "Environment Variable Inclusion Strategy": "Regular Expression",
                "Include Environment Variables": "INPUT_.*"
            }
        }
    ],
    "rootGroup": {
        "name": "MiNiFi Flow",
        "processors": [
            {
                "identifier": "00000000-0000-0000-0000-000000000001",
                "name": "MyProcessor",
                "type": "org.apache.nifi.processors.GetFile",
                "schedulingStrategy": "TIMER_DRIVEN",
                "schedulingPeriod": "3 sec",
                "properties": {
                    "Input Directory": "#{INPUT_DIR}"
                }
            }
        ],
        "parameterContextName": "environment-variable-parameter-context"
    }
}
```

The same example in YAML configuration file:

```yaml
MiNiFi Config Version: 3
Flow Controller:
name: MiNiFi Flow
Parameter Providers:
  - id: d26ee5f5-0192-1000-0482-4e333725e089
    name: EnvironmentVariableParameterProvider
    type: EnvironmentVariableParameterProvider
    Properties:
      Parameter Group Name: environment-variable-parameter-context
      Environment Variable Inclusion Strategy: Regular Expression
      Include Environment Variables: INPUT_.*
Processors:
  - name: MyProcessor
    id: 00000000-0000-0000-0000-000000000001
    class: org.apache.nifi.processors.GetFile
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 3 sec
    Properties:
      Input Directory: "#{INPUT_DIR}"
Parameter Context Name: environment-variable-parameter-context
```

In the above example, the `EnvironmentVariableParameterProvider` is used to generate a parameter context with the name `environment-variable-parameter-context` that includes all environment variables starting with `INPUT_`. The generated parameter context is assigned to the root process group and the `INPUT_DIR` environment variable is used in the `Input Directory` property of the `MyProcessor` processor which is a generated parameter in the `environment-variable-parameter-context` parameter context.

After the parameter contexts are generated successfully, the parameter contexts are persisted in the flow configuration file, which looks like this for the above example:

```json
{
    "parameterProviders": [
        {
            "identifier": "d26ee5f5-0192-1000-0482-4e333725e089",
            "name": "EnvironmentVariableParameterProvider",
            "type": "EnvironmentVariableParameterProvider",
            "properties": {
                "Parameter Group Name": "environment-variable-parameter-context",
                "Environment Variable Inclusion Strategy": "Regular Expression",
                "Include Environment Variables": "INPUT_.*"
            }
        }
    ],
    "rootGroup": {
        "name": "MiNiFi Flow",
        "processors": [
            {
                "identifier": "00000000-0000-0000-0000-000000000001",
                "name": "MyProcessor",
                "type": "org.apache.nifi.processors.GetFile",
                "schedulingStrategy": "TIMER_DRIVEN",
                "schedulingPeriod": "3 sec",
                "properties": {
                    "Input Directory": "#{INPUT_DIR}"
                }
            }
        ],
        "parameterContextName": "environment-variable-parameter-context"
    },
    "parameterContexts": [
        {
            "identifier": "a48df754-a0f4-11ef-ae56-10f60a596f64",
            "name": "environment-variable-parameter-context",
            "parameterProvider": "d26ee5f5-0192-1000-0482-4e333725e089",
            "parameters": [
                {
                    "name": "INPUT_DIR",
                    "description": "",
                    "sensitive": false,
                    "provided": true,
                    "value": "/tmp/input/"
                }
            ]
        }
    ]
}
```

To see the full list of available parameter providers and their properties, please refer to the [Parameter Providers documentation](PARAMETER_PROVIDERS.md).

### Configuring flow configuration format

MiNiFi supports YAML and JSON configuration formats. The desired configuration format can be set in the minifi.properties file, but it is automatically identified by default. The default value is `adaptiveconfiguration`, but we can force to use YAML with the `yamlconfiguration` value.

    # in minifi.properties
    nifi.flow.configuration.class.name=adaptiveconfiguration

### Scheduling strategies
Currently Apache NiFi MiNiFi C++ supports TIMER_DRIVEN, EVENT_DRIVEN, and CRON_DRIVEN. TIMER_DRIVEN uses periods to execute your processor(s) at given intervals.
The EVENT_DRIVEN strategy awaits for data be available or some other notification mechanism to trigger execution. EVENT_DRIVEN is the recommended strategy for non-source processors.
CRON_DRIVEN executes at the desired intervals based on the CRON periods. Apache NiFi MiNiFi C++ supports standard CRON expressions ( */5 * * * * ).

### Configuring encryption for flow configuration

To encrypt flow configuration set the following property to true.

    # in minifi.properties
    nifi.flow.configuration.encrypt=true

### Configuring additional sensitive properties

It is possible to set a comma seperated list of encrypted configuration options beyond the default sensitive property list.

    # in minifi.properties
    nifi.sensitive.props.additional.keys=nifi.flow.configuration.file, nifi.rest.api.password

### Backup previous flow configuration on flow update

It is possible to backup the previous flow configuration file with `.bak` extension in case of a flow update (e.g. through C2 or controller socket protocol).

    # in minifi.properties
    nifi.flow.configuration.backup.on.update=true

### Set number of flow threads

The number of threads used by the flow scheduler can be set in the MiNiFi configuration. The default value is 5.

    # in minifi.properties
    nifi.flow.engine.threads=5

### OnTrigger runtime alert

MiNiFi writes warning logs in case a processor has been running for too long. The period for these alerts can be set in the configuration file with the default being 5 seconds.

    # in minifi.properties
    nifi.flow.engine.alert.period=5 sec

### Event driven processor time slice

The flow scheduler can be configured how much time it should allocate at maximum for event driven processors. The processor is triggered until it has work to do, but no more than the configured time slice. The default value is 500 milliseconds.

    # in minifi.properties
    nifi.flow.engine.event.driven.time.slice=500 millis

### Administrative yield duration

In case an uncaught exception is thrown while running a processor, the processor will yield for the configured administrative yield time. The default yield duration is 30 seconds.

    # in minifi.properties
    nifi.administrative.yield.duration=30 sec

### Bored yield duration

If a processor is triggered but has no work available, it will yield for the configured bored yield time. The default yield duration is 100 milliseconds.

    # in minifi.properties
    nifi.bored.yield.duration=100 millis

### FlowController drain timeout

When the flow is stopped, either because of a flow update from C2, a restart command from C2, or because MiNiFi is stopped by the operating system,
MiNiFi stops all source processors (processors without incoming connections) first. Next, it waits for all connection queues to become empty, but
at most the amount of time set in the

    # in minifi.properties
    nifi.flowcontroller.drain.timeout=5 sec

property. The default value is zero, i.e., no wait. Finally, it shuts down the remaining processors. If there are flow files left in some connection
queues after the drain timeout, they will be discarded.

### Graceful shutdown period

When MiNiFi is stopped or restarted, either because of a restart command from C2, or because MiNiFi is stopped by the operating system,
it may wait for connection queues to become empty if `nifi.flowcontroller.drain.timeout` is set in `minifi.properties`. You can limit this wait
to a shorter time by setting the

    # in minifi.properties
    nifi.flowcontroller.graceful.shutdown.period=2 sec

property. By default, the graceful shutdown period property is not set, which means the wait is only limited by the drain timeout property.

### SiteToSite Security Configuration

    # in minifi.properties

    # enable tls
    nifi.remote.input.secure=true

    # if you want to enable client certificate base authorization
    nifi.security.need.ClientAuth=true
    # setup the client certificate and private key PEM files
    nifi.security.client.certificate=./conf/client.pem
    nifi.security.client.private.key=./conf/client.pem
    # setup the client private key passphrase file
    nifi.security.client.pass.phrase=./conf/password
    # setup the client CA certificate file
    nifi.security.client.ca.certificate=./conf/nifi-cert.pem

    # if you do not want to enable client certificate base authorization
    nifi.security.need.ClientAuth=false

It can also be configured to use the system certificate store.

    # in minifi.properties
    nifi.security.use.system.cert.store=true

Windows specific certificate options with the following default values:

    # in minifi.properties
    nifi.security.windows.cert.store.location=LocalMachine
    nifi.security.windows.server.cert.store=ROOT
    nifi.security.windows.client.cert.store=MY

    # The CN that the client certificate is required to match; default: use the first available client certificate in the store
    # nifi.security.windows.client.cert.cn=

    # Comma-separated list of enhanced key usage values that the client certificate is required to have
    nifi.security.windows.client.cert.key.usage=Client Authentication

You have the option of specifying an SSL Context Service definition for the RPGs instead of the properties above.
This will link to a corresponding SSL Context service defined in the flow.

To do this specify the SSL Context Service Property in your RPGs and link it to
a defined controller service. If you do not take this approach the options, above, will be used
for TCP and secure HTTPS communications.

    Remote Processing Groups:
    - name: NiFi Flow
      id: 2438e3c8-015a-1000-79ca-83af40ec1998
      url: http://127.0.0.1:8080/nifi
      timeout: 30 secs
      yield period: 5 sec
      Input Ports:
          - id: 2438e3c8-015a-1000-79ca-83af40ec1999
            name: fromnifi
            max concurrent tasks: 1
            Properties:
                SSL Context Service: SSLServiceName
      Output Ports:
          - id: ac82e521-015c-1000-2b21-41279516e19a
            name: tominifi
            max concurrent tasks: 2
            Properties:
                SSL Context Service: SSLServiceName
    Controller Services:
    - name: SSLServiceName
      id: 2438e3c8-015a-1000-79ca-83af40ec1974
      class: SSLContextService
      Properties:
          Client Certificate: <client cert path>
          Private Key: < private key path >
          Passphrase: <passphrase path or passphrase>
          CA Certificate: <CA cert path>

If the SSL certificates are not provided with an absolute path or cannot be found on the given relative path, MiNiFi will try to find them on the default path provided in the configuration file.

    # in minifi.properties

    # default minifi resource path
    nifi.default.directory=/path/to/cert/files/


### HTTP SiteToSite Configuration
To enable HTTPSiteToSite for a remote process group.
    Remote Processing Groups:
    - name: NiFi Flow
      transport protocol: HTTP

### HTTP SiteToSite Proxy Configuration
To enable HTTP Proxy for a remote process group.

    Remote Processing Groups:
    - name: NiFi Flow
      transport protocol: HTTP
      proxy host: localhost
      proxy port: 8888
      proxy user:
      proxy password:

### Command and Control Configuration
Please see the [C2 readme](C2.md) for more informatoin

### State Storage

State storage is used for keeping the state of stateful processors like TailFile. This is done using RocksDB database, but can be configured to use a different state storage with custom options.

The default location of the RocksDB local state storage is the `corecomponentstate` directory under the MiNiFi root directory. This can be reconfigured if other directory is preferred.

    # in minifi.properties
    nifi.state.storage.local.path=/var/tmp/minifi-state/

To have a custom state storage one option is to configure it in the flow configuration file and set the created controller in the minifi.properties file.

    # in config.yml
    Controller Services:
    - name: testcontroller
      id: 2438e3c8-015a-1000-79ca-83af40ec1994
      class: PersistentMapStateStorage
      Properties:
        Auto Persistence Interval:
            - value: 0 sec
        Always Persist:
            - value: true
        File:
            - value: state.txt

    # in minifi.properties
    nifi.state.storage.local=2438e3c8-015a-1000-79ca-83af40ec1994

Another option to define a state storage is to use the following properties in the minifi.properties file.

    # in minifi.properties
    nifi.state.storage.local.class.name=PersistentMapStateStorage
    nifi.state.storage.local.always.persist=true
    nifi.state.storage.local.auto.persistence.interval=0 sec


### Configuring Repositories

Apache MiNiFi C++ uses three repositories similarly to Apache NiFi:
- The Flow File Repository holds the metadata (like flow file attributes) of the flow files.
- The Content Repository holds the content of the flow files.
- The Provenance Repository holds the history of the flow files.

The underlying implementation to use for these repositories can be configured in the minifi.properties file.

The Flow File Repository can be configured with the `nifi.flowfile.repository.class.name` property. If not specified, it uses the `FlowFileRepository` class by default, which stores the flow file metadata in a RocksDB database. Alternatively it can be configured to use a `NoOpRepository` for not keeping any state, flow files are only stored in memory while being transferred between processors.

    # in minifi.properties
    nifi.flowfile.repository.class.name=NoOpRepository  # VolatileFlowFileRepository can also be used which is an alias for NoOpRepository

The Content Repository can be configured with the `nifi.content.repository.class.name` property. If not specified, it uses the `DatabaseContentRepository` class by default, which persists the content in a RocksDB database. `DatabaseContentRepository` is also the default value specified in the minifi.properties file. Alternatively it can be configured to use a `VolatileContentRepository` that keeps the state in memory (so the state gets lost upon restart), or the `FileSystemRepository` to keep the state in regular files.

**NOTE:** RocksDB database has a limit of 4GB for the size of a database object. Due to this if you expect to process larger flow files than 4GB you should use the `FileSystemRepository`. The downside of using `FileSystemRepository` is that it does not have the transactional guarantees of the RocksDB repository implementation.

    # in minifi.properties
    nifi.content.repository.class.name=FileSystemRepository

During startup, MiNiFi checks if the flowfiles and their respective content are in good health (corruption can rarely occur due to ungraceful shutdowns) and filters out these corrupt flowfiles.
This can slow down startup if there is a significant number of flowfiles. This health check can be disabled by setting `nifi.flowfile.repository.check.health` to `false`


The Provenance Repository can be configured with the `nifi.provenance.repository.class.name` property. If not specified, it uses the `ProvenanceRepository` class by default, which persists the provenance events in a RocksDB database. Alternatively it can be configured to use a `VolatileProvenanceRepository` that keeps the state in memory (so the state gets lost upon restart), or the `NoOpRepository` to not keep track of the provenance events. By default we do not keep track of the provenance data, so `NoOpRepository` is the value specified in the default minifi.properties file.

    # in minifi.properties
    nifi.flowfile.repository.class.name=NoOpRepository


### Configuring Volatile Repositories
As stated before each of the repositories can be configured to be volatile (state kept in memory and flushed upon restart) or persistent. Volatile provenance repository also has some additional options, that can be specified in the following ways:

    # in minifi.properties
    # For Volatile Repositories:
    nifi.flowfile.repository.class.name=VolatileFlowFileRepository  # alias for NoOpRepository in case of flowfile repository
    nifi.provenance.repository.class.name=VolatileProvenanceRepository
    nifi.content.repository.class.name=VolatileContentRepository

    # maximum number of entries to keep in memory
    nifi.volatile.repository.options.provenance.max.count=15000
    # maximum number of bytes to keep in memory, also limited by option above
    nifi.volatile.repository.options.provenance.max.bytes=7680 KB

**NOTE:** If the volatile provenance repository reaches the maximum number of entries, it will start to drop the oldest entries, and replace them with the new entries in round robin manner. Make sure to set the maximum number of entries to a reasonable value, so that the repository does not run out of memory. Volatile content and flowfile repositories do not have such limits, their size is only limited by the available system memory.

### Configuring Repository storage locations
Persistent repositories, such as the Flow File repository, use configurable paths to store data. The application detects its installation type at runtime and uses the appropriate default locations.

In a self-contained installation (from a .tar.gz archive), paths are relative to the application's root directory, which is typically configured using the ${MINIFI_HOME} environment variable. You may specify your own path in place of these defaults.

    # in minifi.properties
    nifi.provenance.repository.directory.default=${MINIFI_HOME}/provenance_repository
    nifi.flowfile.repository.directory.default=${MINIFI_HOME}/flowfile_repository
    nifi.database.content.repository.directory.default=${MINIFI_HOME}/content_repository

In a Filesystem Hierarchy Standard (FHS) installation (from an RPM package), the default location for all persistent repositories is /var/lib/nifi-minifi-cpp/

    # in minifi.properties
    nifi.provenance.repository.directory.default=/var/lib/nifi-minifi-cpp/provenance_repository
    nifi.flowfile.repository.directory.default=/var/lib/nifi-minifi-cpp/flowfile_repository
    nifi.database.content.repository.directory.default=/var/lib/nifi-minifi-cpp/content_repository


### Configuring compression for rocksdb database

Rocksdb has an option to set compression type for its database to use less disk space.
If content repository or flow file repository is set to use the rocksdb database as their storage, then we have the option to compress those repositories. On Unix operating systems `zlib`, `bzip2`, `zstd`, `lz4` and `lz4hc` compression types and on Windows `xpress` compression type is supported by MiNiFi C++. If the property is set to `auto` then `xpress` will be used on Windows, `zstd` on Unix operating systems. These options can be set in the minifi.properies file with the following properties:

     # in minifi.properties
     nifi.flowfile.repository.rocksdb.compression=zlib
     nifi.content.repository.rocksdb.compression=auto

### Configuring compaction for rocksdb database

Rocksdb has an option to run compaction at specific intervals not just when needed.

     # in minifi.properties
     nifi.flowfile.repository.rocksdb.compaction.period=2 min
     nifi.database.content.repository.rocksdb.compaction.period=2 min

### Configuring synchronous or asynchronous writes for RocksDB content repository

RocksDB has an option to set synchronous writes for its database, ensuring that the write operation does not return until the data being written has been pushed all the way to persistent storage. In MiNiFi C++, this is set to true by default to avoid any data loss, as according to the RocksDB documentation using non-sync writes can result in data loss in the event of a host machine crash. You can read more information about this option on the [RocksDB wiki page](https://github.com/facebook/rocksdb/wiki/Basic-Operations#synchronous-writes). If you prefer to use non-sync writes in your content repository for better write performance and can accept the possibility of the mentioned data loss, you can set this option to false.

    # in minifi.properties
    nifi.content.repository.rocksdb.use.synchronous.writes=true

### Configuring checksum verification for RocksDB reads

RocksDB has an option to verify checksums for its database reads. This option is set to false by default for better performance. If you prefer to enable checksum verification you can set this option to true.

    # in minifi.properties
    nifi.content.repository.rocksdb.read.verify.checksums=false
    nifi.flowfile.repository.rocksdb.read.verify.checksums=false
    nifi.provenance.repository.rocksdb.read.verify.checksums=false
    nifi.rocksdb.state.storage.read.verify.checksums=false

### Global RocksDB options

There are a few options for RocksDB that are set for all used RocksDB databases in MiNiFi:
    `create_if_missing` is set to `true`
    `use_direct_io_for_flush_and_compaction` is set to `true`
    `use_direct_reads` is set to `true`
    `keep_log_file_num` is set to `5`

Any RocksDB option can be set or overriden using the `nifi.global.rocksdb.options.` prefix in the minifi.properties file.

    # in minifi.properties
    nifi.global.rocksdb.options.keep_log_file_num=7
    nifi.global.rocksdb.options.atomic_flush=true

RocksDB options can also be overridden for a specific repository using the `nifi.<repository>.rocksdb.options.` prefix in the minifi.properties file.

    # in minifi.properties
    nifi.flowfile.repository.rocksdb.options.keep_log_file_num=3
    nifi.content.repository.rocksdb.options.atomic_flush=true
    nifi.provenance.repository.rocksdb.options.use_direct_reads=false
    nifi.state.storage.rocksdb.options.use_direct_io_for_flush_and_compaction=false

#### Shared database

It is also possible to use a single database to store multiple repositories with the `minifidb://` scheme.
This could help with migration and centralize agent state persistence. In the scheme the final path segment designates the
column family in the repository, while the preceding path indicates the directory the rocksdb database is
created into. E.g. in `minifidb:///home/user/minifi/agent_state/flowfile` a directory will be created at
`/home/user/minifi/agent_state` populated with rocksdb-specific content, and in that repository a logically
separate "subdatabase" is created under the name `"flowfile"`.

    # in minifi.properties
    nifi.flowfile.repository.directory.default=minifidb://${MINIFI_HOME}/agent_state/flowfile
    nifi.database.content.repository.directory.default=minifidb://${MINIFI_HOME}/agent_state/content
    nifi.state.storage.local.path=minifidb://${MINIFI_HOME}/agent_state/processor_states

We should not simultaneously use the same directory with and without the `minifidb://` scheme.
Moreover the `"default"` name is restricted and should not be used.


    # in minifi.properties
    nifi.flowfile.repository.directory.default=minifidb://${MINIFI_HOME}/agent_state/flowfile
    nifi.database.content.repository.directory.default=${MINIFI_HOME}/agent_state
    ^ error: using the same database directory without the "minifidb://" scheme
    nifi.state.storage.local.path=minifidb://${MINIFI_HOME}/agent_state/default
    ^ error: "default" is restricted

### Configuring Repository encryption

It is possible to provide rocksdb-backed repositories a key to request their
encryption.

    in conf/bootstrap.conf
    nifi.flowfile.repository.encryption.key=805D7B95EF44DC27C87FFBC4DFDE376DAE604D55DB2C5496DEEF5236362DE62E
    nifi.database.content.repository.encryption.key=
    # nifi.state.management.provider.local.encryption.key=

In the above configuration the first line will cause `FlowFileRepository` to use the specified `256` bit key.
The second line will trigger the generation of a random (`256` bit) key persisted back into `conf/bootstrap.conf`, which `DatabaseContentRepository` will then use for encryption.
(This way one can request encryption while not bothering with what key to use.)
Finally, as the last line is commented out, it will make the state manager use plaintext storage, and not trigger encryption.

#### Mixing encryption with shared backend

When multiple repositories use the same directory (as with `minifidb://` scheme) they should either be all plaintext or all encrypted with the same key.

### Configuring Repository Cleanup

When a flow file content is no longer needed we can specify the deletion strategy.

    # any value other than 0 enables garbage collection with the specified frequency
    # while a value of 0 sec triggers an immediate deletion as soon as the resource
    # is not needed
    # (the default value is 1 sec)
    nifi.database.content.repository.purge.period = 1 sec

 #### Caveats
 Systems that have limited memory must be cognizant of the options above. Limiting the max count for the number of entries limits memory consumption but also limits the number of events that can be stored. If you are limiting the amount of volatile content you are configuring, you may have excessive session rollback due to invalid stream errors that occur when a claim cannot be found.

 The content repository has a default option for "minimal.locking" set to true. This will attempt to use lock free structures. This may or may not be optimal as this requires additional additional searching of the underlying vector. This may be optimal for cases where max.count is not excessively high. In cases where object permanence is low within the repositories, minimal locking will result in better performance. If there are many processors and/or timing is such that the content repository fills up quickly, performance may be reduced. In all cases a locking cache is used to avoid the worst case complexity of O(n) for the content repository; however, this caching is more heavily used when "minimal.locking" is set to false.

### Configuring provenance repository storage

Provenance repository size buffer size and TTL can be configured when used with RocksDB. If not set it uses the available maximum RocksDB values.

    #in minifi.properties
    nifi.provenance.repository.max.storage.size=16 MB
    nifi.provenance.repository.max.storage.time=30 days

### Provenance Reporter

    Add Provenance Reporting to config.yml
    Provenance Reporting:
      scheduling strategy: TIMER_DRIVEN
      scheduling period: 1 sec
      url: http://localhost:8080/nifi
      port uuid: 471deef6-2a6e-4a7d-912a-81cc17e3a204
      batch size: 100

### REST API access

    Configure REST API user name and password
    nifi.rest.api.user.name=admin
    nifi.rest.api.password=password

    if you want to enable client certificate
    nifi.https.need.ClientAuth=true
    nifi.https.client.certificate=./conf/client.pem
    nifi.https.client.private.key=./conf/client.key
    nifi.https.client.pass.phrase=./conf/password
    nifi.https.client.ca.certificate=./conf/nifi-cert.pem

### UID generation

MiNiFi needs to generate many unique identifiers in the course of operations.  There are a few different uid implementations available that can be configured in minifi-uid.properties.

Implementation for uid generation can be selected using the uid.implementation property values:
1. time - use uuid_generate_time (default option if the file or property value is missing or invalid)
2. random - use uuid_generate_random
3. uuid_default - use uuid_generate (will attempt to use uuid_generate_random and fall back to uuid_generate_time if no high quality randomness is available)
4. minifi_uid - use custom uid algorthim

If minifi_uuid is selected MiNiFi will use a custom uid algorthim consisting of first N bits device identifier, second M bits as bottom portion of a timestamp where N + M = 64, the last 64 bits is an atomic incrementor.

This is faster than the random uuid generator and encodes the device id and a timestamp into every value, making tracing of flowfiles, etc easier.

It does require more configuration.  uid.minifi.device.segment.bits is used to specify how many bits at the beginning to reserve for the device identifier.  It also puts a limit on how many distinct devices can be used.  With the default of 16 bits, there are a maximum of 65,535 unique device identifiers that can be used.  The 48 bit timestamp won't repeat for almost 9,000 years.  With 24 bits for the device identifier, there are a maximum of 16,777,215 unique device identifiers and the 40 bit timestamp won't repeat for about 35 years.

Additionally, a unique hexadecimal uid.minifi.device.segment should be assigned to each MiNiFi instance.

### Asset directory

The location for downloaded assets is specified by the `nifi.asset.directory` agent property. The default path depends on the installation mode:

* In a **self-contained (TGZ)** installation, this defaults to **`${MINIFI_HOME}/asset`**.
* In a **system-wide FHS** installation, the default is **`/var/lib/nifi-minifi-cpp/asset`**.

The files referenced in the `.state` file in this directory are managed by the agent. They are deleted, updated, downloaded
using the asset sync c2 command. For the asset sync command to work, the c2 server must be made aware of the current state of the
managed assets by adding the `AssetInformation` entry to the `nifi.c2.root.classes` property.

Files and directories not referenced in the `.state` file are not directly controlled by the agent, although
it is possible to download an asset (triggered through the c2 protocol) into the asset directory instead.

### Asset resolution

Assets can be referenced in processor properties using the syntax `@{asset-id:<asset-id>}`.
When the flow is started, MiNiFi will resolve the asset reference to the actual file path in the asset directory.
The ID of the asset can be found in the `.state` file in the asset directory.

For example, if the `.state` file contains the following content:

    {
        "digest": "dd46a660e637d83a8cf5b97cb5a92ac0bf1aea54870ce915700861009357d4b710e3c4ef649e4e4c60ab77cb50482281e23864dccd753a33b03a32de9f475a5c",
        "assets": {
            "4658b1a5-012a-4c75-afdd-3f2e9c0955c1": {
                "path": "data.json",
                "url": "/c2-protocol/resource/4658b1a5-012a-4c75-afdd-3f2e9c0955c1"
            }
        }
    }

You can use the asset ID `4658b1a5-012a-4c75-afdd-3f2e9c0955c1` to reference the `data.json` file in the asset directory. This asset can be referenced in processor properties, such as the `File to Fetch` property of the `FetchFile` processor:

    Processors:
      - id: 25bdc712-444a-4fef-aebb-90dd982ddeb8
        name: FetchFile
        class: org.apache.nifi.minifi.processors.FetchFile
        scheduling strategy: TIMER_DRIVEN
        scheduling period: 1000 ms
        auto-terminated relationships list:
          - permission.denied
          - failure
          - not.found
        Properties:
          Completion Strategy: None
          File to Fetch: "@{asset-id:4658b1a5-012a-4c75-afdd-3f2e9c0955c1}"
          Log level when file not found: ERROR
          Log level when permission denied: ERROR
          Move Conflict Strategy: Rename

### Controller Services
 If you need to reference a controller service in your config.yml file, use the following template. In the example, below, ControllerServiceClass is the name of the class defining the controller Service. ControllerService1
 is linked to ControllerService2, and requires the latter to be started for ControllerService1 to start.

    Controller Services:
      - name: ControllerService1
        id: 2438e3c8-015a-1000-79ca-83af40ec1974
        class: ControllerServiceClass
        Properties:
          Property one: value
          Linked Services:
          - value: ControllerService2
      - name: ControllerService2
        id: 2438e3c8-015a-1000-79ca-83af40ec1992
        class: ControllerServiceClass
        Properties:

### Linux Power Manager Controller Service
  The linux power manager controller service can be configured to monitor the battery level and status ( discharging or charging ) via the following configuration.
  Simply provide the capacity path and status path along with your threshold for the trigger and low battery alarm and you can monitor your battery and throttle
  the threadpools within MiNiFi C++. Note that the name is identified must be ThreadPoolManager.

    Controller Services:
    - name: ThreadPoolManager
      id: 2438e3c8-015a-1000-79ca-83af40ec1888
      class: LinuxPowerManagerService
      Properties:
          Battery Capacity Path: /path/to/battery/capacity
          Battery Status Path: /path/to/battery/status
          Trigger Threshold: 90
          Low Battery Threshold: 50
          Wait Period: 500 ms

### MQTT Controller service
The MQTTController Service can be configured for MQTT connectivity and provide that capability to your processors when MQTT is built.

    Controller Services:
    - name: mqttservice
      id: 294491a38-015a-1000-0000-000000000001
      class: MQTTContextService
      Properties:
          Broker URI: localhost:1883
          Client ID: client ID
          Quality of Service: 2

### Network Prioritizer Controller Service
  The network prioritizer controller service can be configured to manage prioritizing and binding to specific network interfaces. Linked Services, can be used
  as a prioritized list to create a disjunction among multiple networking prioritizers. This allows you to create classes with different configurations that
  create multiple prioritizations. Max Throughput is the maximum throughput in bytes per second. Max Payload is the maximum number of bytes supported by that
  prioritizer. If a prioritizer is configured with the option "Default Prioritizer: true," then all socket communications will use that default prioritizer.

  In the configuration below there are two classes defined under "NetworkPrioritizerService", one class "NetworkPrioritizerService2" defines en0, and en1.
  If en0 is down at any point, then en1 will be given priority before resorting to en2 and en3 of  "NetworkPrioritizerService3". If the throughput for
  "NetworkPrioritizerService2" exceeds the defined throughput or the max payload of 1024, then "NetworkPrioritizerService3" will be used. If Max Payload and
  Max Throughput are not defined, then they will not be limiting factors.

  Since connection queues can't be re-prioritized, this can create a starvation problem if no connection is available. 
  The configuration is required to account for this.

   Controller Services:
   - name: NetworkPrioritizerService
     id: 2438e3c8-015a-1000-79ca-83af40ec1883
     class: NetworkPrioritizerService
     Properties:
         Linked Services: NetworkPrioritizerService2,NetworkPrioritizerService3
   - name: NetworkPrioritizerService2
     id: 2438e3c8-015a-1000-79ca-83af40ec1884
     class: NetworkPrioritizerService
     Properties:
         Network Controllers: en0,en1
         Max Throughput: 1,024,1024
         Max Payload: 1024
   - name: NetworkPrioritizerService3
     id: 2438e3c8-015a-1000-79ca-83af40ec1884
     class: NetworkPrioritizerService
     Properties:
         Network Controllers: en2,en3
         Max Throughput: 1,024,1024
         Max Payload: 1,024,1024

### Disk space watchdog #

Stops MiNiFi FlowController activity (excluding C2), when the available disk space on either of the repository volumes go below stop.threshold, checked every interval, then restarts when the available space on all repository volumes reach at least restart.threshold.

    # in minifi.properties
    minifi.disk.space.watchdog.enable=true
    minifi.disk.space.watchdog.interval=15 sec
    minifi.disk.space.watchdog.stop.threshold=100 MB
    minifi.disk.space.watchdog.restart.threshold=150 MB

### Extension configuration
To notify the agent which extensions it should load see [Loading extensions](Extensions.md#Loading extensions).

### Python processors
Please see the [Python Processors Readme](extensions/python/PYTHON.md).

### Enabling FIPS support

To enable FIPS support, and use MiNiFi C++ in FIPS compliant mode, there are a few steps that need to be taken before the application startup. First the following property needs to be set in the minifi.properties file:

    # in minifi.properties
    nifi.openssl.fips.support.enable=true

Before first starting the application, the fipsmodule.cnf needs to be generated. This can be done in two ways, either automatically or manually.

### Generating the `fipsmodule.cnf` File

#### Automatic Generation

If the application is started with the `nifi.openssl.fips.support.enable` property set to `true`, it will check for the `fipsmodule.cnf` file. If the file is not found, the application will attempt to generate it automatically before loading the OpenSSL configuration.

The location where the application looks for and generates the file depends on the installation type:
* **FHS Installation (RPM):** The application checks for `/etc/nifi-minifi-cpp/fips/fipsmodule.cnf`.
* **Self-Contained Installation (TGZ):** The application checks for `$MINIFI_HOME/fips/fipsmodule.cnf`.

If the automatic generation is successful, the application will start in FIPS mode.

#### Manual Generation

If automatic generation fails, or you need to create the file manually, follow the steps below.

#### For FHS Installations (RPM)

Run the bundled `openssl` binary located in the application's private library directory, and specify the absolute path for the output configuration file.
```bash
cd /usr/lib64/nifi-minifi-cpp/fips
./openssl fipsinstall -out /etc/nifi-minifi-cpp/fips/fipsmodule.cnf -module ./fips.so
```

#### For self-contained (TGZ) Unix installation
```bash
cd $MINIFI_HOME/fips
./openssl fipsinstall -out fipsmodule.cnf -module $MINIFI_HOME/fips/fips.so
```

#### For Windows
```dos
cd $MINIFI_HOME/fips
openssl.exe fipsinstall -out fipsmodule.cnf -module $MINIFI_HOME\fips\fips.dll
```


## Log configuration
By default, the application logs for Apache MiNiFi C++ can be found in the ${MINIFI_HOME}/logs/minifi-app.log file with default INFO level logging. The logger can be reconfigured in the ${MINIFI_HOME}/conf/minifi-log.properties file to use different output streams, log level, and output format.

### Log appenders
In the configuration file it is possible to define different output streams by defining what type of appenders (log sinks) should be used for logging. The following appenders are supported:
- rollingappender - logs written to this log sink are written to a file with a rolling policy
- stdout - logs written to this log sink are written to the standard output
- stderr - logs written to this log sink are written to the standard error
- syslog - logs written to this log sink are written to the syslog
- alert - logs written to this log sink are critical logs forwarded through HTTP as alerts
- nullappender - logs written to this log sink are discarded

Defining a new appender can be done in the format `appender.<appender name>=<appender type>` followed by defining its properties. For example here is how to define a rolling file appender:

    # in minifi-log.properties
    appender.rolling=rollingappender
    appender.rolling.directory=${MINIFI_HOME}/logs
    appender.rolling.file_name=minifi-app.log
    appender.rolling.max_files=3
    appender.rolling.max_file_size=5 MB

Here's an example of an alert appender with its available properties:

    # in minifi-log.properties
    appender.alert1=alert
    appender.alert1.url=<URL>
    appender.alert1.filter=<regex pattern to match logs against>
    appender.alert1.rate.limit=10 min
    appender.alert1.flush.period=5 s
    appender.alert1.batch.size=100 KB
    appender.alert1.buffer.limit=1 MB
    appender.alert1.level=TRACE

If you want to use SSL connection for the alert appender, you can set up the certificate configuration properties in the minifi.properties file:

    # setup the client certificate and private key PEM files
    nifi.security.client.certificate=./conf/client.pem
    nifi.security.client.private.key=./conf/client.pem
    # setup the client private key passphrase file
    nifi.security.client.pass.phrase=./conf/password
    # setup the client CA certificate file
    nifi.security.client.ca.certificate=./conf/nifi-cert.pem

### Log levels
After the appenders are defined you can set the log level and logging target for each of them. Appenders can be set to log everything on a specific log level, but you can also define some appenders to only write logs from specific namespaces or classes with different log levels. The log level can be set to one of the following values: OFF, TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL. The log levels can be set in the following way:

    # in minifi-log.properties
    # Set the rolling appender to log everything with INFO level
    logger.root=INFO,rolling
    # But only write logs from the org::apache::nifi::minifi::FlowController class with ERROR level
    logger.org::apache::nifi::minifi::core::logging::FlowController=ERROR,rolling

    # Write all error logs in the minifi namespace to the alert1 appender
    logger.org::apache::nifi::minifi=ERROR,alert1

    # Logs from the FlowController class should be written to the stderr and rolling appenders with DEBUG level
    logger.org::apache::nifi::minifi::FlowController=DEBUG,stderr,rolling

    # Log warnings and errors from the LoggerConfiguration class to all appenders
    logger.org::apache::nifi::minifi::core::logging::LoggerConfiguration=WARN

### Log pattern
The log pattern can be changed if needed using the `spdlog.pattern` property. The documentation of the pattern flags can be found on the [spdlog's custom formatting wiki page](https://github.com/gabime/spdlog/wiki/3.-Custom-formatting#customizing-format-using-set_pattern). The default log pattern is the following, but there are additional examples in the default minifi-log.properties file:

    # in minifi-log.properties
    spdlog.pattern=[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v

### Log compression
Logs can be retrieved with the C2 Debug command through the C2 protocol. For this functionality some logs are kept in memory in compressed form. The following properties can be used to configure the limits for this compression:

    # in minifi-log.properties
    compression.cached.log.max.size=8 MB
    compression.compressed.log.max.size=8 MB

### Shortening log messages
There are some additional properties that can be used to shorten log messages:

    # in minifi-log.properties
    # The spdlog.shorten_names can be set to prune package names from logs
    spdlog.shorten_names=true

    # Use the following if you do not want to include the UUID of the component at the end of log lines
    logger.include.uuid=false

    # You can set the maximum length of the log entries
    max.log.entry.length=4096

**NOTE:** All log configuration properties can be changed through C2 protocol with the property update command, using the `nifi.log.` prefix. For example to change the root log level with the `logger.root` property through C2, the C2 property update command should change the `nifi.log.logger.root` property.

## Recommended Antivirus Exclusions
Antivirus software can take a long time to scan large directories and the numerous files within them. Additionally, if the antivirus software locks files or directories during a scan, those resources are unavailable to the MiNiFi C++ process. To prevent these performance and reliability issues from occurring, it is highly recommended to configure your antivirus software to skip scans on the following MiNiFi C++ directories (or the specified alternatives defined in the minifi.properties file):

- content_repository
- flowfile_repository
- logs
- provenance_repository
- corecomponentstate
