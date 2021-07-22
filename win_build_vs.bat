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
set skiptestrun=OFF
set cmake_build_type=Release
set build_platform=Win32
set build_kafka=OFF
set build_coap=OFF
set build_jni=OFF
set build_SQL=OFF
set build_AWS=OFF
set build_SFTP=OFF
set build_azure=OFF
set test_custom_wel_provider=OFF
set generator="Visual Studio 16 2019"
set cpack=OFF
set installer_merge_modules=OFF
set strict_gsl_checks=
set redist=
set build_linter=OFF
set build_nanofi=OFF
set build_opencv=OFF
set real_odbc=OFF

set arg_counter=0
for %%x in (%*) do (
    set /A arg_counter+=1
    echo %%~x
    if [%%~x] EQU [/T]           set skiptests=ON
    if [%%~x] EQU [/R]           set skiptestrun=ON
    if [%%~x] EQU [/P]           set cpack=ON
    if [%%~x] EQU [/K]           set build_kafka=ON
    if [%%~x] EQU [/J]           set build_JNI=ON
    if [%%~x] EQU [/S]           set build_SQL=ON
    if [%%~x] EQU [/C]           set build_coap=ON
    if [%%~x] EQU [/A]           set build_AWS=ON
    if [%%~x] EQU [/SFTP]        set build_SFTP=ON
    if [%%~x] EQU [/PDH]         set build_PDH=ON
    if [%%~x] EQU [/M]           set installer_merge_modules=ON
    if [%%~x] EQU [/Z]           set build_azure=ON
    if [%%~x] EQU [/N]           set build_nanofi=ON
    if [%%~x] EQU [/O]           set build_opencv=ON
    if [%%~x] EQU [/64]          set build_platform=x64
    if [%%~x] EQU [/D]           set cmake_build_type=RelWithDebInfo
    if [%%~x] EQU [/DD]          set cmake_build_type=Debug
    if [%%~x] EQU [/CI]          set "strict_gsl_checks=-DSTRICT_GSL_CHECKS=AUDIT" & set test_custom_wel_provider=ON
    if [%%~x] EQU [/NONFREEUCRT] set "redist=-DMSI_REDISTRIBUTE_UCRT_NONASL=ON"
    if [%%~x] EQU [/L]           set build_linter=ON
    if [%%~x] EQU [/RO]          set real_odbc=ON
)

mkdir %builddir%
pushd %builddir%\

cmake -G %generator% -A %build_platform% -DINSTALLER_MERGE_MODULES=%installer_merge_modules% -DTEST_CUSTOM_WEL_PROVIDER=%test_custom_wel_provider% -DENABLE_SQL=%build_SQL% -DUSE_REAL_ODBC_TEST_DRIVER=%real_odbc% -DCMAKE_BUILD_TYPE_INIT=%cmake_build_type% -DCMAKE_BUILD_TYPE=%cmake_build_type% -DWIN32=WIN32 -DENABLE_LIBRDKAFKA=%build_kafka% -DENABLE_JNI=%build_jni% -DOPENSSL_OFF=OFF -DENABLE_COAP=%build_coap% -DENABLE_AWS=%build_AWS% -DENABLE_PDH=%build_PDH% -DENABLE_AZURE=%build_azure% -DENABLE_SFTP=%build_SFTP% -DENABLE_NANOFI=%build_nanofi% -DENABLE_OPENCV=%build_opencv% -DUSE_SHARED_LIBS=OFF -DDISABLE_CONTROLLER=ON  -DBUILD_ROCKSDB=ON -DFORCE_WINDOWS=ON -DUSE_SYSTEM_UUID=OFF -DDISABLE_LIBARCHIVE=OFF -DDISABLE_SCRIPTING=ON -DEXCLUDE_BOOST=ON -DENABLE_WEL=ON -DFAIL_ON_WARNINGS=OFF -DSKIP_TESTS=%skiptests% %strict_gsl_checks% %redist% -DENABLE_LINTER=%build_linter% .. && msbuild /m nifi-minifi-cpp.sln /property:Configuration=%cmake_build_type% /property:Platform=%build_platform% && copy bin\%cmake_build_type%\minifi.exe main\
IF %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%
if [%cpack%] EQU [ON] (
    cpack -C %cmake_build_type%
    IF !ERRORLEVEL! NEQ 0 ( popd & exit /b !ERRORLEVEL! )
)
if [%skiptests%] NEQ [ON] (
    if [%skiptestrun%] NEQ [ON] (
        ctest --timeout 300 --parallel 8 -C %cmake_build_type% --output-on-failure
        IF !ERRORLEVEL! NEQ 0 ( popd & exit /b !ERRORLEVEL! )
    )
)
popd
goto :eof

:usage
@echo "Usage: %0 <build_dir> options"
exit /B 1
