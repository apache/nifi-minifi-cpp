@echo off 
SETLOCAL enabledelayedexpansion
rem Licensed to the Apache Software Foundation (ASF) under one or more
rem contributor license agreements.  See the NOTICE file distributed with
rem this work for additional information regarding copyright ownership.
rem The ASF licenses this file to You under the Apache License, Version 2.0
rem (the "License"); you may not use this file except in compliance with
rem the License.  You may obtain a copy of the License at
rem
rem     http://www.apache.org/licenses/LICENSE-2.0
rem
rem Unless required by applicable law or agreed to in writing, software
rem distributed under the License is distributed on an "AS IS" BASIS,
rem WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
rem See the License for the specific language governing permissions and
rem limitations under the License.

set builddir=%1
set builddir=%builddir:"=%

TITLE Apache NiFi MiNiFi C++ Windows Build Helper

mkdir %builddir%

cd %builddir%\

rem -DENABLE_COAP=ON
cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE_INIT=Release -DCMAKE_BUILD_TYPE=Release -DWIN32=WIN32 -DOPENSSL_OFF=OFF  -DUSE_SHARED_LIBS=OFF -DDISABLE_CONTROLLER=ON  -DBUILD_ROCKSDB=ON -DFORCE_WINDOWS=ON -DUSE_SYSTEM_UUID=OFF -DDISABLE_LIBARCHIVE=ON -DDISABLE_SCRIPTING=ON -DEXCLUDE_BOOST=ON -DENABLE_WEL=TRUE -DSKIP_TESTS=ON -DFAIL_ON_WARNINGS=OFF .. && msbuild /m nifi-minifi-cpp.sln /property:Configuration=Release && copy main\release\minifi.exe main\  && cpack

cd ..
