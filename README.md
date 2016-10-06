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
# Apache NiFi -  MiNiFi - C++ [![Build Status](https://travis-ci.org/apache/nifi-minifi-cpp.svg?branch=master)](https://travis-ci.org/apache/nifi-minifi-cpp)

MiNiFi is a child project effort of Apache NiFi.  This repository is for a native implementation in C++.

## Table of Contents

- [Features](#features)
- [Caveats](#caveats)
- [Getting Started](#getting-started)
  - [System Requirements](#system-requirements)
  - [Building](#building)
  - [Cleaning](#cleaning)
  - [Configuring](#configuring)
  - [Running](#running)
- [Documentation](#documentation)
- [License](#license)

## Features

Apache NiFi - MiNiFi C++ is a complementary data collection approach that supplements the core tenets of [NiFi](http://nifi.apache.org/) in dataflow management, focusing on the collection of data at the source of its creation.  The C++ implementation is an additional implementation to the one in Java with the aim of an even smaller resource footprint.

Specific goals for MiNiFi are comprised of:
- small and lightweight footprint
- central management of agents
- generation of data provenance
- integration with NiFi for follow-on dataflow management and full chain of custody of information

Perspectives of the role of MiNiFi should be from the perspective of the agent acting immediately at, or directly adjacent to, source sensors, systems, or servers.

## Caveats
* 0.0.1 represents the first release, APIs and interfaces are subject to change
* Build and usage currently only supports Linux and OS X environments. Providing the needed tooling to support Windows will be established as part of [MINIFI-34](https://issues.apache.org/jira/browse/MINIFI-34).
* Currently, provenance events are not yet generated.  This effort is captured in [MINIFI-78](https://issues.apache.org/jira/browse/MINIFI-78).
* Using Site to Site requires the additional manual step of specifying the remote socket.  This being autonegotiated through NiFi's REST API is captured in [MINIFI-70](https://issues.apache.org/jira/browse/MINIFI-70).
* The processors currently implemented include:
  * TailFile
  * GetFile
  * GenerateFlowFile
  * LogAttribute
  * ListenSyslog

## System Requirements

### To build

#### Utilities
* Make
* gcc
  * 4.8.4 or greater
* g++
  * 4.8.4 or greater

#### Libraries / Development Headers
* libboost and boost-devel
  * 1.23.0 or greater
* libxml2 and libxml2-devel

### To run

#### Libraries
* libxml2

## Getting Started

### Building
From your source checkout, perform `make` in the root of the directory where the Makefile is located.  For parallel building, the '-j' or '--jobs' option maybe used.  On an average development machine, a serial build takes approximately 90 seconds.

    # ~/Development/code/apache/nifi-minifi-cpp on git:master
    $ make
    make -C thirdparty/yaml-cpp-yaml-cpp-0.5.3
    mkdir -p ./build
    g++ -Os -I./include  -c -o build/parse.o src/parse.cpp
    mkdir -p ./build
    g++ -Os -I./include  -c -o build/parser.o src/parser.cpp
    mkdir -p ./build
    g++ -Os -I./include  -c -o build/regex_yaml.o src/regex_yaml.cpp
    ...


### Cleaning
Generated files and artifacts can be removed by performing a `make clean`.

    # ~/Development/code/apache/nifi-minifi-cpp on git:master
    $ make clean
    rm -rf ./build
    rm -rf ./target
    rm -rf ./assemblies
    make -C thirdparty/yaml-cpp-yaml-cpp-0.5.3 clean
    rm -rf ./lib ./build
    make -C thirdparty/uuid clean
    rm -f *.o libuuid.a
    find ./ -iname "*.o" -exec rm -f {} \;

### Configuring
The 'conf' directory in the root contains a template flow.yml document.  

This is compatible with the format used with the Java MiNiFi application.  Currently, a subset of the configuration is supported.  Additional information on the YAML format for the flow.yml can be found in the [MiNiFi System Administrator Guide](https://nifi.apache.org/minifi/system-admin-guide.html).  

Additionally, users can utilize the MiNiFi Toolkit Converter to aid in creating a flow configuration from a generated template exported from a NiFi instance.  The MiNiFi Toolkit Converter tool can be downloaded from http://nifi.apache.org/minifi/download.html under the `MiNiFi Toolkit Binaries` section.  Information on its usage is available at https://nifi.apache.org/minifi/minifi-toolkit.html.


    Flow Controller:
        name: MiNiFi Flow

    Processors:
        - name: GetFile
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
          source name: GetFile
          source relationship name: success
          destination name: 471deef6-2a6e-4a7d-912a-81cc17e3a204
          max work queue size: 0
          max work queue data size: 1 MB
          flowfile expiration: 60 sec

    Remote Processing Groups:
        - name: NiFi Flow
          url: http://localhost:8080/nifi
          timeout: 30 secs
          yield period: 10 sec
          Input Ports:
              - id: 471deef6-2a6e-4a7d-912a-81cc17e3a204
                name: From Node A
                max concurrent tasks: 1
                Properties:
                    Port: 10001
                    Host Name: localhost

### Running
After completing a [build](#building), the application can be run by issuing:

    $ ./bin/minifi.sh start

By default, this will make use of a flow.yml located in the conf directory.  This configuration file location can be altered by adjusting the property `nifi.flow.configuration.file` in minifi.properties located in the conf directory.

### Stopping  

MiNiFi can then be stopped by issuing:

    $ ./bin/minifi.sh stop

### Installing as a service

MiNiFi can also be installed as a system service using minifi.sh with an optional "service name" (default: minifi)

    $ ./bin/minifi.sh install [service name]

## Documentation
See http://nifi.apache.org/minifi for the latest documentation.

## License
Except as otherwise noted this software is licensed under the
[Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0.html)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
