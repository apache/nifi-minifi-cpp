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
* 0.2.0 represents a non-GA release, APIs and interfaces are subject to change
* Build and usage currently only supports Linux and OS X environments. Providing the needed tooling to support Windows will be established as part of [MINIFI-34](https://issues.apache.org/jira/browse/MINIFI-34).
* Using Site to Site requires the additional manual step of specifying the remote socket.  This being autonegotiated through NiFi's REST API is captured in [MINIFI-70](https://issues.apache.org/jira/browse/MINIFI-70).
* The processors currently implemented include:
  * AppendHostInfo
  * ExecuteProcess
  * GetFile
  * GenerateFlowFile
  * InvokeHTTP
  * LogAttribute
  * ListenHTTP
  * ListenSyslog
  * PutFile
  * TailFile
* Provenance events generation is supported and are persisted using levelDB.

## System Requirements

### To build

#### Utilities
* CMake
  * 3.1 or greater
* gcc
  * 4.8.4 or greater
* g++
  * 4.8.4 or greater

#### Libraries / Development Headers
* libboost and boost-devel
  * 1.48.0 or greater
* libcurl
* libleveldb and libleveldb-devel
* libuuid and uuid-dev
* openssl

### To run

#### Libraries
* libuuid
* libleveldb
* libcurl
* libssl and libcrypto from openssl 

The needed dependencies can be installed with the following commands for:

Yum based Linux Distributions
```
# ~/Development/code/apache/nifi-minifi-cpp on git:master
$ yum install cmake \
  gcc gcc-c++ \
  libcurl-devel \
  leveldb-devel leveldb \
  libuuid libuuid-devel \
  boost-devel \
  libssl-dev \
  doxygen
```

Aptitude based Linux Distributions
```
# ~/Development/code/apache/nifi-minifi-cpp on git:master
$ apt-get install cmake \
  gcc g++ \
  libcurl-dev \
  libleveldb-dev libleveldb1v5 \
  uuid-dev uuid \
  libboost-all-dev libssl-dev \
  doxygen
```

OS X Using Homebrew (with XCode Command Line Tools installed)
```
# ~/Development/code/apache/nifi-minifi-cpp on git:master
$ brew install cmake \
  curl \
  leveldb \
  ossp-uuid \
  boost \
  openssl \
  doxygen
```


## Getting Started

### Building

- From your source checkout, create a directory to perform the build (e.g. build) and cd into that directory.
  ```
  # ~/Development/code/apache/nifi-minifi-cpp on git:master
  $ mkdir build
  # ~/Development/code/apache/nifi-minifi-cpp on git:master
  $ cd build
  ```

- Perform a `cmake ..` to generate the project files
  ```
  # ~/Development/code/apache/nifi-minifi-cpp on git:master
  $ cmake ..
  ...
  -- Configuring done
  -- Generating done
  -- Build files have been written to: /Users/apiri/Development/code/apache/nifi-minifi-cpp/build
  ```

- Perform a build
  ```
  # ~/Development/code/apache/nifi-minifi-cpp on git:master
  $ make
  Scanning dependencies of target gmock_main
  Scanning dependencies of target gmock
  Scanning dependencies of target minifi
  Scanning dependencies of target gtest
  Scanning dependencies of target yaml-cpp
  [  1%] Building CXX object thirdparty/yaml-cpp-yaml-cpp-0.5.3/test/gmock-1.7.0/gtest/CMakeFiles/gtest.dir/src/gtest-all.cc.o
  [  3%] Building CXX object thirdparty/yaml-cpp-yaml-cpp-0.5.3/test/gmock-1.7.0/CMakeFiles/gmock.dir/gtest/src/gtest-all.cc.o
  [  3%] Building CXX object thirdparty/yaml-cpp-yaml-cpp-0.5.3/test/gmock-1.7.0/CMakeFiles/gmock.dir/src/gmock-all.cc.o
  [  6%] Building CXX object thirdparty/yaml-cpp-yaml-cpp-0.5.3/test/gmock-1.7.0/CMakeFiles/gmock_main.dir/gtest/src/gtest-all.cc.o
  [  6%] Building CXX object thirdparty/yaml-cpp-yaml-cpp-0.5.3/test/gmock-1.7.0/CMakeFiles/gmock_main.dir/src/gmock-all.cc.o
  [  7%] Building CXX object libminifi/CMakeFiles/minifi.dir/src/Configure.cpp.o

  ...

  [ 97%] Linking CXX executable minifi
  [ 97%] Built target minifiexe
  [ 98%] Building CXX object thirdparty/yaml-cpp-yaml-cpp-0.5.3/test/CMakeFiles/run-tests.dir/node/node_test.cpp.o
  [100%] Linking CXX executable run-tests
  [100%] Built target run-tests
  ```

- Create a binary assembly located in your build directory with suffix -bin.tar.gz
  ```
  ~/Development/code/apache/nifi-minifi-cpp/build
  $ make package
  Run CPack packaging tool for source...
  CPack: Create package using TGZ
  CPack: Install projects
  CPack: - Install directory: ~/Development/code/apache/nifi-minifi-cpp
  CPack: Create package
  CPack: - package: ~/Development/code/apache/nifi-minifi-cpp/build/nifi-minifi-cpp-0.1.0-bin.tar.gz generated.
  ```

- Create a source assembly located in your build directory with suffix -source.tar.gz
  ```
  ~/Development/code/apache/nifi-minifi-cpp/build
  $ make package_source
  Run CPack packaging tool for source...
  CPack: Create package using TGZ
  CPack: Install projects
  CPack: - Install directory: ~/Development/code/apache/nifi-minifi-cpp
  CPack: Create package
  CPack: - package: ~/Development/code/apache/nifi-minifi-cpp/build/nifi-minifi-cpp-0.1.0-source.tar.gz generated.
  ```

- (Optional) Create a Docker image from the resulting binary assembly output from "make package".
```
~/Development/code/apache/nifi-minifi-cpp/build
$ make docker
NiFi-MiNiFi-CPP Version: 0.3.0
Current Working Directory: /Users/jdyer/Development/github/nifi-minifi-cpp/docker
CMake Source Directory: /Users/jdyer/Development/github/nifi-minifi-cpp
MiNiFi Package: nifi-minifi-cpp-0.3.0-bin.tar.gz
Docker Command: 'docker build --build-arg UID=1000 --build-arg GID=1000 --build-arg MINIFI_VERSION=0.3.0 --build-arg MINIFI_PACKAGE=nifi-minifi-cpp-0.3.0-bin.tar.gz -t apacheminificpp:0.3.0 .'
Sending build context to Docker daemon 777.2 kB
Step 1 : FROM alpine:3.5
 ---> 88e169ea8f46
Step 2 : MAINTAINER Apache NiFi <dev@nifi.apache.org>

...

Step 15 : CMD $MINIFI_HOME/bin/minifi.sh run
 ---> Using cache
 ---> c390063d9bd1
Successfully built c390063d9bd1
Built target docker
```

### Cleaning
Remove the build directory created above.
```
# ~/Development/code/apache/nifi-minifi-cpp on git:master
$ rm -rf ./build
```

### Configuring
The 'conf' directory in the root contains a template config.yml document.  

This is compatible with the format used with the Java MiNiFi application.  Currently, a subset of the configuration is supported and MiNiFi C++ is currently compatible with version 1 of the MiNiFi YAML schema.
Additional information on the YAML format for the config.yml and schema versioning can be found in the [MiNiFi System Administrator Guide](https://nifi.apache.org/minifi/system-admin-guide.html).

Additionally, users can utilize the MiNiFi Toolkit Converter (version 0.0.1 - schema version 1) to aid in creating a flow configuration from a generated template exported from a NiFi instance.  The MiNiFi Toolkit Converter tool can be downloaded from http://nifi.apache.org/minifi/download.html under the `MiNiFi Toolkit Binaries` section.  Information on its usage is available at https://nifi.apache.org/minifi/minifi-toolkit.html.


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
                    Port: 10001
                    Host Name: localhost

### Site2Site Security Configuration

    in minifi.properties 

    enable tls ssl
    nifi.remote.input.secure=true

    if you want to enable client certificate base authorization 
    nifi.security.need.ClientAuth=true
    setup the client certificate and private key PEM files
    nifi.security.client.certificate=./conf/client.pem
    nifi.security.client.private.key=./conf/client.pem
    setup the client private key passphrase file
    nifi.security.client.pass.phrase=./conf/password
    setup the client CA certificate file
    nifi.security.client.ca.certificate=./conf/nifi-cert.pem

    if you do not want to enable client certificate base authorization
    nifi.security.need.ClientAuth=false

### Provenance Report

    Add Provenance Reporting to config.yml
    Provenance Reporting:
      scheduling strategy: TIMER_DRIVEN
      scheduling period: 1 sec
      port: 10001
      host: localhost
      port uuid: 471deef6-2a6e-4a7d-912a-81cc17e3a204
      batch size: 100
      
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
        

### Running
After completing a [build](#building), the application can be run by issuing the following from :

    $ ./bin/minifi.sh start

By default, this will make use of a config.yml located in the conf directory.  This configuration file location can be altered by adjusting the property `nifi.flow.configuration.file` in minifi.properties located in the conf directory.

### Stopping  

MiNiFi can then be stopped by issuing:

    $ ./bin/minifi.sh stop

### Installing as a service

MiNiFi can also be installed as a system service using minifi.sh with an optional "service name" (default: minifi)

    $ ./bin/minifi.sh install [service name]

## Documentation
See https://nifi.apache.org/minifi for the latest documentation.

## Contributing

We welcome all contributions to Apache MiNiFi. To make development easier, we've included 
the linter for the Google Style guide. Google provides an Eclipse formatter for their style
guide. It is located [here](https://github.com/google/styleguide/blob/gh-pages/eclipse-cpp-google-style.xml).
New contributions are expected to follow the Google style guide when it is reasonable. 
Additionally, all new files must include a copy of the Apache License Header. 

MiNiFi C++ contains a dynamic loading mechanism that loads arbitrary objects. To maintain
consistency of development amongst the NiFi ecosystem, it is called a class loader. If you
are contributing a custom Processor or Controller Service, the mechanism to register your class
into the default class loader is a pragma definition named:
    
    REGISTER_RESOURCE(CLASSNAME);
    
To use this include REGISTER_RESOURCE(YourClassName); in your header file. The default class
loader will make instnaces of YourClassName available for inclusion.  

Once you have completed your changes, including source code and tests, you can verify that
you follow the Google style guide by running the following command:
     $ make linter. 
This will provide output for all source files.

## License
Except as otherwise noted this software is licensed under the
[Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0.html)

For additional information regarding the source of included projects and
the corresponding licenses, you may visit the following [website](https://cwiki.apache.org/confluence/display/MINIFI/Licensing+Information)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
