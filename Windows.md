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

Apache NiFi MiNiFi C++ has been built on Window Server 2016, 2019, and Windows 10 operating systems. Since the project is CMAKE focused we suggest building via
Visual Studio 2017 or our ms_build.bat script. In order to build the MSI, please install the [WiX Toolset](https://wixtoolset.org/)

The project previously required OpenSSL to be installed. If you follow our build procedures, below, you will not need to install that dependency. Further, any MSI
distributable requires that systems install the [Visual Studio 2010 redistributables](https://www.microsoft.com/en-us/download/details.aspx?id=26999&irgwc=1&OCID=AID681541_aff_7593_312327&tduid=(ir__2dz2dkkuookfr3a22jsv1zgwwn2xmb9vrhvzwmdq00)

## Building with Visual Studio 

In order to support Visual Studio you must install [plugins capable of building CMAKE](https://devblogs.microsoft.com/cppblog/cmake-support-in-visual-studio/). We also advise
installing WiX and Visual Studio Command Prompt via the marketplace. To do this please go to the Tools Menu, followed by Extensions and Updates. Once the popup displays you
may install additional features from Online sources in the Online menui.

Once you have CMAKE support available please select the appropriate target. A file named CMakeSettings.json provides a few base configurations, x64-RelWithDebInfo, x64-Release,
x64-RelWithDebInfo-WithJNI, and x64-Release-WithJNI. Though the project is written in C++, JNI functionality supports running Java processors stored in NiFi Archives. These can be run
in a much smaller memory footprint and consumer fewer resources. If your systems do not support Java or you do not want a JDK installed, please select the non JNI targets.

CMAKE must generate its cache, under Cache in the CMake Menu. After that is complete go to 'Build Only' under the CMAKE menu. Due to limitations in Visual Studio's CMAKE support, it is advised
that you build minifi.lib then minifiexe targets. Once you have built these targets, you may use the cpack command to build your MSI. If you are building with JNI functionality the MSI will be
significantly larger ( about 160 MB ) since it contains the base NARs to run the standard set of Apache NiFi processors. 

## Building via the build script

The preferred way of building the project is via the win_build_vs.bat script found in our root source folder. You must supply a single command, the build directory. Typically you create a directory
with CMAKE and build in that referencing your CMAKE tree. Simply supply win_build_vs.bat an argument like 'build' as the name of your build directory and it will create this directory building the 
project within it. If WiX is installed cpack will create your MSI in the chosen build directory.

