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
| **Base**      | [AppendHostInfo](PROCESSORS.md#appendhostinfo)<br/>[AttributesToJSON](PROCESSORS.md#attributestojson)<br/>[DefragmentText](PROCESSORS.md#defragmenttext)<br/>[ExecuteProcess](PROCESSORS.md#executeprocess)<br/>[ExtractText](PROCESSORS.md#extracttext)<br/>[FetchFile](PROCESSORS.md#fetchfile)<br/>[GenerateFlowFile](PROCESSORS.md#generateflowfile)<br/>[GetFile](PROCESSORS.md#getfile)<br/>[GetTCP](PROCESSORS.md#gettcp)<br/>[HashContent](PROCESSORS.md#hashcontent)<br/>[InvokeHTTP](PROCESSORS.md#invokehttp)<br/>[ListenSyslog](PROCESSORS.md#listensyslog)<br/>[ListenTCP](PROCESSORS.md#listentcp)<br/>[ListenUDP](PROCESSORS.md#listenudp)<br/>[ListFile](PROCESSORS.md#listfile)<br/>[LogAttribute](PROCESSORS.md#logattribute)<br/>[PutFile](PROCESSORS.md#putfile)<br/>[PutTCP](PROCESSORS.md#puttcp)<br/>[PutUDP](PROCESSORS.md#putudp)<br/>[ReplaceText](PROCESSORS.md#replacetext)<br/>[RetryFlowFile](PROCESSORS.md#retryflowfile)<br/>[RouteOnAttribute](PROCESSORS.md#routeonattribute)<br/>[RouteText](PROCESSORS.md#routetext)<br/>[SplitContent](PROCESSORS.md#splitcontent)<br/>[SplitText](PROCESSORS.md#splittext)<br/>[TailFile](PROCESSORS.md#tailfile)<br/>[UpdateAttribute](PROCESSORS.md#updateattribute) |

The next table outlines CMAKE flags that correspond with MiNiFi extensions. Extensions that are enabled by default ( such as RocksDB ), can be disabled with the respective CMAKE flag on the command line.

| Extension Set                    | Processors and Controller Services                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              | CMAKE Flag                   |
|----------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-----------------------------|
| Archive Extensions               | [ApplyTemplate](PROCESSORS.md#applytemplate)<br/>[BinFiles](PROCESSORS.md#binfiles)<br/>[CompressContent](PROCESSORS.md#compresscontent)<br/>[ManipulateArchive](PROCESSORS.md#manipulatearchive)<br/>[MergeContent](PROCESSORS.md#mergecontent)<br/>[FocusArchiveEntry](PROCESSORS.md#focusarchiveentry)<br/>[UnfocusArchiveEntry](PROCESSORS.md#unfocusarchiveentry)                                                                                                                                                                                                                                                                          | -DBUILD_LIBARCHIVE=ON        |
| AWS                              | [AWSCredentialsService](CONTROLLERS.md#awscredentialsservice)<br/>[PutS3Object](PROCESSORS.md#puts3object)<br/>[DeleteS3Object](PROCESSORS.md#deletes3object)<br/>[FetchS3Object](PROCESSORS.md#fetchs3object)<br/>[ListS3](PROCESSORS.md#lists3)                                                                                                                                                                                                                                                                                                                                                                                               | -DENABLE_AWS=ON              |
| Azure                            | [AzureStorageCredentialsService](CONTROLLERS.md#azurestoragecredentialsservice)<br/>[PutAzureBlobStorage](PROCESSORS.md#putazureblobstorage)<br/>[DeleteAzureBlobStorage](PROCESSORS.md#deleteazureblobstorage)<br/>[FetchAzureBlobStorage](PROCESSORS.md#fetchazureblobstorage)<br/>[ListAzureBlobStorage](PROCESSORS.md#listazureblobstorage)<br/>[PutAzureDataLakeStorage](PROCESSORS.md#putazuredatalakestorage)<br/>[DeleteAzureDataLakeStorage](PROCESSORS.md#deleteazuredatalakestorage)<br/>[FetchAzureDataLakeStorage](PROCESSORS.md#fetchazuredatalakestorage)<br/>[ListAzureDataLakeStorage](PROCESSORS.md#listazuredatalakestorage) | -DENABLE_AZURE=ON            |
| CivetWeb                         | [ListenHTTP](PROCESSORS.md#listenhttp)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | -DENABLE_CIVET=ON            |
| Elasticsearch                    | [ElasticsearchCredentialsControllerService](CONTROLLERS.md#elasticsearchcredentialscontrollerservice)<br/>[PostElasticsearch](PROCESSORS.md#postelasticsearch)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | -DENABLE_ELASTICSEARCH=ON    |
| ExecuteProcess (Linux and macOS) | [ExecuteProcess](PROCESSORS.md#executeprocess)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | -DENABLE_EXECUTE_PROCESS=ON  |
| Google Cloud Platform            | [DeleteGCSObject](PROCESSORS.md#deletegcsobject)<br>[FetchGCSObject](PROCESSORS.md#fetchgcsobject)<br>[GCPCredentialsControllerService](CONTROLLERS.md#gcpcredentialscontrollerservice)<br>[ListGCSBucket](PROCESSORS.md#listgcsbucket)<br>[PutGCSObject](PROCESSORS.md#putgcsobject)                                                                                                                                                                                                                                                                                                                                                           | -DENABLE_GCP=ON              |
| Grafana Loki                     | [PushGrafanaLokiREST](PROCESSORS.md#pushgrafanalokirest)<br>[PushGrafanaLokiGrpc](PROCESSORS.md#pushgrafanalokigrpc)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            | -DENABLE_GRAFANA_LOKI=ON     |
| Kafka                            | [PublishKafka](PROCESSORS.md#publishkafka)<br>[ConsumeKafka](PROCESSORS.md#consumekafka)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        | -DENABLE_LIBRDKAFKA=ON       |
| Kubernetes (Linux)               | [KubernetesControllerService](CONTROLLERS.md#kubernetescontrollerservice)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       | -DENABLE_KUBERNETES=ON       |
| Lua Scripting                    | [ExecuteScript](PROCESSORS.md#executescript)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | -DENABLE_LUA_SCRIPTING=ON    |
| MQTT                             | [ConsumeMQTT](PROCESSORS.md#consumemqtt)<br/>[PublishMQTT](PROCESSORS.md#publishmqtt)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | -DENABLE_MQTT=ON             |
| OPC                              | [FetchOPCProcessor](PROCESSORS.md#fetchopcprocessor)<br/>[PutOPCProcessor](PROCESSORS.md#putopcprocessor)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       | -DENABLE_OPC=ON              |
| OpenCV                           | [CaptureRTSPFrame](PROCESSORS.md#capturertspframe)<br/>[MotionDetector](PROCESSORS.md#motiondetector)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | -DENABLE_OPENCV=ON           |
| PDH (Windows)                    | [PerformanceDataMonitor](PROCESSORS.md#performancedatamonitor)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | -DENABLE_PDH=ON              |
| ProcFs (Linux)                   | [ProcFsMonitor](PROCESSORS.md#procfsmonitor)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | -DENABLE_PROCFS=ON           |
| Python Scripting                 | [ExecuteScript](PROCESSORS.md#executescript)<br>[ExecutePythonProcessor](PROCESSORS.md#executepythonprocessor)<br/>**Custom Python Processors**                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 | -DENABLE_PYTHON_SCRIPTING=ON |
| SMB (Windows)                    | [FetchSmb](PROCESSORS.md#fetchsmb)<br/>[ListSmb](PROCESSORS.md#listsmb)<br/>[PutSmb](PROCESSORS.md#putsmb)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      | -DENABLE_SMB=ON              |
| SFTP                             | [FetchSFTP](PROCESSORS.md#fetchsftp)<br/>[ListSFTP](PROCESSORS.md#listsftp)<br/>[PutSFTP](PROCESSORS.md#putsftp)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | -DENABLE_SFTP=ON             |
| SQL                              | [ExecuteSQL](PROCESSORS.md#executesql)<br/>[PutSQL](PROCESSORS.md#putsql)<br/>[QueryDatabaseTable](PROCESSORS.md#querydatabasetable)<br/>                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       | -DENABLE_SQL=ON              |
| Splunk                           | [PutSplunkHTTP](PROCESSORS.md#putsplunkhttp)<br/>[QuerySplunkIndexingStatus](PROCESSORS.md#querysplunkindexingstatus)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | -DENABLE_SPLUNK=ON           |
| Systemd (Linux)                  | [ConsumeJournald](PROCESSORS.md#consumejournald)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | -DENABLE_SYSTEMD=ON          |
| Windows Event Log (Windows)      | [ConsumeWindowsEventLog](PROCESSORS.md#consumewindowseventlog)<br/>[TailEventLog](PROCESSORS.md#taileventlog)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   | -DENABLE_WEL=ON              |

 Please see our [Python guide](extensions/python/PYTHON.md) on how to write Python processors and use them within MiNiFi C++.

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

#### Shell based bootstrapping (linux and macOS)
- Please run the command `bootstrap.sh` from the root of the MiNiFi C++ source tree.

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
    C. libarchive features .........Enabled
    D. Python Scripting support ....Enabled
    E. Expression Language support .Enabled
    F. Kafka support ...............Enabled
    K. Bustache Support ............Disabled
    L. Lua Scripting Support .......Enabled
    M. MQTT Support ................Enabled
    O. SFTP Support ................Disabled
    S. AWS Support .................Enabled
    T. OpenCV Support ..............Disabled
    U. OPC-UA Support...............Enabled
    V. SQL Support..................Enabled
    X. Azure Support ...............Enabled
    Y. Systemd Support .............Enabled
    AA. Splunk Support .............Enabled
    AB. Kubernetes Support .........Enabled
    AC. Google Cloud Support .......Enabled
    AD. ProcFs Support .............Enabled
    AE. Prometheus Support .........Enabled
    AF. Elasticsearch Support ......Enabled
    ****************************************
                Build Options.
    ****************************************
    1. Enable Tests ................Enabled
    2. Enable all extensions
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

### Building a docker image

#### Building your own custom image
You can create a custom docker image using the cmake configuration to specify which extensions should be included in the final image. Use `-DDOCKER_BUILD_ONLY=ON` to skip local environment checks of cmake.
```
~/Development/code/apache/nifi-minifi-cpp/build
$ cmake -DENABLE_AWS=ON -DENABLE_LIBRDKAFKA=ON -DENABLE_MQTT=ON -DENABLE_AZURE=ON -DENABLE_SQL=ON -DDOCKER_BUILD_ONLY=ON ..
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
Since only glibc and libstdc++ is dynamically linked to MiNiFi, the binary built with RHEL devtoolset should be distro-agnostic, and work on any relatively modern distro.

| Distro        | command     | Output File                                |
|---------------|:------------|:-------------------------------------------|
| centos 7      | make centos | nifi-minifi-cpp-centos-$VERSION.tar.gz     |
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

### Configuring
The 'conf' directory in the installation root contains a template config.yml document, minifi.properties, and minifi-log.properties. Please see our [Configuration document](CONFIGURE.md) for details on how to configure agents.

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
