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
  - [Bootstrapping](#bootstrapping)
  - [Building](#building)
  - [Cleaning](#cleaning)
  - [Configuring](#configuring)
  - [Running](#running)
- [Documentation](#documentation)
- [License](#license)

## Features

Apache NiFi - MiNiFi C++ is a complementary data collection approach that supplements the core tenets of [NiFi](http://nifi.apache.org/) in dataflow management, focusing on the collection of data at the source of its creation.  The C++ implementation is an additional implementation to the one in Java with the aim of an even smaller resource footprint.

Specific goals for MiNiFi comprise:
- small and lightweight footprint
- central management of agents
- generation of data provenance
- integration with NiFi for follow-on dataflow management and full chain of custody of information

Perspectives of the role of MiNiFi should be from the perspective of the agent acting immediately at, or directly adjacent to, source sensors, systems, or servers.

### Expression Language

A subset of the Apache NiFi [Expression Language](EXPRESSIONS.md) is supported.

### Processors

MiNiFi - C++ supports the following processors:

* [AppendHostInfo](PROCESSORS.md#appendhostinfo)
* [CompressContent](PROCESSORS.md#compresscontent)
* [ConsumeMQTT](PROCESSORS.md#consumeMQTT)
* [ExecuteProcess](PROCESSORS.md#executeprocess)
* [ExecuteScript](PROCESSORS.md#executescript)
* [ExtractText](PROCESSORS.md#extracttext)
* [FocusArchiveEntry](PROCESSORS.md#focusarchiveentry)
* [GenerateFlowFile](PROCESSORS.md#generateflowfile)
* [GetFile](PROCESSORS.md#getfile)
* [GetUSBCamera](PROCESSORS.md#getusbcamera)
* [InvokeHTTP](PROCESSORS.md#invokehttp)
* [ListenHTTP](PROCESSORS.md#listenhttp)
* [ListenSyslog](PROCESSORS.md#listensyslog)
* [LogAttribute](PROCESSORS.md#logattribute)
* [ManipulateArchive](PROCESSORS.md#manipulatearchive)
* [MergeContent](PROCESSORS.md#mergecontent)
* [PublishKafka](PROCESSORS.md#publishkafka)
* [PublishMQTT](PROCESSORS.md#publishMQTT)
* [PutFile](PROCESSORS.md#putfile)
* [TailFile](PROCESSORS.md#tailfile)
* [TFApplyGraph](PROCESSORS.md#tfapplygraph)
* [TFConvertImageToTensor](PROCESSORS.md#tfconvertimagetotensor)
* [TFExtractTopLabels](PROCESSORS.md#tfextracttoplabels)
* [UnfocusArchiveEntry](PROCESSORS.md#unfocusarchiveentry)

## Caveats
* 0.5.0 represents a non-GA release, APIs and interfaces are subject to change
* Build and usage currently only supports Linux and OS X environments. MiNiFi C++ can be built and run through the Windows Subsystem for Linux but we provide no support for this platform.
* Provenance events generation is supported and are persisted using RocksDB. Volatile repositories can be used on systems without persistent storage.

## System Requirements

### To build

#### Utilities
* CMake
  * 3.1 or greater
* gcc
  * 4.8.4 or greater
* g++
  * 4.8.4 or greater
* bison
  * 3.0 or greater
* flex
  * 2.5 or greater
  
**NOTE** if Lua support is enabled, then a C++ compiler with support for c++-14 must be used. If using GCC, version 6.x
or greater is recommended.

#### Libraries / Development Headers
* libboost and boost-devel
  * 1.48.0 or greater
* libcurl
* librocksdb4.1 and librocksdb-dev
* libuuid and uuid-dev
* openssl
* Python 3 and development headers -- Required, unless Python support is disabled
* Lua and development headers -- Optional, unless Lua support is enabled
* libgps-dev -- Required if building libGPS support

** NOTE: IF ROCKSDB IS NOT INSTALLED, IT WILL BE BUILT FROM THE THIRD PARTY
DIRECTORY UNLESS YOU SPECIFY -DDISABLE_ROCKSDB=true WITH CMAKE ***

#### CentOS 6

Additional environmental preparations are required for CentOS 6 support. Before
building, install and enable the devtoolset-6 SCL:

```
$ sudo yum install centos-release-scl
$ sudo yum install devtoolset-6
$ scl enable devtoolset-6 bash
```

Additionally, for expression language support, it is recommended to install GNU
Bison 3.x:

```
$ wget https://ftp.gnu.org/gnu/bison/bison-3.0.4.tar.xz
$ tar xvf bison-3.0.4.tar.xz
$ cd bison-3.0.4
$ ./configure
$ make
$ sudo make install
```

Finally, it is required to add the `-lrt` compiler flag by using the
`-DCMAKE_CXX_FLAGS=-lrt` flag when invoking cmake.

#### SLES
  SLES 11 requires manual installation of the SDK using the following link:
    https://www.novell.com/support/kb/doc.php?id=7015337
  Once these are installed you will need to download and build CMAKE3, OpenSSL1.0,
  and Python3. Once these are installed follow the cmake procedures. The bootstrap
  script will not work.

  SLES 12 requires you to enable the SDK module in YAST. It is advised that you use
  the bootstrap script to help guide installation. Please see the Bootstrapping section
  below.

### To run

#### Libraries
* libuuid
* librocksdb *** IF NOT INSTALLED, WILL BE BUILT FROM THIRD PARTY DIRECTORY ***
* libcurl
* libssl and libcrypto from openssl 
* libarchive
* librdkafka
* Python 3 -- Required, unless Python support is disabled
* Lua -- Optional, unless Lua support is enabled
* libusb -- Optional, unless USB Camera support is enabled
* libpng -- Optional, unless USB Camera support is enabled
* libpcap -- Optional, unless ENABLE_PCAP specified

The needed dependencies can be installed with the following commands for:

##### Yum based Linux Distributions

**NOTE** if a newer compiler is required, such as when Lua support is enabled, it is recommended to use a newer compiler
using a devtools-* package from the Software Collections (SCL). 

```
# ~/Development/code/apache/nifi-minifi-cpp on git:master
$ yum install cmake \
  gcc gcc-c++ \
  bison \
  flex \
  libcurl-devel \
  rocksdb-devel rocksdb \
  libuuid libuuid-devel \
  boost-devel \
  openssl-devel \
  bzip2-devel \
  xz-devel \
  doxygen
$ # (Optional) for building Python support
$ yum install python34-devel
$ # (Optional) for building Lua support
$ yum install lua-devel
$ # (Optional) for building USB Camera support
$ yum install libusb-devel libpng-devel
$ # (Optional) for building docker image
$ yum install docker
$ # (Optional) for system integration tests
$ yum install docker python-virtualenv
# If building with GPS support
$ yum install gpsd-devel
$ # (Optional) for PacketCapture Processor
$ yum install libpcap-devel
```

##### Aptitude based Linux Distributions
```
# ~/Development/code/apache/nifi-minifi-cpp on git:master
$ apt-get install cmake \
  gcc g++ \
  bison \
  flex \
  libcurl-dev \
  librocksdb-dev librocksdb4.1 \
  uuid-dev uuid \
  libboost-all-dev libssl-dev \
  libbz2-dev liblzma-dev \
  doxygen
$ # (Optional) for building Python support
$ apt-get install libpython3-dev
$ # (Optional) for building Lua support
$ apt-get install liblua5.1-0-dev
$ # (Optional) for building USB Camera support
$ apt-get install libusb-1.0.0-0-dev libpng12-dev
$ # (Optional) for building docker image
$ apt-get install docker.io
$ # (Optional) for system integration tests
$ apt-get install docker.io python-virtualenv
# If building with GPS support
$ apt-get install libgps-dev
$ # (Optional) for PacketCapture Processor
$ apt-get install libpcap-dev
```

##### OS X Using Homebrew (with XCode Command Line Tools installed)
```
# ~/Development/code/apache/nifi-minifi-cpp on git:master
$ brew install cmake \
  bison \
  flex \
  rocksdb \
  ossp-uuid \
  boost \
  openssl \
  python \
  lua \
  xz \
  bzip2 \
  doxygen
$ brew install curl
$ brew link curl --force
$ # (Optional) for building USB Camera support
$ brew install libusb libpng
$ # (Optional) for building docker image/running system integration tests
$ # Install docker using instructions at https://docs.docker.com/docker-for-mac/install/
$ sudo pip install virtualenv
# If building with GPS support
$ brew install gpsd
$ # (Optional) for PacketCapture Processor
$ sudo brew install libpcap
```


## Getting Started

### Bootstrapping

- MiNiFi C++ offers a bootstrap script in the root of our github repo that will boot strap the cmake and build process for you without the need to install dependencies yourself. To use this process, please run the command boostrap.sh from the root of the MiNiFi C++ source tree.


- Per the table, below, you will be presented with a menu guided bootstrap process. You may enable and disable extensions ( further defined below ). Once you are finished selecting the features you wish to build, enter N to continue with the process. CMAKE dependencies will be resolved for your distro. You may enter command line options -n to force yes to all prompts ( including the package installation prompts ) and -b to automatically run make once the cmake process is complete. Alternatively, you may include the package argument to boostrap, -p, which will run make package.

- If you provide -b or -p to bootstrap.sh, you do not need to follow the Building section, below. If you do not provide these arguments you may skip the cmake .. section from Building.

  ```
  # ~/Development/code/apache/nifi-minifi-cpp on git:master
  $ ./bootstrap.sh
  # CMAKE Build dir exists, should we overwrite your build directory before we begin?
    If you have already bootstrapped, bootstrapping again isn't necessary to run make [ Y/N ] Y
  $  ****************************************
     Select MiNiFi C++ Features to toggle.
    ****************************************
    A. Persistent Repositories .....Enabled
    B. Lib Curl Features ...........Enabled
    C. Lib Archive Features ........Enabled
    D. Execute Script support ......Enabled
    E. Expression Langauge support .Enabled
    F. Kafka support ...............Disabled
    G. PCAP support ................Disabled
    H. USB Camera support ..........Disabled
    I. GPS support .................Disabled
    J. TensorFlow Support ..........Disabled
    K. Enable all extensions
    L. Portable Build ..............Enabled
    M. Build with Debug symbols ....Disabled
    N. Continue with these options
    Q. Exit
    * Extension cannot be installed due to
      version of cmake or other software
    
    Enter choice [ A - N ] 
  ```

### Building

- From your source checkout, create a directory to perform the build (e.g. build) and cd into that directory.
  ```
  # ~/Development/code/apache/nifi-minifi-cpp on git:master
  $ mkdir build
  # ~/Development/code/apache/nifi-minifi-cpp on git:master
  $ cd build
  ```

- Perform a `cmake ..` to generate the project files
  - Optionally disable or enable extensions. Please visit our guide [extensions guide](Extensions.md) for flags or our wiki entry on
    [customizing builds](https://cwiki.apache.org/confluence/pages/viewpage.action?pageId=74685143) for more information on this topic. 
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
  CPack: - package: ~/Development/code/apache/nifi-minifi-cpp/build/nifi-minifi-cpp-0.5.0-bin.tar.gz generated.
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
  CPack: - package: ~/Development/code/apache/nifi-minifi-cpp/build/nifi-minifi-cpp-0.5.0-source.tar.gz generated.
  ```

- (Optional) Create a Docker image from the resulting binary assembly output from "make package".
```
~/Development/code/apache/nifi-minifi-cpp/build
$ make docker
NiFi-MiNiFi-CPP Version: 0.5.0
Current Working Directory: /Users/jdyer/Development/github/nifi-minifi-cpp/docker
CMake Source Directory: /Users/jdyer/Development/github/nifi-minifi-cpp
MiNiFi Package: nifi-minifi-cpp-0.5.0-bin.tar.gz
Docker Command: 'docker build --build-arg UID=1000 --build-arg GID=1000 --build-arg MINIFI_VERSION=0.5.0 --build-arg MINIFI_PACKAGE=nifi-minifi-cpp-0.5.0-bin.tar.gz -t apacheminificpp:0.5.0 .'
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

- (Optional) Execute system integration tests using the docker image built locally on a docker daemon running locally.
```
~/Development/code/apache/nifi-minifi-cpp/build
$ make docker-verify
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

### SiteToSite Security Configuration

    in minifi.properties

    enable tls
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
  
If you are using HTTP Site to Site, you have the option of specifying an SSL Context Service definition for the RPGs. 
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
                Port: 10443
                SSL Context Service: SSLServiceName
                Host Name: 127.0.0.1
      Output Ports:
          - id: ac82e521-015c-1000-2b21-41279516e19a
            name: tominifi
            max concurrent tasks: 2
            Properties:
                Port: 10443
		        SSL Context Service: SSLServiceName
		        Host Name: 127.0.0.1
	Controller Services:
    - name: SSLServiceName
      id: 2438e3c8-015a-1000-79ca-83af40ec1974
      class: SSLContextService
      Properties:
          Client Certificate: <client cert path>
          Private Key: < private key path > 
          Passphrase: <passphrase path or passphrase>
          CA Certificate: <CA cert path>

If during testing you have a need to disable host or peer verification, you may use the following options:

    # in minifi.properties
    nifi.security.client.disable.host.verification=true
	nifi.security.client.disable.peer.verification=true
    
### HTTP SiteToSite Configuration
To enable HTTPSiteToSite you must set the following flag to true. 
	
    nifi.remote.input.http.enabled=true
    
### Command and Control Configuration
For more more insight into the API used within the C2 agent, please visit:
https://cwiki.apache.org/confluence/display/MINIFI/C2+Design+Proposal

    in minifi.properties

    #Disable/Enable C2
    nifi.c2.enable=true

	#specify metrics classes
	nifi.flow.metrics.classes=DeviceInformation,SystemInformation,ProcessMetrics
	
	#specify C2 protocol
	c2.agent.protocol.class=RESTSender
	
	#control c2 heartbeat interval in millisecocnds
	c2.agent.heartbeat.period=3000
	
	# enable reporter classes
	c2.agent.heartbeat.reporter.class=RESTReciver
	
	
### Configuring Repository storage locations
Persistent repositories, such as the Flow File repository, use a configurable path to store data. 
The repository locations and their defaults are defined below. By default the MINIFI_HOME env
variable is used. If this is not specified we extrapolate the path and use the root installation
folder. You may specify your own path in place of these defaults. 
     
     in minifi.properties
     nifi.provenance.repository.directory.default=${MINIFI_HOME}/provenance_repository
     nifi.flowfile.repository.directory.default=${MINIFI_HOME}/flowfile_repository
	 nifi.database.content.repository.directory.default=${MINIFI_HOME}/content_repository

### Configuring Volatile and NO-OP Repositories
Each of the repositories can be configured to be volatile ( state kept in memory and flushed
 upon restart ) or persistent. Currently, the flow file and provenance repositories can persist
 to RocksDB. The content repository will persist to the local file system if a volatile repo
 is not configured.

 To configure the repositories:

     in minifi.properties
     # For Volatile Repositories:
     nifi.flowfile.repository.class.name=VolatileFlowFileRepository
     nifi.provenance.repository.class.name=VolatileProvenanceRepository
     nifi.content.repository.class.name=VolatileContentRepository

     # configuration options
     # maximum number of entries to keep in memory
     nifi.volatile.repository.options.flowfile.max.count=10000
     # maximum number of bytes to keep in memory, also limited by option above
     nifi.volatile.repository.options.flowfile.max.bytes=1M

     # maximum number of entries to keep in memory
     nifi.volatile.repository.options.provenance.max.count=10000
     # maximum number of bytes to keep in memory, also limited by option above
     nifi.volatile.repository.options.provenance.max.bytes=1M

     # maximum number of entries to keep in memory
     nifi.volatile.repository.options.content.max.count=100000
     # maximum number of bytes to keep in memory, also limited by option above
     nifi.volatile.repository.options.content.max.bytes=1M
     # limits locking for the content repository
     nifi.volatile.repository.options.content.minimal.locking=true
     
     # For NO-OP Repositories:
	 nifi.flowfile.repository.class.name=NoOpRepository
     nifi.provenance.repository.class.name=NoOpRepository

 #### Caveats
 Systems that have limited memory must be cognizant of the options above. Limiting the max count for the number of entries limits memory consumption but also limits the number of events that can be stored. If you are limiting the amount of volatile content you are configuring, you may have excessive session rollback due to invalid stream errors that occur when a claim cannot be found.

 The content repository has a default option for "minimal.locking" set to true. This will attempt to use lock free structures. This may or may not be optimal as this requires additional additional searching of the underlying vector. This may be optimal for cases where max.count is not excessively high. In cases where object permanence is low within the repositories, minimal locking will result in better performance. If there are many processors and/or timing is such that the content repository fills up quickly, performance may be reduced. In all cases a locking cache is used to avoid the worst case complexity of O(n) for the content repository; however, this caching is more heavily used when "minimal.locking" is set to false.

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
    
### Managing MiNFI C++ through the MiNiFi Controller

The MiNiFi controller is an executable in the bin directory that can be used to control the MiNFi C++ agent while it runs. Currently the controller will let you stop subcomponents within a running instance, clear queues, get the status of queues, and update the flow for a warm re-deploy.

The minificontroller can track a single MiNiFi C++ agent through the use of three options. Port is required.
The hostname is not and will default to localhost. Additionally, controller.socket.local.any.interface allows
you to bind to any address when using localhost. Otherwise, we will bind only to the loopback adapter so only
minificontroller on the local host can control the agent:

	$ controller.socket.host=localhost
	$ controller.socket.port=9998
	$ controller.socket.local.any.interface=true/false ( default false)

These are defined by default to the above values. If the port option is left undefined, the MiNiFi controller
will be disabled in your deployment.

 The executable is stored in the bin directory and is titled minificontroller. Available commands are listed below.
 
 #### Specifying connecting information
 
   ./minificontroller --host "host name" --port "port"

        * By default these options use those defined in minifi.properties and are not required

 #### Start Command
 
   ./minificontroller --start "component name"
 
 #### Stop command 
   ./minificontroller --stop "component name"
   	  
 #### List connections command
   ./minificontroller --list connections
      
 #### List components command
   ./minificontroller --list components
 
 #### Clear connection command
   ./minificontroller --clear "connection name"
      
 #### GetSize command
   ./minificontroller --getsize "connection name"

       * Returns the size of the connection. The current size along with the max will be reported
 
 #### Update flow
   ./minificontroller --updateflow "config yml"
    
       *Updates the flow file reference and performs a warm re-deploy.
 
 #### Get full connection command     
   ./minificontroller --getfull 
   
       *Provides a list of full connections, if any.

### Extensions

Please see [Extensions.md](Extensions.md) on how to build and run conditionally built dependencies and extensions.

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
