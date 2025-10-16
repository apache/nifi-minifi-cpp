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

## Python based bootstrapping (recommended)
Prerequisites:
- [python](https://docs.python.org/)
- [venv](https://docs.python.org/3/library/venv.html)
- [chocolatey](https://chocolatey.org/)
```dos
.\bootstrap\py_bootstrap.bat
```

This will set up a virtual environment in the bootstrap folder, and guide you through the build process.
This will also download and install all dependencies required for the selected components.
It will also create a batch file (.\bootstrap\build_environment.bat),
which sets the necessary environment variables for the build, so it can be built without bootstrapping everytime.


## Alternative: Building via build script (advanced)

Apache NiFi MiNiFi C++ has been built on Window Server 2016, 2019, and Windows 10 operating systems. The project is CMake focused we suggest building via Visual Studio 2022 or our `win_build_vs.bat` script.

The project previously required OpenSSL to be installed. If you follow our build procedures, below, you will not need to install that dependency.

#### Required software

 - Visual Studio 2022
 - [CMake](https://cmake.org/download/)
 - [Git](https://git-scm.com/download/win) (the build process requires the bash.exe and patch.exe tools packaged with Git)
 - [Perl](https://strawberryperl.com/)
 - [NASM](https://nasm.us)
 - (Optional) [WiX Toolset](https://wixtoolset.org/releases/) (only for building the MSI)

### Building with Visual Studio

Make sure your Visual Studio installation includes the "Visual C++ tools for CMake" and "Visual C++ ATL for x86 and x64" options.
You can also add these after installation using the Visual Studio Installer app. We also advise
installing WiX and Visual Studio Command Prompt via the marketplace. To do this please go to the Tools Menu, followed by Extensions and Updates. Once the popup displays you
may install additional features from Online sources in the Online menu.

A file named CMakeSettings.json provides the CMake configuration.

CMake must generate its cache, under Cache in the CMake Menu. After that is complete go to 'Build Only' under the CMake menu. Due to limitations in Visual Studio's CMake support, it is advised
that you build `minifi.lib` then `minifi.exe` targets.  `Build All` works, too, but it takes much longer.
Once you have built these targets, you may use the `cpack` command to build your MSI.

### Building via the build script

The preferred way of building the project is via the `win_build_vs.bat` script found in our root source folder. Its first parameter is mandatory, the directory in which it will build the project. `build` is a good default choice for this.

After the build directory it will take optional parameters modifying the CMake configuration used in the build:

| Argument             | Effect                                                                              |
|----------------------|-------------------------------------------------------------------------------------|
| /T                   | Disables building tests                                                             |
| /R                   | Disables automatic test running after build                                         |
| /P                   | Enables MSI creation                                                                |
| /NO_KAFKA            | Disables Kafka extension                                                            |
| /NO_SQL              | Disables SQL extension                                                              |
| /NO_AWS              | Disables AWS extension                                                              |
| /SFTP                | Enables SFTP extension                                                              |
| /PDH                 | Enables Performance Monitor extension                                               |
| /NO_SPLUNK           | Disables Splunk extension                                                           |
| /NO_GCP              | Disables Google cloud storage extension                                             |
| /NO_ELASTIC          | Disables Elastic extension                                                          |
| /NO_AZURE            | Disables Azure extension                                                            |
| /NO_MQTT             | Disables MQTT extension                                                             |
| /NO_LUA_SCRIPTING    | Disables Lua scripting extension                                                    |
| /NO_PYTHON_SCRIPTING | Disables Python scripting extension                                                 |
| /O                   | Enables OpenCV                                                                      |
| /NO_PROMETHEUS       | Disables Prometheus                                                                 |
| /RO                  | Use real ODBC driver in tests instead of mock SQL driver                            |
| /M                   | Creates installer with merge modules                                                |
| /32                  | Creates 32-bit build instead of a 64-bit one                                        |
| /D                   | Builds RelWithDebInfo build instead of Release                                      |
| /DD                  | Builds Debug build instead of Release                                               |
| /CI                  | Sets STRICT_GSL_CHECKS to AUDIT                                                     |
| /RO                  | Use SQLite ODBC driver in SQL extenstion unit tests instead of a mock database      |
| /NINJA               | Uses Ninja build system instead of MSBuild                                          |
| /NO_ENCRYPT_CONFIG   | Disables build of encrypt-config binary                                             |
| /SCCACHE             | Uses sccache build caching                                                          |
| /BUSTACHE            | Enables Bustache templating support                                                 |
| /NO_OPC              | Disables OPC extension                                                              |
| /NO_OPS              | Disables OPS extension                                                              |
| /LOKI                | Enables Grafana Loki extension                                                      |
| /NONFREEUCRT         | Enables inclusion of non-free UCRT libraries in the installer (not redistributable) |

Examples:
 - 32-bit build with kafka, disabling tests, enabling MSI creation: `win_build_vs.bat build32 /T /K /P`
 - 64-bit build with PDH, with debug symbols: `win_build_vs.bat build64 /64 /D /PDH`

`win_build_vs.bat` requires a Visual Studio 2022 build environment to be set up. Use the `x86 Native Tools Command Prompt for VS 2022`, or the `x64 Native Tools Command Prompt for VS 2022` for 32-bit and 64-bit builds respectively.

You can specify additional CMake arguments by setting the EXTRA_CMAKE_ARGUMENTS variable:
```
> set EXTRA_CMAKE_ARGUMENTS=-DCOMPANY_NAME="Acme Inc" -DPRODUCT_NAME="Roadrunner Stopper"
> win_build_vs.bat ...
```

### Alternative building: Manual bootstrapping (advanced)

The project can also be built manually using CMake. It requires the same environment the build script does (the proper Native Tools Command Prompt).

A basic working CMake configuration can be inferred from the `win_build_vs.bat`.

`win_build_vs.bat /64 /P` is equivalent to running the following commands:

```
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 -DMINIFI_INCLUDE_VC_REDIST_MERGE_MODULES=OFF -DTEST_CUSTOM_WEL_PROVIDER=OFF -DENABLE_SQL=OFF -DMINIFI_USE_REAL_ODBC_TEST_DRIVER=OFF -DCMAKE_BUILD_TYPE_INIT=Release -DCMAKE_BUILD_TYPE=Release -DWIN32=WIN32 -DENABLE_KAFKA=OFF -DENABLE_AWS=OFF -DENABLE_PDH= -DENABLE_AZURE=OFF -DENABLE_SFTP=OFF -DENABLE_SPLUNK= -DENABLE_GCP= -DENABLE_OPENCV=OFF -DENABLE_PROMETHEUS=OFF -DENABLE_ELASTICSEARCH= -DUSE_SHARED_LIBS=OFF -DENABLE_CONTROLLER=ON -DENABLE_BUSTACHE=OFF -DENABLE_ENCRYPT_CONFIG=OFF -DENABLE_LUA_SCRIPTING=OFF -DENABLE_MQTT=OFF -DENABLE_OPC=OFF -DENABLE_OPS=OFF -DENABLE_PYTHON_SCRIPTING= -DBUILD_ROCKSDB=ON -DUSE_SYSTEM_UUID=OFF -DENABLE_LIBARCHIVE=ON -DENABLE_WEL=ON -DENABLE_COUCHBASE=ON -DMINIFI_FAIL_ON_WARNINGS=OFF -DSKIP_TESTS=OFF ..
msbuild /m nifi-minifi-cpp.sln /property:Configuration=Release /property:Platform=x64
copy minifi_main\Release\minifi.exe minifi_main\
cpack
ctest -C Release
```

## Using MiNiFi C++ as a Windows service

Building and packaging MiNiFi C++ results in an MSI installer. This installer can be used to install MiNiFi C++ as a Windows service. Depending on the build options, specific extensions can be selected for installation.

The installer also provides an option to specify a service account to be used for the Windows service. By default, the `LocalSystem` account is used. If you want to specify a different account, make sure to provide the account name in the `DOMAIN\username` format.

**NOTE:** To start the Windows service using the specified account, the account must have the `Log on as a service` right. If this right is missing, the following error event will appear in the system logs:

```
The Apache NiFi MiNiFi service was unable to log on as .\username with the currently configured password due to the following error:
Logon failure: the user has not been granted the requested logon type at this computer.
```

This right can be granted using the Local Security Policy tool (`secpol.msc`). Navigate to `Local Policies` -> `User Rights Assignment` -> `Log on as a service` and add the account.
