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

TITLE Apache NiFi MiNiFi C++ Windows Build Helper

if [%1]==[] goto usage

set builddir=%1
set skiptests=OFF
set cmake_build_type=Release
set build_platform=Win32
set build_kafka=OFF
set build_coap=OFF
set build_jni=OFF
set build_SQL=OFF
set generator="Visual Studio 15 2017"
set cpack=OFF
set installer_merge_modules=OFF
set strict_gsl_checks=

set arg_counter=0
for %%x in (%*) do (
    set /A arg_counter+=1
    echo %%~x
    if [%%~x] EQU [/T] (
        set skiptests=ON
    )
    if [%%~x] EQU [/P] (
        set cpack=ON
    )
    if [%%~x] EQU [/K] (
        set build_kafka=ON
    )
    if [%%~x] EQU [/J] (
        set build_JNI=ON
    )
    if [%%~x] EQU [/S] (
        set build_SQL=ON
    )
    if [%%~x] EQU [/M] (
        set installer_merge_modules=ON
    )
    if [%%~x] EQU [/C] (
        set build_coap=ON
    )
    if [%%~x] EQU [/64] (
        set build_platform=x64
        set generator="Visual Studio 15 2017 Win64"
    )
    if [%%~x] EQU [/D] (
        set cmake_build_type=RelWithDebInfo
    )
    if [%%~x] EQU [/CI] (
        set "strict_gsl_checks=-DSTRICT_GSL_CHECKS=AUDIT"
    )
)

mkdir %builddir%
pushd %builddir%\

cmake -G %generator% -DINSTALLER_MERGE_MODULES=%installer_merge_modules% -DENABLE_SQL=%build_SQL% -DCMAKE_BUILD_TYPE_INIT=%cmake_build_type% -DCMAKE_BUILD_TYPE=%cmake_build_type% -DWIN32=WIN32 -DENABLE_LIBRDKAFKA=%build_kafka% -DENABLE_JNI=%build_jni% -DOPENSSL_OFF=OFF -DENABLE_COAP=%build_coap% -DUSE_SHARED_LIBS=OFF -DDISABLE_CONTROLLER=ON  -DBUILD_ROCKSDB=ON -DFORCE_WINDOWS=ON -DUSE_SYSTEM_UUID=OFF -DDISABLE_LIBARCHIVE=OFF -DDISABLE_SCRIPTING=ON -DEXCLUDE_BOOST=ON -DENABLE_WEL=TRUE -DFAIL_ON_WARNINGS=OFF -DSKIP_TESTS=%skiptests% %strict_gsl_checks% .. && msbuild /m nifi-minifi-cpp.sln /property:Configuration=%cmake_build_type% /property:Platform=%build_platform% && copy main\%cmake_build_type%\minifi.exe main\
IF %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%
if [%cpack%] EQU [ON] (
    cpack
    IF !ERRORLEVEL! NEQ 0 ( popd & exit /b !ERRORLEVEL! )
)
if [%skiptests%] NEQ [ON] (
    ctest --timeout 300 --parallel 8 -C %cmake_build_type% --output-on-failure
    IF !ERRORLEVEL! NEQ 0 ( popd & exit /b !ERRORLEVEL! )
)
popd
goto :eof

:usage
@echo "Usage: %0 <build_dir> options"
exit /B 1
