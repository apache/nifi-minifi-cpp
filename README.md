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

# Apache NiFi -  MiNiFi - C++ [![MiNiFi-CPP CI](https://github.com/apache/nifi-minifi-cpp/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/apache/nifi-minifi-cpp/actions/workflows/ci.yml?query=workflow%3A%22MiNiFi-CPP+CI%22+branch%3Amain)

MiNiFi is a child project effort of Apache NiFi.  This repository is for a native implementation in C++.

## Table of Contents

- [Features](#features)
- [Caveats](#caveats)
- [Getting Started](#getting-started)
  - [System Requirements](#system-requirements)
  - [Bootstrapping](#bootstrapping)
  - [Building For Other Distros](#building-for-other-distros)
  - [Installation](#installation)
  - [Tests](#tests)
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

| Extension Set | Processors                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
|---------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Base**      | [AppendHostInfo](PROCESSORS.md#appendhostinfo)<br/>[AttributesToJSON](PROCESSORS.md#attributestojson)<br/>[DefragmentText](PROCESSORS.md#defragmenttext)<br/>[ExecuteProcess](PROCESSORS.md#executeprocess)<br/>[ExtractText](PROCESSORS.md#extracttext)<br/>[FetchFile](PROCESSORS.md#fetchfile)<br/>[GenerateFlowFile](PROCESSORS.md#generateflowfile)<br/>[GetFile](PROCESSORS.md#getfile)<br/>[GetTCP](PROCESSORS.md#gettcp)<br/>[HashContent](PROCESSORS.md#hashcontent)<br/>[InvokeHTTP](PROCESSORS.md#invokehttp)<br/>[ListenSyslog](PROCESSORS.md#listensyslog)<br/>[ListenTCP](PROCESSORS.md#listentcp)<br/>[ListenUDP](PROCESSORS.md#listenudp)<br/>[ListFile](PROCESSORS.md#listfile)<br/>[LogAttribute](PROCESSORS.md#logattribute)<br/>[PutFile](PROCESSORS.md#putfile)<br/>[PutTCP](PROCESSORS.md#puttcp)<br/>[PutUDP](PROCESSORS.md#putudp)<br/>[ReplaceText](PROCESSORS.md#replacetext)<br/>[RetryFlowFile](PROCESSORS.md#retryflowfile)<br/>[RouteOnAttribute](PROCESSORS.md#routeonattribute)<br/>[RouteText](PROCESSORS.md#routetext)<br/>[SegmentContent](PROCESSORS.md#segmentcontent)<br/>[SplitContent](PROCESSORS.md#splitcontent)<br/>[SplitText](PROCESSORS.md#splittext)<br/>[TailFile](PROCESSORS.md#tailfile)<br/>[UpdateAttribute](PROCESSORS.md#updateattribute) |

The next table outlines CMAKE flags that correspond with MiNiFi extensions. Extensions that are enabled by default ( such as RocksDB ), can be disabled with the respective CMAKE flag on the command line.

| Extension Set                    | Processors and Controller Services                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              | CMAKE Flag                   |
|----------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-----------------------------|
| Archive Extensions               | [ApplyTemplate](PROCESSORS.md#applytemplate)<br/>[BinFiles](PROCESSORS.md#binfiles)<br/>[CompressContent](PROCESSORS.md#compresscontent)<br/>[ManipulateArchive](PROCESSORS.md#manipulatearchive)<br/>[MergeContent](PROCESSORS.md#mergecontent)<br/>[FocusArchiveEntry](PROCESSORS.md#focusarchiveentry)<br/>[UnfocusArchiveEntry](PROCESSORS.md#unfocusarchiveentry)                                                                                                                                                                                                                                                                          | -DBUILD_LIBARCHIVE=ON        |
| AWS                              | [AWSCredentialsService](CONTROLLERS.md#awscredentialsservice)<br/>[PutS3Object](PROCESSORS.md#puts3object)<br/>[DeleteS3Object](PROCESSORS.md#deletes3object)<br/>[FetchS3Object](PROCESSORS.md#fetchs3object)<br/>[ListS3](PROCESSORS.md#lists3)<br/>[PutKinesisStream](PROCESSORS.md#putkinesisstream)                                                                                                                                                                                                                                                                                                                                        | -DENABLE_AWS=ON              |
| Azure                            | [AzureStorageCredentialsService](CONTROLLERS.md#azurestoragecredentialsservice)<br/>[PutAzureBlobStorage](PROCESSORS.md#putazureblobstorage)<br/>[DeleteAzureBlobStorage](PROCESSORS.md#deleteazureblobstorage)<br/>[FetchAzureBlobStorage](PROCESSORS.md#fetchazureblobstorage)<br/>[ListAzureBlobStorage](PROCESSORS.md#listazureblobstorage)<br/>[PutAzureDataLakeStorage](PROCESSORS.md#putazuredatalakestorage)<br/>[DeleteAzureDataLakeStorage](PROCESSORS.md#deleteazuredatalakestorage)<br/>[FetchAzureDataLakeStorage](PROCESSORS.md#fetchazuredatalakestorage)<br/>[ListAzureDataLakeStorage](PROCESSORS.md#listazuredatalakestorage) | -DENABLE_AZURE=ON            |
| CivetWeb                         | [ListenHTTP](PROCESSORS.md#listenhttp)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | -DENABLE_CIVET=ON            |
| Couchbase                        | [CouchbaseClusterService](CONTROLLERS.md#couchbaseclusterservice)<br/>[PutCouchbaseKey](PROCESSORS.md#putcouchbasekey)<br/>[GetCouchbaseKey](PROCESSORS.md#getcouchbasekey)                                                                                                                                                                                                                                                                                                                                                                                                                                                                     | -DENABLE_COUCHBASE=ON        |
| Elasticsearch                    | [ElasticsearchCredentialsControllerService](CONTROLLERS.md#elasticsearchcredentialscontrollerservice)<br/>[PostElasticsearch](PROCESSORS.md#postelasticsearch)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | -DENABLE_ELASTICSEARCH=ON    |
| ExecuteProcess (Linux and macOS) | [ExecuteProcess](PROCESSORS.md#executeprocess)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | -DENABLE_EXECUTE_PROCESS=ON  |
| Google Cloud Platform            | [DeleteGCSObject](PROCESSORS.md#deletegcsobject)<br>[FetchGCSObject](PROCESSORS.md#fetchgcsobject)<br>[GCPCredentialsControllerService](CONTROLLERS.md#gcpcredentialscontrollerservice)<br>[ListGCSBucket](PROCESSORS.md#listgcsbucket)<br>[PutGCSObject](PROCESSORS.md#putgcsobject)                                                                                                                                                                                                                                                                                                                                                           | -DENABLE_GCP=ON              |
| Grafana Loki                     | [PushGrafanaLokiREST](PROCESSORS.md#pushgrafanalokirest)<br>[PushGrafanaLokiGrpc](PROCESSORS.md#pushgrafanalokigrpc)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            | -DENABLE_GRAFANA_LOKI=ON     |
| Kafka                            | [PublishKafka](PROCESSORS.md#publishkafka)<br>[ConsumeKafka](PROCESSORS.md#consumekafka)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        | -DENABLE_KAFKA=ON            |
| Kubernetes (Linux)               | [KubernetesControllerService](CONTROLLERS.md#kubernetescontrollerservice)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       | -DENABLE_KUBERNETES=ON       |
| LlamaCpp                         | [RunLlamaCppInference](PROCESSORS.md#runllamacppinference)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      | -DENABLE_LLAMACPP=ON         |
| Lua Scripting                    | [ExecuteScript](PROCESSORS.md#executescript)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | -DENABLE_LUA_SCRIPTING=ON    |
| MQTT                             | [ConsumeMQTT](PROCESSORS.md#consumemqtt)<br/>[PublishMQTT](PROCESSORS.md#publishmqtt)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | -DENABLE_MQTT=ON             |
| OPC                              | [FetchOPCProcessor](PROCESSORS.md#fetchopcprocessor)<br/>[PutOPCProcessor](PROCESSORS.md#putopcprocessor)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       | -DENABLE_OPC=ON              |
| OpenCV                           | [CaptureRTSPFrame](PROCESSORS.md#capturertspframe)<br/>[MotionDetector](PROCESSORS.md#motiondetector)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | -DENABLE_OPENCV=ON           |
| PDH (Windows)                    | [PerformanceDataMonitor](PROCESSORS.md#performancedatamonitor)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | -DENABLE_PDH=ON              |
| ProcFs (Linux)                   | [ProcFsMonitor](PROCESSORS.md#procfsmonitor)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | -DENABLE_PROCFS=ON           |
| Python Scripting                 | [ExecuteScript](PROCESSORS.md#executescript)<br/>[**Custom Python Processors**](extensions/python/PYTHON.md)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | -DENABLE_PYTHON_SCRIPTING=ON |
| SMB (Windows)                    | [FetchSmb](PROCESSORS.md#fetchsmb)<br/>[ListSmb](PROCESSORS.md#listsmb)<br/>[PutSmb](PROCESSORS.md#putsmb)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      | -DENABLE_SMB=ON              |
| SFTP                             | [FetchSFTP](PROCESSORS.md#fetchsftp)<br/>[ListSFTP](PROCESSORS.md#listsftp)<br/>[PutSFTP](PROCESSORS.md#putsftp)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | -DENABLE_SFTP=ON             |
| SQL                              | [ExecuteSQL](PROCESSORS.md#executesql)<br/>[PutSQL](PROCESSORS.md#putsql)<br/>[QueryDatabaseTable](PROCESSORS.md#querydatabasetable)<br/>                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       | -DENABLE_SQL=ON              |
| Splunk                           | [PutSplunkHTTP](PROCESSORS.md#putsplunkhttp)<br/>[QuerySplunkIndexingStatus](PROCESSORS.md#querysplunkindexingstatus)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | -DENABLE_SPLUNK=ON           |
| Systemd (Linux)                  | [ConsumeJournald](PROCESSORS.md#consumejournald)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | -DENABLE_SYSTEMD=ON          |
| Windows Event Log (Windows)      | [ConsumeWindowsEventLog](PROCESSORS.md#consumewindowseventlog)<br/>[TailEventLog](PROCESSORS.md#taileventlog)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   | -DENABLE_WEL=ON              |

 Please see our [Python guide](extensions/python/PYTHON.md) on how to write Python processors and use them within MiNiFi C++.

### Site-to-Site communication

Communicating with Apache NiFi can be done using the Site-to-Site protocol. More information on configuring it in MiNiFi C++ can be found in the [Site-to-Site documentation](SITE_TO_SITE.md).

## Caveats
* We follow semver with regards to API compatibility, but no ABI compatibility is provided. See [semver's website](https://semver.org/) for more information
* Build and usage currently only supports Windows, Linux and macOS environments. MiNiFi C++ can be built and run through the Windows Subsystem for Linux but we provide no support for this platform.
* Provenance events generation is supported and are persisted using RocksDB. Volatile repositories can be used on systems without persistent storage.
* If MiNiFi C++ is built with the OPC-UA extension enabled, it bundles [open62541](https://open62541.org/), which is available under the Mozilla Public License Version 2.0, a Category B license under [ASF 3rd party license policy](https://www.apache.org/legal/resolved.html#category-b).
* If MiNiFi C++ is built using one of the following flags: -DMINIFI_INCLUDE_VC_REDIST_MERGE_MODULES=ON, -DMINIFI_INCLUDE_VC_REDIST_DLLS=ON, -DMINIFI_INCLUDE_UCRT_DLLS=ON, and packaged on Windows, the resulting MSI may not be publicly redistributed under the Apache license, because it contains Microsoft redistributable DLLs, which fall under Category X of the [ASF 3rd party license policy](https://www.apache.org/legal/resolved.html#category-x).

## System Requirements

### To build

#### Utilities
* CMake 3.24 or greater
* Compiler
  * g++ 12 or greater
  * clang 16 or greater
  * msvc 19.33 or greater
* bison 3.0.x+ (3.2 has been shown to fail builds)
* flex 2.6 or greater

##### External Projects

The following utilities are needed to build external projects, when bundled
versions of OpenSSL, cURL, or zlib are used:

* patch
* autoconf
* automake
* libtool

**NOTE:** if Expression Language support is enabled, FlexLexer must be in the include path and the version must be compatible with the version of flex used when generating lexer sources. On macOS and FreeBSD, make sure that the system version is overridden with the correct flex version, which may require merging your CPPFLAGS into CFLAGS and CXXFLAGS. Lexer source generation is automatically performed during CMake builds. To re-generate the sources, remove:

 * build/el-generated/Parser.cpp
 * build/el-generated/Parser.hpp
 * build/el-generated/Scanner.cpp
 * build/el-generated/location.hh
 * build/el-generated/position.hh
 * build/el-generated/stack.hh

and rebuild.

#### System Libraries / Development Headers Required
* Python 3 and development headers -- Required if Python support is enabled
* perl -- Required for OpenSSL configuration
* NASM -- Required for OpenSSL only on Windows
* jom (optional) -- for parallel build of OpenSSL on Windows

**NOTE:** On Windows if Strawberry Perl is used the `${StrawberryPerlRoot}\c\bin` directory should not be part of the PATH environment variable as the patch executable in this directory interferes with git's patch executable. Alternatively [scoop](https://scoop.sh/) package manager can also be used to install Strawberry Perl using the command `scoop install perl` that does not pollute the PATH variable. Also on Windows CMake's CPack is used for MSI generation, building WIX files and calling WIX toolset tools to create an MSI. If Chocolatey package manager is used its CPack can conflict with CMake, so make sure that CMake's CPack is found in the %PATH% before that.

#### Bundled Thirdparty Dependencies

The [NOTICE](NOTICE) file lists all the bundled thirdparty dependencies that are built and linked statically to MiNiFi or one of its extensions. The licenses of these projects can be found in the [LICENSE](LICENSE) file.

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
  Build and Installation has been tested with Windows 10 using Visual Studio 2022.
  You can build and create an MSI via the CPACK command. This requires the installation of the WiX
  toolset (http://wixtoolset.org/). To do this, open up a prompt into your build directory and
  type 'cpack' . The CPACK command will automatically generate and provide you a path to the distributable
  msi file. See [Windows.md](Windows.md) for more details.

### To run

#### System Libraries Required
* Python 3 -- Required if Python support is enabled

The needed dependencies can be installed with the following commands for:

##### rpm-based Linux Distributions

**NOTE:** it is recommended to use the newest compiler using the latest devtoolset-*/gcc-toolset-*/llvm-toolset-* packages from the Software Collections (SCL).

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
# (Optional) for building Python support
dnf install python36-devel
# (Optional) for building docker image
dnf install docker
# (Optional) for system integration tests
dnf install docker python-virtualenv
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
# (Optional) for building Python support
apt install libpython3-dev
# (Optional) for building docker image
apt install docker.io
# (Optional) for system integration tests
apt install docker.io python-virtualenv
```

##### macOS Using Homebrew (with XCode Command Line Tools installed)
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
  xz \
  bzip2 \
  doxygen \
  zlib
brew install curl
brew link curl --force
# (Optional) for building docker image/running system integration tests
# Install docker using instructions at https://docs.docker.com/docker-for-mac/install/
sudo pip install virtualenv
# It is recommended that you install bison from source as HomeBrew now uses an incompatible version of Bison
```


## Getting Started

### Bootstrapping

MiNiFi C++ offers bootstrap scripts that will bootstrap the cmake and build process for you without the need to install dependencies yourself.

#### Python based bootstrapping (recommended)
##### Linux
Prerequisites:
- [python](https://docs.python.org/)
- [venv](https://docs.python.org/3/library/venv.html)

```bash
./bootstrap/py_bootstrap.sh
```

#### macOS
Prerequisites:
- [python](https://docs.python.org/)
- [venv](https://docs.python.org/3/library/venv.html)
- [Homebrew](https://brew.sh/)
```bash
./bootstrap/py_bootstrap.sh
```

#### Windows
Prerequisites:
- [python](https://docs.python.org/)
- [venv](https://docs.python.org/3/library/venv.html)
- [chocolatey](https://chocolatey.org/)
```dos
.\bootstrap\py_bootstrap.bat
```

This will set up a virtual environment in the bootstrap folder, and guide you through the build process.

### Building

#### Build MiNiFi using Standalone CMake

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

#### Create MiNiFi Package using Standalone CMake

- Create a binary assembly located in your build directory with suffix .tar.gz
  ```
  ~/Development/code/apache/nifi-minifi-cpp/build
  $ make package
  Run CPack packaging tool for source...
  CPack: Create package using TGZ
  CPack: Install projects
  CPack: - Install directory: ~/Development/code/apache/nifi-minifi-cpp
  CPack: Create package
  CPack: - package: ~/Development/code/apache/nifi-minifi-cpp/build/nifi-minifi-cpp-0.99.0.tar.gz generated.
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
  CPack: - package: ~/Development/code/apache/nifi-minifi-cpp/build/nifi-minifi-cpp-0.99.0-source.tar.gz generated.
  ```

#### Build MiNiFi & Create MiNiFi Package using Conan v2

Building MiNiFi and creating MiNiFi package supporting a portion of the extensions has been tested with Conan version 2 using VS code as an alternative to standalone CMake. By building MiNiFi using prebuilt conan packages for the external libraries as an alternative to CMake building the sources of those external libraries, we maybe able to speed up MiNiFi builds. Additionally, by creating a MiNiFi package as a conan package, it should be easier to integrate MiNiFi library and its executables into other C++
project infrastructure to build out data pipelines on the edge. For instance, once we create the MiNiFi conan package, we can upload it to jfrog, conancenter or some other supported conan cloud repository and then download the prebuilt MiNiFi conan package to our new C++ project by adding it to our conanfile. For more details on the conan commands to build MiNiFi and create a MiNiFi conan package, see [CONAN.md](CONAN.md).

### Building a docker image

#### Building your own custom image
You can create a custom docker image using the cmake configuration to specify which extensions should be included in the final image. Use `-DDOCKER_BUILD_ONLY=ON` to skip local environment checks of cmake.
```
~/Development/code/apache/nifi-minifi-cpp/build
$ cmake -DENABLE_AWS=ON -DENABLE_KAFKA=ON -DENABLE_MQTT=ON -DENABLE_AZURE=ON -DENABLE_SQL=ON -DDOCKER_BUILD_ONLY=ON ..
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
You can execute system integration tests using a minifi docker image.</br>
Currently, there are two types of docker integration tests:
##### Monolith legacy tests (features locates in docker/test/integration/features)
(we are in the process of migrating these)
  ```
  ~/Development/code/apache/nifi-minifi-cpp/build
  $ make docker-verify
  ```
##### Modular tests located near the tested extension (e.g. extensions/aws/tests/features)
  ```
  ~/Development/code/apache/nifi-minifi-cpp/build
  $ make docker-verify-modular
  ```
You can also run these tests individually. You need to install the minifi behave framework and call behavex.
This will use the apacheminificpp:behave docker image
  ```
  python -m venv .venv
  source .venv/bin/activate
  pip install -e behave_framework
  cd extensions/aws/tests
  behavex
  ```

### Building For Other Distros
Since only glibc and libstdc++ is dynamically linked to MiNiFi, the binary built with RHEL devtoolset should be distro-agnostic, and work on any relatively modern distro.

| Distro        | command     | Output File                                |
|---------------|:------------|:-------------------------------------------|
| Rockylinux 8  | make rocky  | nifi-minifi-cpp-rockylinux-$VERSION.tar.gz |

You can avoid the requirement of an up-to-date compiler when generating the build system by adding `-DDOCKER_BUILD_ONLY=ON` to the cmake command line. This disables all cmake targets except the docker build and test ones.

### Installation
After [building](#building) MiNiFi C++, extract the generated binary package 'nifi-minifi-cpp-$VERSION.tar.gz' at your desired installation path.
```shell
$ MINIFI_PACKAGE="$(pwd)"/build/nifi-minifi-cpp-*.tar.gz
$ pushd /opt
$ sudo tar xvzf "$MINIFI_PACKAGE"
$ cd nifi-minifi-cpp-*
```

### Tests
If you have enabled tests to be built during the bootstrap process, you can run them
in the build directory. (See `ctest --help` for selecting tests or running them in parallel)
```
$ ctest --output-on-failure
```

The performance tests can similarly be enabled. To execute them and see their output.

```
$ ctest --verbose -L performance
```


### Configuring
The 'conf' directory in the installation root contains all configuration files.

The files conf/minifi.properties, conf/minifi-log.properties and conf/minifi-uid.properties contain key-value pair configuration settings;
these are the default settings supplied by the latest MiNiFi version. If you would like to modify these, you should create a corresponding
.d directory (e.g. conf/minifi.properties.d) and put your settings in a new file inside this directory. These files are read and applied
in lexicographic order, after the default settings file.
The Windows installer creates a conf/minifi.properties.d/10_installer_properties file, which contains C2 connection settings.
If C2 is enabled and settings are added/modified from the C2 server, these will be saved in conf/minifi.properties.d/90_c2_properties.

The conf/config.yml file contains the flow definition (i.e. the layout of processors, controller services etc). When you start MiNiFi for
the first time, the flow will be fetched from the C2 server (if available), or a file containing an empty flow will be created by MiNiFi.

Please see our [Configuration document](CONFIGURE.md) for details on how to configure agents.


### Installing as a service

MiNiFi can also be installed as a system service using minifi.sh:

    $ ./bin/minifi.sh install

### Running
After completing the [installation](#installing-as-a-service), the application can be run by issuing the following command from the installation directory:

    $ ./bin/minifi.sh start

By default, this will make use of a config.yml located in the conf directory. This configuration file location can be altered by adjusting the property `nifi.flow.configuration.file` in minifi.properties located in the conf directory.

### Stopping

MiNiFi can then be stopped by issuing:

    $ ./bin/minifi.sh stop

### Query flow status

To query the status of the flow, you can use the following command on Unix systems:

    $ ./bin/minifi.sh flowStatus [host:optional] [port:optional] "<query>"

On Windows systems, you can use the following command:

    $ ./bin/flowstatus-minifi.bat [host:optional] [port:optional] "<query>"

The query can look like the following: "processor:TailFile:health,stats,bulletins". For more information on the query syntax and options, please see the [flow status documentation](OPS.md#Flowstatus-command).
Note: The command requires minifi controller to be enabled in the minifi.properties file.

### Running as a docker container
You can use the officially released image pulled from the [apache/nifi-minifi-cpp](https://hub.docker.com/r/apache/nifi-minifi-cpp) repository on dockerhub or you can use your locally built image.
The container can be run with a specific configuration by mounting the locally edited configuration files to your docker container.
```
$ docker run -v ~/Development/apache/nifi-minifi-cpp/conf/config.yml:/opt/minifi/minifi-current/conf/config.yml -v ~/Development/apache/nifi-minifi-cpp/conf/minifi.properties:/opt/minifi/minifi-current/conf/minifi.properties apache/nifi-minifi-cpp
```

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
