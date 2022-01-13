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

[<img src="https://nifi.apache.org/assets/images/minifi/minifi-logo.svg" width="300" height="126" alt="Apache NiFi MiNiFi"/>](https://nifi.apache.org/minifi/)

# Apache NiFi -  MiNiFi - C++ ![MiNiFi-CPP CI](https://github.com/apache/nifi-minifi-cpp/workflows/MiNiFi-CPP%20CI/badge.svg?branch=main)

MiNiFi is a child project effort of Apache NiFi.  This repository is for a native implementation in C++.

## Table of Contents

- [Features](#features)
- [Caveats](#caveats)
- [Getting Started](#getting-started)
  - [System Requirements](#system-requirements)
  - [Bootstrapping](#bootstrapping)
  - [Building For Other Distros](#building-for-other-distros)
  - [Installation](#installation)
  - [Configuring](#configuring)
  - [Running](#running)
  - [Deploying](#deploying)
  - [Cleaning](#cleaning)
  - [Extensions](#extensions)
  - [Security](#security)
- [Operations](#operations)
- [Issue Tracking](#issue-tracking)
- [Documentation](#documentation)
- [Examples](#examples)
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

| Extension Set | Processors                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
|---------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Base**      | [AppendHostInfo](PROCESSORS.md#appendhostinfo)<br/>[AttributesToJSON](PROCESSORS.md#attributestojson)<br/>[DefragmentText](PROCESSORS.md#defragmenttext)<br/>[ExecuteProcess](PROCESSORS.md#executeprocess)<br/>[ExtractText](PROCESSORS.md#extracttext)<br/>[FetchFile](PROCESSORS.md#fetchfile)<br/>[GenerateFlowFile](PROCESSORS.md#generateflowfile)<br/>[GetFile](PROCESSORS.md#getfile)<br/>[GetTCP](PROCESSORS.md#gettcp)<br/>[HashContent](PROCESSORS.md#hashcontent)<br/>[ListenSyslog](PROCESSORS.md#listensyslog)<br/>[ListenTCP](PROCESSORS.md#listentcp)<br/>[ListenUDP](PROCESSORS.md#listenudp)<br/>[ListFile](PROCESSORS.md#listfile)<br/>[LogAttribute](PROCESSORS.md#logattribute)<br/>[PutFile](PROCESSORS.md#putfile)<br/>[PutTCP](PROCESSORS.md#puttcp)<br/>[PutUDP](PROCESSORS.md#putudp)<br/>[ReplaceText](PROCESSORS.md#replacetext)<br/>[RetryFlowFile](PROCESSORS.md#retryflowfile)<br/>[RouteOnAttribute](PROCESSORS.md#routeonattribute)<br/>[RouteText](PROCESSORS.md#routetext)<br/>[TailFile](PROCESSORS.md#tailfile)<br/>[UpdateAttribute](PROCESSORS.md#updateattribute) |

The next table outlines CMAKE flags that correspond with MiNiFi extensions. Extensions that are enabled by default ( such as CURL ), can be disabled with the respective CMAKE flag on the command line.

Through JNI extensions you can run NiFi processors using NARs. The JNI extension set allows you to run these Java processors. MiNiFi C++ will favor C++ implementations over Java implements. In the case where a processor is implemented in either language, the one in C++ will be selected; however, will remain transparent to the consumer.


| Extension Set                    | Processors and Controller Services                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              | CMAKE Flag                   |
|----------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-----------------------------|
| Archive Extensions               | [ApplyTemplate](PROCESSORS.md#applytemplate)<br/>[BinFiles](PROCESSORS.md#binfiles)<br/>[CompressContent](PROCESSORS.md#compresscontent)<br/>[ManipulateArchive](PROCESSORS.md#manipulatearchive)<br/>[MergeContent](PROCESSORS.md#mergecontent)<br/>[FocusArchiveEntry](PROCESSORS.md#focusarchiveentry)<br/>[UnfocusArchiveEntry](PROCESSORS.md#unfocusarchiveentry)                                                                                                                                                                                                                                                                          | -DBUILD_LIBARCHIVE=ON        |
| AWS                              | [AWSCredentialsService](CONTROLLERS.md#awscredentialsservice)<br/>[PutS3Object](PROCESSORS.md#puts3object)<br/>[DeleteS3Object](PROCESSORS.md#deletes3object)<br/>[FetchS3Object](PROCESSORS.md#fetchs3object)<br/>[ListS3](PROCESSORS.md#lists3)                                                                                                                                                                                                                                                                                                                                                                                               | -DENABLE_AWS=ON              |
| Azure                            | [AzureStorageCredentialsService](CONTROLLERS.md#azurestoragecredentialsservice)<br/>[PutAzureBlobStorage](PROCESSORS.md#putazureblobstorage)<br/>[DeleteAzureBlobStorage](PROCESSORS.md#deleteazureblobstorage)<br/>[FetchAzureBlobStorage](PROCESSORS.md#fetchazureblobstorage)<br/>[ListAzureBlobStorage](PROCESSORS.md#listazureblobstorage)<br/>[PutAzureDataLakeStorage](PROCESSORS.md#putazuredatalakestorage)<br/>[DeleteAzureDataLakeStorage](PROCESSORS.md#deleteazuredatalakestorage)<br/>[FetchAzureDataLakeStorage](PROCESSORS.md#fetchazuredatalakestorage)<br/>[ListAzureDataLakeStorage](PROCESSORS.md#listazuredatalakestorage) | -DENABLE_AZURE=ON            |
| CivetWeb                         | [ListenHTTP](PROCESSORS.md#listenhttp)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | -DDISABLE_CIVET=ON           |
| CURL                             | [InvokeHTTP](PROCESSORS.md#invokehttp)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | -DDISABLE_CURL=ON            |
| Elasticsearch                    | [ElasticsearchCredentialsControllerService](CONTROLLERS.md#elasticsearchcredentialscontrollerservice)<br/>[PostElasticsearch](PROCESSORS.md#postelasticsearch)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | -DENABLE_ELASTICSEARCH=ON    |
| GPS                              | [GetGPS](PROCESSORS.md#getgps)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | -DENABLE_GPS=ON              |
| Google Cloud Platform            | [DeleteGCSObject](PROCESSORS.md#deletegcsobject)<br>[FetchGCSObject](PROCESSORS.md#fetchgcsobject)<br>[GCPCredentialsControllerService](CONTROLLERS.md#gcpcredentialscontrollerservice)<br>[ListGCSBucket](PROCESSORS.md#listgcsbucket)<br>[PutGCSObject](PROCESSORS.md#putgcsobject)                                                                                                                                                                                                                                                                                                                                                           | -DENABLE_GCP=ON              |
| Kafka                            | [PublishKafka](PROCESSORS.md#publishkafka)<br>[ConsumeKafka](PROCESSORS.md#consumekafka)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        | -DENABLE_LIBRDKAFKA=ON       |
| Kubernetes                       | [KubernetesControllerService](CONTROLLERS.md#kubernetescontrollerservice)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       | -DENABLE_KUBERNETES=ON       |
| JNI                              | **NiFi Processors**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             | -DENABLE_JNI=ON              |
| Lua Scripting                    | [ExecuteScript](PROCESSORS.md#executescript)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | -DENABLE_LUA_SCRIPTING=ON    |
| MQTT                             | [ConsumeMQTT](PROCESSORS.md#consumemqtt)<br/>[PublishMQTT](PROCESSORS.md#publishmqtt)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | -DENABLE_MQTT=ON             |
| OPC                              | [FetchOPCProcessor](PROCESSORS.md#fetchopcprocessor)<br/>[PutOPCProcessor](PROCESSORS.md#putopcprocessor)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      | -DENABLE_OPC=ON              |
| OpenCV                           | [CaptureRTSPFrame](PROCESSORS.md#capturertspframe)<br/>[MotionDetector](PROCESSORS.md#motiondetector)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | -DENABLE_OPENCV=ON           |
| OpenWSMAN                        | SourceInitiatedSubscriptionListener                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             | -DENABLE_OPENWSMAN=ON        |
| PCAP                             | [CapturePacket](PROCESSORS.md#capturepacket)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | -DENABLE_PCAP=ON             |
| PDH (Windows only)               | [PerformanceDataMonitor](PROCESSORS.md#performancedatamonitor)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | -DENABLE_PDH=ON              |
| ProcFs (Linux only)              | [ProcFsMonitor](PROCESSORS.md#procfsmonitor)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | -DENABLE_PROCFS=ON           |
| Python Scripting                 | [ExecuteScript](PROCESSORS.md#executescript)<br>[ExecutePythonProcessor](PROCESSORS.md#executepythonprocessor)<br/>**Custom Python Processors**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 | -DENABLE_PYTHON_SCRIPTING=ON |
| Sensors                          | GetEnvironmentalSensors<br/>GetMovementSensors                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | -DENABLE_SENSORS=ON          |
| SFTP                             | [FetchSFTP](PROCESSORS.md#fetchsftp)<br/>[ListSFTP](PROCESSORS.md#listsftp)<br/>[PutSFTP](PROCESSORS.md#putsftp)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | -DENABLE_SFTP=ON             |
| SQL                              | [ExecuteSQL](PROCESSORS.md#executesql)<br/>[PutSQL](PROCESSORS.md#putsql)<br/>[QueryDatabaseTable](PROCESSORS.md#querydatabasetable)<br/>                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       | -DENABLE_SQL=ON              |
| Splunk                           | [PutSplunkHTTP](PROCESSORS.md#putsplunkhttp)<br/>[QuerySplunkIndexingStatus](PROCESSORS.md#querysplunkindexingstatus)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | -DENABLE_SPLUNK=ON           |
| Systemd                          | [ConsumeJournald](PROCESSORS.md#consumejournald)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | -DENABLE_SYSTEMD=ON          |
| Tensorflow                       | TFApplyGraph<br/>TFConvertImageToTensor<br/>TFExtractTopLabels<br/>                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             | -DENABLE_TENSORFLOW=ON       |
| USB Camera                       | [GetUSBCamera](PROCESSORS.md#getusbcamera)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      | -DENABLE_USB_CAMERA=ON       |
| Windows Event Log (Windows only) | [CollectorInitiatedSubscription](PROCESSORS.md#collectorinitiatedsubscription)<br/>[ConsumeWindowsEventLog](PROCESSORS.md#consumewindowseventlog)<br/>[TailEventLog](PROCESSORS.md#taileventlog)                                                                                                                                                                                                                                                                                                                                                                                                                                                | -DENABLE_WEL=ON              |

 Please see our [Python guide](extensions/script/README.md) on how to write Python processors and use them within MiNiFi C++.

## Caveats
* We follow semver with regards to API compatibility, but no ABI compatibility is provided. See [semver's website](https://semver.org/) for more information
* Build and usage currently only supports Windows, Linux and OS X environments. MiNiFi C++ can be built and run through the Windows Subsystem for Linux but we provide no support for this platform.
* Provenance events generation is supported and are persisted using RocksDB. Volatile repositories can be used on systems without persistent storage.
* If MiNiFi C++ is built with the OPC-UA extension enabled, it bundles [open62541](https://open62541.org/), which is available under the Mozilla Public License Version 2.0, a Category B license under [ASF 3rd party license policy](https://www.apache.org/legal/resolved.html#category-b).
* If MiNiFi C++ packaged on Windows, the resulting MSI may not be publicly redistributed under the Apache license, because it contains Microsoft redistributable DLLs, which fall under Category X of the [ASF 3rd party license policy](https://www.apache.org/legal/resolved.html#category-x).

## System Requirements

### To build

#### Utilities
* CMake 3.17 or greater
* gcc 11 or greater
* bison 3.0.x+ (3.2 has been shown to fail builds)
* flex 2.6 or greater

##### External Projects

The following utilities are needed to build external projects, when bundled
versions of OpenSSL, cURL, or zlib are used:

* patch
* autoconf
* automake
* libtool

**NOTE** if Expression Language support is enabled, FlexLexer must be in the include path and the version must be compatible with the version of flex used when generating lexer sources. On Mac OS X and FreeBSD, make sure that the system version is overridden with the correct flex version, which may require merging your CPPFLAGS into CFLAGS and CXXFLAGS. Lexer source generation is automatically performed during CMake builds. To re-generate the sources, remove:

 * extensions/expression-language/Parser.cpp
 * extensions/expression-language/Parser.hpp
 * extensions/expression-language/Scanner.cpp
 * extensions/expression-language/location.hh
 * extensions/expression-language/position.hh
 * extensions/expression-language/stack.hh

and rebuild.

#### Libraries / Development Headers
* libcurl-openssl (If not available or desired, NSS will be used)
* libuuid and uuid-dev
* openssl
* Python 3 and development headers -- Required if Python support is enabled
* Lua and development headers -- Required if Lua support is enabled
* libgps-dev -- Required if building libGPS support
* Zlib headers
* perl -- Required for OpenSSL configuration
* NASM -- Required for OpenSSL only on Windows

**NOTE** On Windows if Strawberry Perl is used the `${StrawberryPerlRoot}\c\bin` directory should not be part of the %PATH% variable as Strawberry Perl's patch.exe will be found as the patch executable in the configure phase instead if the git patch executable.

#### CentOS 7

Additional environmental preparations are required for CentOS 7 support. Before
building, install and enable the devtoolset-10 SCL:

```
$ sudo yum install centos-release-scl
$ sudo yum install devtoolset-10
$ scl enable devtoolset-10 bash
```

Finally, it is required to add the `-lrt` compiler flag by using the
`-DCMAKE_CXX_FLAGS=-lrt` flag when invoking cmake.

On all distributions please use -DUSE_SHARED_LIBS=OFF to statically link zlib, libcurl, and OpenSSL.

#### Windows
  Build and Installation has been tested with Windows 10 using Visual Studio 2019.
  You can build and create an MSI via the CPACK command. This requires the installation of the WiX
  toolset (http://wixtoolset.org/). To do this, open up a prompt into your build directory and
  type 'cpack' . The CPACK command will automatically generate and provide you a path to the distributable
  msi file. See [Windows.md](Windows.md) for more details.

### To run

#### Libraries
* libuuid
* librocksdb (built and statically linked)
* libcurl-openssl (If not available or desired, NSS will be used)
* libssl and libcrypto from openssl (built and statically linked)
* libarchive (built and statically linked)
* librdkafka (built and statically linked)
* Python 3 -- Required if Python support is enabled
* Lua -- Required if Lua support is enabled
* libusb -- Optional, unless USB Camera support is enabled
* libpng -- Optional, unless USB Camera support is enabled
* libpcap -- Optional, unless ENABLE_PCAP specified

The needed dependencies can be installed with the following commands for:

##### rpm-based Linux Distributions

**NOTE** it is recommended to use the newest compiler using the latest devtoolset-*/gcc-toolset-*/llvm-toolset-* packages from the Software Collections (SCL).

```
# ~/Development/code/apache/nifi-minifi-cpp on git:master
dnf install cmake \
  gcc gcc-c++ \
  git \
  bison \
  flex \
  patch \
  autoconf \
  automake \
  libtool \
  libuuid libuuid-devel \
  openssl-devel \
  bzip2-devel \
  xz-devel \
  doxygen \
  zlib-devel
# (Optional) for building OpenCV or Bustache support
dnf install boost-devel
# (Optional) for building Python support
dnf install python36-devel
# (Optional) for building Lua support
dnf install lua-devel
# (Optional) for building USB Camera support
dnf install libusb-devel libpng-devel
# (Optional) for building docker image
dnf install docker
# (Optional) for system integration tests
dnf install docker python-virtualenv
# If building with GPS support
dnf install gpsd-devel
# (Optional) for PacketCapture Processor
dnf install libpcap-devel
```

##### Aptitude based Linux Distributions
```
# ~/Development/code/apache/nifi-minifi-cpp on git:master
apt install cmake \
  gcc g++ \
  git \
  bison \
  flex \
  patch \
  ca-certificates \
  autoconf \
  automake \
  libtool \
  libcurl4-openssl-dev \
  uuid-dev uuid \
  libssl-dev \
  libbz2-dev liblzma-dev \
  doxygen \
  zlib1g-dev
# (Optional) for building OpenCV or Bustache support
apt install libboost-all-dev
# (Optional) for building Python support
apt install libpython3-dev
# (Optional) for building Lua support
apt install liblua5.1-0-dev
# (Optional) for building USB Camera support
apt install libusb-1.0.0-0-dev libpng12-dev
# (Optional) for building docker image
apt install docker.io
# (Optional) for system integration tests
apt install docker.io python-virtualenv
# (Optional) If building with GPS support
apt install libgps-dev
# (Optional) for PacketCapture Processor
apt install libpcap-dev
```

##### OS X Using Homebrew (with XCode Command Line Tools installed)
```
# ~/Development/code/apache/nifi-minifi-cpp on git:master
brew install cmake \
  flex \
  patch \
  autoconf \
  automake \
  libtool \
  ossp-uuid \
  openssl \
  python \
  lua \
  xz \
  bzip2 \
  doxygen \
  zlib
brew install curl
brew link curl --force
# (Optional) for building OpenCV or Bustache support
brew install boost
# (Optional) for building USB Camera support
brew install libusb libpng
# (Optional) for building docker image/running system integration tests
# Install docker using instructions at https://docs.docker.com/docker-for-mac/install/
sudo pip install virtualenv
# If building with GPS support
brew install gpsd
# (Optional) for PacketCapture Processor
sudo brew install libpcap
# It is recommended that you install bison from source as HomeBrew now uses an incompatible version of Bison
```


## Getting Started

### Bootstrapping

- MiNiFi C++ offers a bootstrap script in the root of our github repo that will bootstrap the cmake and build process for you without the need to install dependencies yourself. To use this
  process, please run the command `bootstrap.sh` from the root of the MiNiFi C++ source tree.

- Per the table, below, you will be presented with a menu guided bootstrap process. You may enable and disable extensions ( further defined below ). Once you are finished selecting the features
  you wish to build, enter P to continue with the process. CMAKE dependencies will be resolved for your distro. You may enter command line options -n to force yes to all prompts
  (including the package installation prompts ) and -b to automatically run make once the cmake process is complete. Alternatively, you may include the package argument to bootstrap, -p,
  which will run make package.

- If you provide -b or -p to bootstrap.sh, you do not need to follow the Building section, below. If you do not provide these arguments you may skip the cmake .. section from Building.

- Using the Release build profile is recommended to reduce binary size. (~200 MB vs ~30 MB)

  ```
  # ~/Development/code/apache/nifi-minifi-cpp on git:master
  $ ./bootstrap.sh
  # CMAKE Build dir exists, should we overwrite your build directory before we begin?
    If you have already bootstrapped, bootstrapping again isn't necessary to run make [ Y/N ] Y
    ****************************************
     Select MiNiFi C++ Features to toggle.
    ****************************************
    A. Persistent Repositories .....Enabled
    B. libcurl features ............Enabled
    C. libarchive features .........Enabled
    D. Python Scripting support ....Disabled
    E. Expression Language support .Enabled
    F. Kafka support ...............Enabled
    G. PCAP support ................Disabled
    H. USB Camera support ..........Disabled
    I. GPS support .................Disabled
    J. TensorFlow Support ..........Disabled
    K. Bustache Support ............Disabled
    L. Lua Scripting Support .......Disabled
    M. MQTT Support ................Disabled
    N. COAP Support ................Disabled
    O. SFTP Support ................Disabled
    S. AWS Support .................Enabled
    T. OpenCV Support ..............Disabled
    U. OPC-UA Support...............Disabled
    V. SQL Support..................Enabled
    W. Openwsman Support ...........Disabled
    X. Azure Support ...............Enabled
    Y. Systemd Support .............Enabled
    Z. NanoFi Support ..............Disabled
    AA. Splunk Support .............Enabled
    AB. Kubernetes Support .........Disabled
    AC. Google Cloud Support .......Enabled
    AD. ProcFs Support .............Enabled
    AE. Prometheus Support .........Disabled
    AF. Elasticsearch Support ......Disabled
    ****************************************
                Build Options.
    ****************************************
    1. Enable Tests ................Enabled
    2. Enable all extensions
    3. Enable JNI Support ..........Disabled
    4. Use Shared Dependency Links .Enabled
    5. Build Profile ...............RelWithDebInfo Debug MinSizeRel Release
    6. Create ASAN build ...........Disabled
    7. Treat warnings as errors.....Disabled
    P. Continue with these options
    Q. Quit
    * Extension cannot be installed due to
      version of cmake or other software, or
      incompatibility with other extensions

    Enter choice [A-Z or AA-AF or 1-7]
  ```

- Bootstrap now saves state between runs. State will automatically be saved. Provide -c or --clear to clear this state. The -i option provides a guided menu install with the ability to change
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
  $ make -j$(nproc)
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

- Create a binary assembly located in your build directory with suffix .tar.gz
  ```
  ~/Development/code/apache/nifi-minifi-cpp/build
  $ make package
  Run CPack packaging tool for source...
  CPack: Create package using TGZ
  CPack: Install projects
  CPack: - Install directory: ~/Development/code/apache/nifi-minifi-cpp
  CPack: Create package
  CPack: - package: ~/Development/code/apache/nifi-minifi-cpp/build/nifi-minifi-cpp-0.15.0.tar.gz generated.
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
  CPack: - package: ~/Development/code/apache/nifi-minifi-cpp/build/nifi-minifi-cpp-0.15.0-source.tar.gz generated.
  ```

### Building a docker image

#### Building your own custom image
You can create a custom docker image using the cmake configuration to specify which extensions should be included in the final image. Use `-DDOCKER_BUILD_ONLY=ON` to skip local environment checks of cmake.
```
~/Development/code/apache/nifi-minifi-cpp/build
$ cmake -DENABLE_JNI=OFF -DENABLE_AWS=ON -DENABLE_LIBRDKAFKA=ON -DENABLE_MQTT=ON -DENABLE_AZURE=ON -DENABLE_SQL=ON -DDOCKER_BUILD_ONLY=ON ..
$ make docker
```

#### Building the minimal/cloud image
There is also on option to build the minimal image which only contains the most common extensions used in cloud environments. That image includes the standard MiNiFi processors and the AWS, Azure and Kafka extensions. This is also the image which is released to the [apache/nifi-minifi-cpp](https://hub.docker.com/r/apache/nifi-minifi-cpp) repository on dockerhub.
```
~/Development/code/apache/nifi-minifi-cpp/build
$ cmake -DDOCKER_BUILD_ONLY=ON ..
$ make docker-minimal
```

#### Executing integration tests with your docker image
You can execute system integration tests using the docker image built locally on a docker daemon running locally. The image shall contain every extension tested in the test suite for all scenarios to be executed (currently that includes AWS, Azure, Kafka, MQTT, SQL extensions).
```
~/Development/code/apache/nifi-minifi-cpp/build
$ make docker-verify
```

### Building For Other Distros
If you have docker installed on your machine you can build for CentOS 7, Fedora 34, Ubuntu 18.04, and Ubuntu 20.04 via our make docker commands. The following table
provides the command to build your distro and the output file in your build directory. Since the versions are limited ( except for Ubuntu ) we output the archive based on the distro's name.


| Distro                | command     | Output File                            |
|-----------------------|:------------|:---------------------------------------|
| CentOS 7              | make centos | nifi-minifi-cpp-centos-$VERSION.tar.gz |
| Fedora 34             | make fedora | nifi-minifi-cpp-fedora-$VERSION.tar.gz |
| Ubuntu 18.04 (bionic) | make u18    | nifi-minifi-cpp-bionic-$VERSION.tar.gz |
| Ubuntu 20.04 (focal)  | make u20    | nifi-minifi-cpp-focal-$VERSION.tar.gz  |

You can avoid the requirement of an up-to-date compiler when generating the build system by adding `-DDOCKER_BUILD_ONLY=ON` to the cmake command line. This disables all cmake targets except the docker build and test ones.

### Installation
After [building](#building) MiNiFi C++, extract the generated binary package 'nifi-minifi-cpp-$VERSION.tar.gz' at your desired installation path.
```shell
$ MINIFI_PACKAGE="$(pwd)"/build/nifi-minifi-cpp-*.tar.gz
$ pushd /opt
$ sudo tar xvzf "$MINIFI_PACKAGE"
$ cd nifi-minifi-cpp-*
```

### Configuring
The 'conf' directory in the installation root contains a template config.yml document, minifi.properties, and minifi-log.properties. Please see our [Configuration document](CONFIGURE.md) for details on how to configure agents.

### Running
After completing the [installation](#installation), the application can be run by issuing the following command from the installation directory:

    $ ./bin/minifi.sh start

By default, this will make use of a config.yml located in the conf directory.  This configuration file location can be altered by adjusting the property `nifi.flow.configuration.file` in minifi.properties located in the conf directory.

### Stopping

MiNiFi can then be stopped by issuing:

    $ ./bin/minifi.sh stop

### Installing as a service

MiNiFi can also be installed as a system service using minifi.sh with an optional "service name" (default: minifi)

    $ ./bin/minifi.sh install [service name]

### Running as a docker container
You can use the officially released image pulled from the [apache/nifi-minifi-cpp](https://hub.docker.com/r/apache/nifi-minifi-cpp) repository on dockerhub or you can use your locally built image.
The container can be run with a specific configuration by mounting the locally edited configuration files to your docker container.
```
$ docker run -v ~/Development/apache/nifi-minifi-cpp/conf/config.yml:/opt/minifi/minifi-current/conf/config.yml -v ~/Development/apache/nifi-minifi-cpp/conf/minifi.properties:/opt/minifi/minifi-current/conf/minifi.properties apache/nifi-minifi-cpp
```

### Deploying
MiNiFi C++ comes with a deployment script. This will build and package minifi. Additionally, a file named build_output will be
created within the build directory that contains a manifest of build artifacts.

    $ deploy.sh <build identifier>

The build identifier will be carried with the deployed binary for the configuration you specify. By default all extensions will be built.

On Windows it is suggested that MSI be used for installation.

### Cleaning
Remove the build directory created above.
```
# ~/Development/code/apache/nifi-minifi-cpp on git:master
$ rm -rf ./build
```


### Extensions

Please see [Extensions.md](Extensions.md) on how to build and run conditionally built dependencies and extensions.

### Security

For securing a MiNiFi agent's configuration files it comes with a tool called `encrypt-config`. Its documentation is available [here](https://cwiki.apache.org/confluence/display/MINIFI/Securing+MiNiFi+configuration+files).

### Recommended Antivirus Exclusions

Antivirus software can take a long time to scan directories and the files within them. Additionally, if the antivirus software locks files or directories during a scan, those resources are unavailable to MiNiFi processes, causing latency or unavailability of these resources in a MiNiFi instance. To prevent these performance and reliability issues from occurring, it is highly recommended to configure your antivirus software to skip scans on the following MiNiFi C++ directories:

- content_repository
- flowfile_repository
- provenance_repository

## Operations
See our [operations documentation for additional information on how to manage instances](OPS.md)

## Monitoring
See our [metrics documentation for information about self published metrics](METRICS.md)

## Issue Tracking
See https://issues.apache.org/jira/projects/MINIFICPP/issues for the issue tracker.

## Documentation
See https://nifi.apache.org/minifi for the latest documentation.

## Examples
See our [examples page](examples/README.md) for flow examples.

## Contributing
See our [Contribution Guide](CONTRIB.md).

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
