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


## Alternative: Building manually (advanced)

Apache NiFi MiNiFi C++ has been built on Window Server 2016, 2019, and Windows 10 operating systems. The project is CMake focused we suggest building via Visual Studio 2022.

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


### Alternative building: Manual bootstrapping (advanced)

The project can also be built manually using CMake. It requires a Visual Studio 2022 build environment to be set up. Use the `x64 Native Tools Command Prompt for VS 2022`.

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
