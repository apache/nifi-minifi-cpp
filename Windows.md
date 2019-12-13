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

# Apache NiFi -  MiNiFi - C++ Windows Build Guide

## Requirements

Apache NiFi MiNiFi C++ has been built on Window Server 2016, 2019, and Windows 10 operating systems. The project is CMake focused we suggest building via
Visual Studio 2017 or our `win_build_vs.bat` script.

The project previously required OpenSSL to be installed. If you follow our build procedures, below, you will not need to install that dependency.

### Required software

 - Visual Studio 2017
 - [CMake](https://cmake.org/download/)
 - [Git](https://git-scm.com/download/win)
 - (Optional) [WiX Toolset](https://wixtoolset.org/releases/) (only for building the MSI)
 - (Optional) JDK (only for JNI support)

### JNI support
Though the project is written in C++, JNI functionality supports running Java processors stored in NiFi Archives. These can be run
in a much smaller memory footprint and consume fewer resources. If your systems do not support Java or you do not want a JDK installed, please use non-JNI builds.

## Building with Visual Studio 

In order to support Visual Studio you must install [plugins capable of building CMake](https://devblogs.microsoft.com/cppblog/cmake-support-in-visual-studio/). We also advise
installing WiX and Visual Studio Command Prompt via the marketplace. To do this please go to the Tools Menu, followed by Extensions and Updates. Once the popup displays you
may install additional features from Online sources in the Online menu.

A file named CMakeSettings.json provides the CMake configuration.

CMake must generate its cache, under Cache in the CMake Menu. After that is complete go to 'Build Only' under the CMake menu. Due to limitations in Visual Studio's CMake support, it is advised
that you build `minifi.lib` then `minifiexe` targets. Once you have built these targets, you may use the `cpack` command to build your MSI. If you are building with JNI functionality the MSI will be
significantly larger ( about 160 MB ) since it contains the base NARs to run the standard set of Apache NiFi processors. 

## Building via the build script

The preferred way of building the project is via the `win_build_vs.bat` script found in our root source folder. Its first parameter is mandatory, the directory in which it will build the project. `build` is a good default choice for this.

After the build directory it will take optional parameters modifying the CMake configuration used in the build:

| Argument | Effect |
|----------|------------------------------------------------|
| /T | Disables tests |
| /P | Enables MSI creation |
| /K | Enables Kafka extension |
| /J | Enables JNI |
| /64 | Creates 64-bit build instead of a 32-bit one |
| /D | Builds RelWithDebInfo build instead of Release |

Examples:
 - 32-bit build with kafka, disabling tests, enabling MSI creation: `win_build_vs.bat build32 /T /K /P`
 - 64-bit build with JNI, with debug symbols: `win_build_vs.bat build64 /64 /J /D`

`win_build_vs.bat` requires a Visual Studio 2017 build environment to be set up. For 32-bit builds this can be achieved by using the `x86 Native Tools Command Prompt for VS 2017`, for 64-bit builds by using the `x64 Native Tools Command Prompt for VS 2017`.

## Building directly with CMake

The project can also be built manually using CMake. It requires the same environment the build script does (the proper Native Tools Command Prompt).

A basic working CMake configuration can be inferred from the `win_build_vs.bat`.

`win_build_vs.bat /64 /P` is equivalent to running the following commands:

```
mkdir build
cd build
cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE_INIT=Release -DCMAKE_BUILD_TYPE=Release -DWIN32=WIN32 -DENABLE_LIBRDKAFKA=OFF -DENABLE_JNI=OFF -DOPENSSL_OFF=OFF -DENABLE_COAP=OFF -DUSE_SHARED_LIBS=OFF -DDISABLE_CONTROLLER=ON  -DBUILD_ROCKSDB=ON -DFORCE_WINDOWS=ON -DUSE_SYSTEM_UUID=OFF -DDISABLE_LIBARCHIVE=ON -DDISABLE_SCRIPTING=ON -DEXCLUDE_BOOST=ON -DENABLE_WEL=TRUE -DFAIL_ON_WARNINGS=OFF -DSKIP_TESTS=OFF ..
msbuild /m nifi-minifi-cpp.sln /property:Configuration=Release /property:Platform=x64
copy main\Release\minifi.exe main\
cpack
ctest -C Release
```