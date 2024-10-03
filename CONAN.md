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

# Apache NiFi - MiNiFi - C++ Conan Build Guide

We will walk through the steps to build MiNiFi using conan version 2 that comes with CMake integration. We will also go through the process of creating a MiNiFi conan package for easier integration into other C++ projects. We will start with a discussion on building MiNiFi using the standalone CMake approach and the conan approach. After we have built MiNiFi and created a MiNiFi conan package, we will conclude by elaborating on some of the benefits that can come with integrating a MiNiFi conan package into new or existing C++ infrastructure with respect to real-time robotic systems.

Conan build support is experimental, and not yet suitable for general use.

## Table of Contents

- [Apache NiFi - MiNiFi - C++ Conan Build Guide](#apache-nifi---minifi---c---conan-build-guide)
  - [Table of Contents](#table-of-contents)
  - [Description](#description)
  - [Build MiNiFi C++ with Conan](#build-minifi-c---with-conan)
  - [Create MiNiFi C++ Conan Package](#create-minifi-c---conan-package)
  - [Conclusion](#conclusion)
  - [Appendix](#appendix)

## Description

Apache NiFi MiNiFi C++ is normally built using standalone CMake. Similarly, we can also create a MiNiFi C++ package using make package. However, each time we build MiNiFi from
source, it uses CMake to install the third party external libraries that are enabled by downloading the source code and then building each library until it is available to use toward building MiNiFi. If a change is made toward MiNiFi's CMake code in regards to one or more of the external libraries, then potentially the long process of installing one or more of the externl library(ies) happens again before the MiNiFi source code is built. Another way we can build MiNiFi and create MiNiFi packages without running into having to rebuild its external libraries is using conan. Conan provides some great features toward managing to install prebuilt conan packages, which for our case is the external libraries that are needed to then build MiNiFi. We will walk through the steps to build the MiNiFi project and create a MiNiFi conan package.

## Build MiNiFi C++ with Conan

To build MiNiFi using conan, first we install conan version 2, then we create a **default** conan profile that will later be ignored for our custom conan profile, create a MINIFI_HOME environment variable, then we install prebuilt conan packages representing the MiNiFi external libraries and finally we compile MiNiFi.

~~~bash
sudo pip install --force-reinstall -v "conan==2.0.17"

# create a "default" conan profile, so conan has it on record in ~/.conan2/, before using your own custom profile.
conan profile detect

# conanfile.py is in root dir of MiNiFi C++ project
cd $HOME/nifi-minifi-cpp

# create MINIFI_HOME env variable for binary executable minifi
export MINIFI_HOME=$(pwd)

# install conan packages for MiNiFi C++ using conanfile.py invoking Conan
# since we created default profile earlier, we can override it with our own minifi profile
conan install . --build=missing --output-folder=build_conan -pr=etc/conan/profiles/release-linux

# build MiNiFi C++ using conanfile.py invoking Conan & CMake
conan build . --output-folder=build_conan -pr=etc/conan/profiles/release-linux
~~~

- **NOTE**: After building MiNiFi, we must have the MINIFI_HOME environment variable created in order to successfully run the minifi binary executable.

- **NOTE**: When we install the prebuilt conan package representing the MiNiFi third party libraries, we add the `--build=missing` in case some of the prebuilt missing conan packages are not found from conancenter, jfrog, or one of our conan repositories, then conan will build the conan packages from source. We can upload new prebuilt conan packages by **[conan upload](https://docs.conan.io/2/reference/commands/upload.html)**.

- **NOTE**: When we tell conan to build MiNiFi, conan first installs prebuilt conan packages and then it compiles MiNiFi from source, so we have the MiNiFi library and its binary executable available.

### Run MiNiFi CTESTs from Build Folder

~~~bash
pushd build_conan/
ctest
popd # build_conan/
~~~

### Run MiNiFi Executable

~~~bash
# verify we can run minifi binary executable
./build_conan/bin/minifi
~~~

## Create MiNiFi C++ Conan Package

To create a MiNiFi package, we will follow the similar steps we took to build MiNiFi. Then right after we install the prebuilt conan packages representing MiNiFi's external libraries, we create that MiNiFi conan package.

~~~bash
# make sure to install conan2 for your environment
sudo pip install --force-reinstall -v "conan==2.0.17"

# create a "default" conan profile, so conan has it on record, before using your own custom profile. Gets created in ~/.conan2/
conan profile detect

# conanfile.py is in root dir of MiNiFi C++ project
cd $HOME/nifi-minifi-cpp

# create MINIFI_HOME env variable for binary executable minifi
export MINIFI_HOME=$(pwd)

# install conan packages for MiNiFi C++ using conanfile.py invoking Conan
# since we created default profile earlier, we can override it with our own minifi profile
# make sure path is correct
conan install . --build=missing --output-folder=build_conan -pr=etc/conan/profiles/release-linux

# create MiNiFi C++ conan package using conanfile.py invoking Conan & CMake
conan create . --user=minifi --channel=develop -pr=etc/conan/profiles/release-linux
~~~

- **NOTE**: When we tell conan to create the MiNiFi conan package, conan first installs prebuilt conan packages, then it compiles MiNiFi from source inside `~/.conan2/p/b/minif<UUID>/b`, and then it copies over MiNiFi's libraries and its binary executables into the conan package folder `~/.conan2/p/b/minif<UUID>/p`. Once we have the MiNiFi conan package, we can integrate it into other C++ infrastructure using CMake.

### Run MiNiFi CTESTs from Its Conan Package

~~~bash
# Example of minif<UUID> conan package folder name: minifbd17f6a02da35
pushd ~/.conan2/p/b/minif<UUID>/b
ctest
popd # build_conan/
~~~

## Conclusion

To have a more consistent quick build process for MiNiFi, we can use conan version 2. When we use conan with MiNiFi's build process, we can install prebuilt conan packages that can represent MiNiFi's external libraries and then build MiNiFi quickly. We also have a bit more control over when we want to make updates to MiNiFi's external libraries that impact their build and then re-build them as prebuilt conan packages. So these external library conan packages can be used toward building MiNiFi again without having to rebuild those external libraries while rebuilding MiNiFi. Therefore, we can focus mainly on configuring the way we want MiNiFi to build without having to worry about MiNiFi's external libraries needing to be rebuilt again. Similarly, if we create a MiNiFi conan package, it will install the prebuilt conan packages, build MiNiFi from source and then create the MiNiFi package.

There are multiple benefits of having MiNiFi prebuilt conan packages. We can upload these MiNiFi conan packages to a conan repository like jfrog for version management. We can easily integrate MiNiFi's edge data pipeline features into other C++ software infrastructure using conan's CMake support. We can still use MiNiFi for edge data collection from the IoT devices embedded on robotic systems. We can integrate MiNiFi into self-driving cars (sensor examples: cameras, lidar, radar, inertial measurement unit (IMU), electronic speed controller (ESC), steering servo, etc), into medical imaging robots (sensor examples: depth cameras, ultrasound, gamma detector, force/torque sensor, joint position sensor, etc) or some other real-time robotic system.

By leveraging MiNiFi as a conan package, we can leverage MiNiFi that comes with the best practices of building data pipelines from NiFi and bring them into existing C++ real-time robotics infrastructure. Some teams across companies typically have their own custom edge data pipelines that process data for the different events to eventually perform actions on that data. As an alternative to all these companies and their teams having their own custom edge data pipeline libraries, MiNiFi C++, which is like a headless NiFi, can provide a more consistent standard approach for team's to build edge pipelines. Through all stages of the edge data pipelines, MiNiFi can still provide telemetry to NiFi instances running in the cloud.

## Appendix

### Create Custom RocksDB Conan Package

~~~bash
pushd nifi-minifi-cpp/thirdparty/rocksdb/all

conan create . --user=minifi --channel=develop --version=8.10.2 -pr=../../../etc/conan/profiles/release-linux

popd
~~~
