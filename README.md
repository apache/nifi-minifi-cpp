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

# Apache NiFi -  MiNiFi - C++ [![Linux/Mac Build Status](https://travis-ci.org/apache/nifi-minifi-cpp.svg?branch=master)](https://travis-ci.org/apache/nifi-minifi-cpp) [![Windows Build Status](https://ci.appveyor.com/api/projects/status/njagiyqmopexidsv/branch/master?svg=true)](https://ci.appveyor.com/project/ApacheSoftwareFoundation/nifi-minifi-cpp) 

MiNiFi is a child project effort of Apache NiFi.  This repository is for a native implementation in C++.

## Table of Contents

- [Features](#features)
- [Caveats](#caveats)
- [Getting Started](#getting-started)
  - [System Requirements](#system-requirements)
  - [Bootstrapping](#bootstrapping)
  - [Cleaning](#cleaning)
  - [Configuring](#configuring)
  - [Running](#running)
  - [Deploying](#deploying)
  - [Extensions](#extensions)
- [Operations](#operations)
- [Issue Tracking](#issue-tracking)
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

MiNiFi - C++ supports the following C++ processors:

The following table lists the base set of processors.  

| Extension Set        | Processors           |
| ------------- |:-------------|
| **Base**    | [AppendHostInfo](PROCESSORS.md#appendhostinfo)<br/>[ExecuteProcess](PROCESSORS.md#executeprocess)<br/>[ExtractText](PROCESSORS.md#extracttext)<br/> [GenerateFlowFile](PROCESSORS.md#generateflowfile)<br/>[GetFile](PROCESSORS.md#getfile)<br/>[GetTCP](PROCESSORS.md#gettcp)<br/>[HashContent](PROCESSORS.md#hashcontent)<br/>[LogAttribute](PROCESSORS.md#logattribute)<br/>[ListenSyslog](PROCESSORS.md#listensyslog)<br/>[PutFile](PROCESSORS.md#putfile)<br/>[RouteOnAttribute](PROCESSORS.md#routeonattribute)<br/>[TailFile](PROCESSORS.md#tailfile)<br/>[UpdateAttribute](PROCESSORS.md#updateattribute)<br/>[ListenHTTP](PROCESSORS.md#listenhttp) 

The next table outlines CMAKE flags that correspond with MiNiFi extensions. Extensions that are enabled by default ( such as CURL ), can be disabled with the respective CMAKE flag on the command line. 

Through JNI extensions you can run NiFi processors using NARs. The JNI extension set allows you to run these Java processors. MiNiFi C++ will favor C++ implementations over Java implements. In the case where a processor is implemented in either language, the one in C++ will be selected; however, will remain transparent to the consumer. 


| Extension Set        | Processors           | CMAKE Flag  |
| ------------- |:-------------| :-----|
| Archive Extensions    | [ApplyTemplate](PROCESSORS.md#applytemplate)<br/>[CompressContent](PROCESSORS.md#compresscontent)<br/>[ManipulateArchive](PROCESSORS.md#manipulatearchive)<br/>[MergeContent](PROCESSORS.md#mergecontent)<br/>[FocusArchiveEntry](PROCESSORS.md#focusarchiveentry)<br/>[UnfocusArchiveEntry](PROCESSORS.md#unfocusarchiveentry)      |   -DBUILD_LIBARCHIVE=ON |
| CURL | [InvokeHTTP](PROCESSORS.md#invokehttp)      |    -DDISABLE_CURL=ON  |
| GPS | GetGPS      |    -DENABLE_GPS=ON  |
| Kafka | [PublishKafka](PROCESSORS.md#publishkafka)      |    -DENABLE_LIBRDKAFKA=ON  |
| JNI | **NiFi Processors**     |    -DENABLE_JNI=ON  |
| MQTT | [ConsumeMQTT](PROCESSORS.md#consumeMQTT)<br/>[PublishMQTT](PROCESSORS.md#publishMQTT)     |    -DENABLE_MQTT=ON  |
| PCAP | [CapturePacket](PROCESSORS.md#capturepacket)      |    -DENABLE_PCAP=ON  |
| Scripting | [ExecuteScript](PROCESSORS.md#executescript)<br/>**Custom Python Processors**     |    -DDISABLE_SCRIPTING=ON  |
| SQLLite | [ExecuteSQL](PROCESSORS.md#executesql)<br/>[PutSQL](PROCESSORS.md#putsql)      |    -DENABLE_SQLITE=ON  |
| Tensorflow | [TFApplyGraph](PROCESSORS.md#tfapplygraph)<br/>[TFConvertImageToTensor](PROCESSORS.md#tfconvertimagetotensor)<br/>[TFExtractTopLabels](PROCESSORS.md#tfextracttoplabels)<br/>      |    -DENABLE_TENSORFLOW=ON  |
| USB Camera | [GetUSBCamera](PROCESSORS.md#getusbcamera)     |    -DENABLE_USB_CAMERA=ON  |

 Please see our [Python guide](extensions/script/README.md) on how to write Python processors and use them within MiNiFi C++. 

## Caveats
* 0.5.0 represents a GA-release. We follow semver so you can expect API and ABI compatibility within minor releases. See [semver's website](https://semver.org/) for more information
* Build and usage currently only supports Linux and OS X environments. MiNiFi C++ can be built and run through the Windows Subsystem for Linux but we provide no support for this platform.
* Native Windows builds are possible with limited support. OPENSSL_ROOT_DIR must be specified to support OpenSSL support within your build.
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
  * 3.0.x (3.2 has been shown to fail builds)
* flex
  * 2.5 or greater

##### External Projects

The following utilities are needed to build external projects, when bundled
versions of LibreSSL, cURL, or zlib are used:

* patch 
* autoconf 
* automake 
* libtool 
  
**NOTE** if Lua support is enabled, then a C++ compiler with support for c++-14 must be used. If using GCC, version 6.x
or greater is recommended.

**NOTE** if bustache (ApplyTemplate) support is enabled, a recent version of a compiler supporting c++-11 must be used. GCC versions >= 6.3.1 are known to work.

**NOTE** if Kafka support is enabled, a recent version of a compiler supporting C++-11 regexes must be used. GCC versions >= 4.9.x are recommended.

**NOTE** if Expression Language support is enabled, FlexLexer must be in the include path and the version must be compatible with the version of flex used when generating lexer sources. Lexer source generation is automatically performed during CMake builds. To re-generate the sources, remove: 

 * extensions/expression-language/Parser.cpp
 * extensions/expression-language/Parser.hpp
 * extensions/expression-language/Scanner.cpp
 * extensions/expression-language/location.hh
 * extensions/expression-language/position.hh
 * extensions/expression-language/stack.hh

and rebuild.

#### Libraries / Development Headers
* libboost and boost-devel
  * 1.48.0 or greater
* libcurl-openssl (If not available or desired, NSS will be used by applying -DUSE_CURL_NSS)
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
Bison 3.0.4:

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

On all distributions please use -DUSE_SHARED_LIBS=OFF to statically link zlib, libcurl, and OpenSSL.

#### SLES
  SLES 11 requires manual installation of the SDK using the following link:
    https://www.novell.com/support/kb/doc.php?id=7015337
  Once these are installed you will need to download and build CMAKE3, OpenSSL1.0,
  and Python3. Once these are installed follow the cmake procedures. The bootstrap
  script will not work.

  SLES 12 requires you to enable the SDK module in YAST. It is advised that you use
  the bootstrap script to help guide installation. Please see the Bootstrapping section
  below.

#### Windows
  Build and Installation has been tested with Windows 10 using Visual Studio. You can build
  and create an MSI via the CPACK command. This requires the installation of the WiX
  toolset (http://wixtoolset.org/). To do this, open up a prompt into your build directory and 
  type 'cpack' . The CPACK command will automatically generate and provide you a path to the distributable
  msi file. 

### To run

#### Libraries
* libuuid
* librocksdb *** IF NOT INSTALLED, WILL BE BUILT FROM THIRD PARTY DIRECTORY ***
* libcurl-openssl (If not available or desired, NSS will be used by applying -DUSE_CURL_NSS)
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
  patch \
  autoconf \
  automake \
  libtool \
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
$ #depending on your yum repo you may need to manually build libcurl-openssl if you do not wish to use
  libcurl with NSS support. By default we will use NSS when libcurl-openssl is not available.
```

##### Aptitude based Linux Distributions
```
# ~/Development/code/apache/nifi-minifi-cpp on git:master
$ apt-get install cmake \
  gcc g++ \
  bison \
  flex \
  patch \
  autoconf \
  automake \
  libtool \
  libcurl4-openssl-dev \
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
  flex \
  patch \
  autoconf \
  automake \
  libtool \
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
$ # It is recommended that you install bison from source as HomeBrew now uses an incompatible version of Bison
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

- Boostrap now saves state between runs. State will automatically be saved. Provide -c or --clear to clear this state. The -i option provides a guided menu install with the ability to change 
advanced features.

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
  CPack: - package: ~/Development/code/apache/nifi-minifi-cpp/build/nifi-minifi-cpp-0.6.0-bin.tar.gz generated.
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
  CPack: - package: ~/Development/code/apache/nifi-minifi-cpp/build/nifi-minifi-cpp-0.6.0-source.tar.gz generated.
  ```

- (Optional) Create a Docker image from the resulting binary assembly output from "make package".
```
~/Development/code/apache/nifi-minifi-cpp/build
$ make docker
NiFi-MiNiFi-CPP Version: 0.6.0
Current Working Directory: /Users/jdyer/Development/github/nifi-minifi-cpp/docker
CMake Source Directory: /Users/jdyer/Development/github/nifi-minifi-cpp
MiNiFi Package: nifi-minifi-cpp-0.6.0-bin.tar.gz
Docker Command: 'docker build --build-arg UID=1000 --build-arg GID=1000 --build-arg MINIFI_VERSION=0.6.0 --build-arg MINIFI_PACKAGE=nifi-minifi-cpp-0.6.0-bin.tar.gz -t apacheminificpp:0.6.0 .'
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

### Snapcraft

Snapcraft builds are supported. As per Snapcraft's official recommendations, we recommend using Ubuntu 16.04 as a build system when building the Snap. To build the snap, run

```
$ snapcraft
```

from the project directory. Further instructions are available in the [Snapcraft documentation](https://docs.snapcraft.io/build-snaps/).

### Configuring
The 'conf' directory in the root contains a template config.yml document, minifi.properties, and minifi-log.properties. Please see our [Configuration document](CONFIGURE.md) for details on how to configure agents.
         
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

*** Currently windows does not support installing a windows service. ***
    
### Deploying
MiNiFi C++ comes with a deployment script. This will build and package minifi. Additionally, a file named build_output will be
created within the build directory that contains a manifest of build artifacts.

    $ deploy.sh <build identifier> 

The build identifier will be carried with the deployed binary for the configuration you specify. By default all extensions will be built.

On Windows it is suggested that MSI be used for installation. 
    
### Extensions

Please see [Extensions.md](Extensions.md) on how to build and run conditionally built dependencies and extensions.

## Operations
See our [operations documentation for additional inforomation on how to manage instances](OPS.md)

## Issue Tracking
See https://issues.apache.org/jira/projects/MINIFICPP/issues for the issue tracker.

## Documentation
See https://nifi.apache.org/minifi for the latest documentation.

## Contributing

We welcome all contributions to Apache MiNiFi. To make development easier, we've included
the linter for the Google Style guide. Google provides an Eclipse formatter for their style
guide. It is located [here](https://github.com/google/styleguide/blob/gh-pages/eclipse-cpp-google-style.xml).
New contributions are expected to follow the Google style guide when it is reasonable.
Additionally, all new files must include a copy of the Apache License Header.

For more details on how to contribute please see our [Contribution Guide](CONTRIB.md)
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
