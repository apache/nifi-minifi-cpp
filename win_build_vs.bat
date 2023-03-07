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
set scriptdir=%~dp0
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
set enable_bustache=OFF
set enable_coap=OFF
set enable_encrypt_config=OFF
set enable_gps=OFF
set enable_lua_scripting=OFF
set enable_mqtt=OFF
set enable_opc=OFF
set enable_openwsman=OFF
set enable_ops=OFF
set enable_pcap=OFF
set enable_python_scripting=OFF
set enable_sensors=OFF
set enable_tensorflow=OFF
set enable_usb_camera=OFF
set test_custom_wel_provider=OFF
set generator="Visual Studio 16 2019"
set cpack=OFF
set installer_merge_modules=OFF
set strict_gsl_checks=
set redist=
set build_nanofi=OFF
set build_opencv=OFF
set build_prometheus=OFF
set real_odbc=OFF

set arg_counter=0
for %%x in (%*) do (
    set /A arg_counter+=1
    echo %%~x
    if [%%~x] EQU [/T]                set skiptests=ON
    if [%%~x] EQU [/R]                set skiptestrun=ON
    if [%%~x] EQU [/P]                set cpack=ON
    if [%%~x] EQU [/K]                set build_kafka=ON
    if [%%~x] EQU [/J]                set build_JNI=ON
    if [%%~x] EQU [/S]                set build_SQL=ON
    if [%%~x] EQU [/C]                set build_coap=ON
    if [%%~x] EQU [/A]                set build_AWS=ON
    if [%%~x] EQU [/SFTP]             set build_SFTP=ON
    if [%%~x] EQU [/PDH]              set build_PDH=ON
    if [%%~x] EQU [/SPLUNK]           set build_SPLUNK=ON
    if [%%~x] EQU [/GCP]              set build_GCP=ON
    if [%%~x] EQU [/ELASTIC]          set build_ELASTIC=ON
    if [%%~x] EQU [/M]                set installer_merge_modules=ON
    if [%%~x] EQU [/Z]                set build_azure=ON
    if [%%~x] EQU [/N]                set build_nanofi=ON
    if [%%~x] EQU [/O]                set build_opencv=ON
    if [%%~x] EQU [/PR]               set build_prometheus=ON
    if [%%~x] EQU [/BUSTACHE]         set enable_bustache=ON
    if [%%~x] EQU [/COAP]             set enable_coap=ON
    if [%%~x] EQU [/ENCRYPT_CONFIG]   set enable_encrypt_config=ON
    if [%%~x] EQU [/GPS]              set enable_gps=ON
    if [%%~x] EQU [/LUA_SCRIPTING]    set enable_lua_scripting=ON
    if [%%~x] EQU [/MQTT]             set enable_mqtt=ON
    if [%%~x] EQU [/OPC]              set enable_opc=ON
    if [%%~x] EQU [/OPENWSMAN]        set enable_openwsman=ON
    if [%%~x] EQU [/OPS]              set enable_ops=ON
    if [%%~x] EQU [/PCAP]             set enable_pcap=ON
    if [%%~x] EQU [/PYTHON_SCRIPTING] set enable_python_scripting=ON
    if [%%~x] EQU [/SENSORS]          set enable_sensors=ON
    if [%%~x] EQU [/TENSORFLOW]       set enable_tensorflow=ON
    if [%%~x] EQU [/USB_CAMERA]       set enable_usb_camera=ON
    if [%%~x] EQU [/64]               set build_platform=x64
    if [%%~x] EQU [/D]                set cmake_build_type=RelWithDebInfo
    if [%%~x] EQU [/DD]               set cmake_build_type=Debug
    if [%%~x] EQU [/CI]               set "strict_gsl_checks=-DSTRICT_GSL_CHECKS=AUDIT" & set test_custom_wel_provider=ON
    if [%%~x] EQU [/NONFREEUCRT]      set "redist=-DMSI_REDISTRIBUTE_UCRT_NONASL=ON"
    if [%%~x] EQU [/RO]               set real_odbc=ON
    if [%%~x] EQU [/NINJA]            set generator="Ninja"
)

mkdir %builddir%
pushd %builddir%\

if [%generator%] EQU ["Ninja"] (
    set "buildcmd=ninja && copy bin\minifi.exe minifi_main\"
    set "build_platform_cmd="
) else (
    set "buildcmd=msbuild /m nifi-minifi-cpp.sln /property:Configuration=%cmake_build_type% /property:Platform=%build_platform% && copy bin\%cmake_build_type%\minifi.exe minifi_main\"
    set "build_platform_cmd=-A %build_platform%"
)
echo on
cmake -G %generator% %build_platform_cmd% -DINSTALLER_MERGE_MODULES=%installer_merge_modules% -DTEST_CUSTOM_WEL_PROVIDER=%test_custom_wel_provider% -DENABLE_SQL=%build_SQL% -DUSE_REAL_ODBC_TEST_DRIVER=%real_odbc% ^
        -DCMAKE_BUILD_TYPE_INIT=%cmake_build_type% -DCMAKE_BUILD_TYPE=%cmake_build_type% -DWIN32=WIN32 -DENABLE_LIBRDKAFKA=%build_kafka% -DENABLE_JNI=%build_jni% -DOPENSSL_OFF=OFF ^
        -DENABLE_COAP=%build_coap% -DENABLE_AWS=%build_AWS% -DENABLE_PDH=%build_PDH% -DENABLE_AZURE=%build_azure% -DENABLE_SFTP=%build_SFTP% -DENABLE_SPLUNK=%build_SPLUNK% -DENABLE_GCP=%build_GCP% ^
        -DENABLE_NANOFI=%build_nanofi% -DENABLE_OPENCV=%build_opencv% -DENABLE_PROMETHEUS=%build_prometheus% -DENABLE_ELASTICSEARCH=%build_ELASTIC% -DUSE_SHARED_LIBS=OFF -DDISABLE_CONTROLLER=ON  ^
        -DENABLE_BUSTACHE=%enable_bustache% -DENABLE_COAP=%enable_coap% -DENABLE_ENCRYPT_CONFIG=%enable_encrypt_config% -DENABLE_GPS=%enable_gps% -DENABLE_LUA_SCRIPTING=%enable_lua_scripting% ^
        -DENABLE_MQTT=%enable_mqtt% -DENABLE_OPC=%enable_opc% -DENABLE_OPENWSMAN=%enable_openwsman% -DENABLE_OPS=%enable_ops% -DENABLE_PCAP=%enable_pcap% ^
        -DENABLE_PYTHON_SCRIPTING=%enable_python_scripting% -DENABLE_SENSORS=%enable_sensors% -DENABLE_TENSORFLOW=%enable_tensorflow% -DENABLE_USB_CAMERA=%enable_usb_camera% ^
        -DBUILD_ROCKSDB=ON -DFORCE_WINDOWS=ON -DUSE_SYSTEM_UUID=OFF -DDISABLE_LIBARCHIVE=OFF -DENABLE_WEL=ON -DFAIL_ON_WARNINGS=OFF -DSKIP_TESTS=%skiptests% ^
        %strict_gsl_checks% %redist% %EXTRA_CMAKE_ARGUMENTS% "%scriptdir%" && %buildcmd%
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
