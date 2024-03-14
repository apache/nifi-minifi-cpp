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
set build_platform=x64
set enable_kafka=ON
set enable_coap=OFF
set enable_jni=OFF
set enable_sql=ON
set enable_aws=ON
set enable_sftp=OFF
set enable_azure=ON
set enable_bustache=OFF
set enable_encrypt_config=ON
set enable_lua_scripting=ON
set enable_mqtt=ON
set enable_opc=ON
set enable_pdh=OFF
set enable_splunk=ON
set enable_smb=ON
set enable_openwsman=OFF
set enable_ops=ON
set enable_pcap=OFF
set enable_python_scripting=ON
set enable_sensors=OFF
set enable_usb_camera=OFF
set enable_nanofi=OFF
set enable_opencv=OFF
set enable_prometheus=ON
set enable_gcp=ON
set enable_elastic=ON
set enable_grafana_loki=OFF
set test_custom_wel_provider=OFF
set generator="Visual Studio 17 2022"
set cpack=OFF
set installer_merge_modules=OFF
set strict_gsl_checks=
set redist=
set real_odbc=OFF
set sccache_arg=

set arg_counter=0
for %%x in (%*) do (
    set /A arg_counter+=1
    echo %%~x
    if [%%~x] EQU [/T]                set skiptests=ON
    if [%%~x] EQU [/R]                set skiptestrun=ON
    if [%%~x] EQU [/P]                set cpack=ON
    if [%%~x] EQU [/NO_KAFKA]         set enable_kafka=OFF
    if [%%~x] EQU [/J]                set enable_jni=ON
    if [%%~x] EQU [/NO_SQL]           set enable_sql=OFF
    if [%%~x] EQU [/C]                set enable_coap=ON
    if [%%~x] EQU [/NO_AWS]           set enable_aws=OFF
    if [%%~x] EQU [/SFTP]             set enable_sftp=ON
    if [%%~x] EQU [/PDH]              set enable_pdh=ON
    if [%%~x] EQU [/NO_SPLUNK]        set enable_splunk=OFF
    if [%%~x] EQU [/NO_SMB]           set enable_smb=OFF
    if [%%~x] EQU [/NO_GCP]           set enable_gcp=OFF
    if [%%~x] EQU [/NO_ELASTIC]       set enable_elastic=OFF
    if [%%~x] EQU [/M]                set installer_merge_modules=ON
    if [%%~x] EQU [/NO_AZURE]         set enable_azure=OFF
    if [%%~x] EQU [/N]                set enable_nanofi=ON
    if [%%~x] EQU [/O]                set enable_opencv=ON
    if [%%~x] EQU [/NO_PROMETHEUS]    set enable_prometheus=OFF
    if [%%~x] EQU [/BUSTACHE]         set enable_bustache=ON
    if [%%~x] EQU [/NO_ENCRYPT_CONFIG] set enable_encrypt_config=OFF
    if [%%~x] EQU [/NO_LUA_SCRIPTING] set enable_lua_scripting=OFF
    if [%%~x] EQU [/NO_MQTT]          set enable_mqtt=OFF
    if [%%~x] EQU [/NO_OPC]           set enable_opc=OFF
    if [%%~x] EQU [/OPENWSMAN]        set enable_openwsman=ON
    if [%%~x] EQU [/NO_OPS]           set enable_ops=OFF
    if [%%~x] EQU [/PCAP]             set enable_pcap=ON
    if [%%~x] EQU [/NO_PYTHON_SCRIPTING] set enable_python_scripting=OFF
    if [%%~x] EQU [/SENSORS]          set enable_sensors=ON
    if [%%~x] EQU [/USB_CAMERA]       set enable_usb_camera=ON
    if [%%~x] EQU [/LOKI]             set enable_grafana_loki=ON
    if [%%~x] EQU [/32]               set build_platform=Win32
    if [%%~x] EQU [/D]                set cmake_build_type=RelWithDebInfo
    if [%%~x] EQU [/DD]               set cmake_build_type=Debug
    if [%%~x] EQU [/CI]               set "strict_gsl_checks=-DSTRICT_GSL_CHECKS=AUDIT" & set test_custom_wel_provider=ON
    if [%%~x] EQU [/NONFREEUCRT]      set "redist=-DMSI_REDISTRIBUTE_UCRT_NONASL=ON"
    if [%%~x] EQU [/RO]               set real_odbc=ON
    if [%%~x] EQU [/NINJA]            set generator="Ninja"
    if [%%~x] EQU [/SCCACHE]          set "sccache_arg=-DCMAKE_C_COMPILER_LAUNCHER=sccache -DCMAKE_CXX_COMPILER_LAUNCHER=sccache"
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
cmake -G %generator% %build_platform_cmd% -DINSTALLER_MERGE_MODULES=%installer_merge_modules% -DTEST_CUSTOM_WEL_PROVIDER=%test_custom_wel_provider% -DENABLE_SQL=%enable_sql% -DMINIFI_USE_REAL_ODBC_TEST_DRIVER=%real_odbc% ^
        -DCMAKE_BUILD_TYPE_INIT=%cmake_build_type% -DCMAKE_BUILD_TYPE=%cmake_build_type% -DWIN32=WIN32 -DENABLE_LIBRDKAFKA=%enable_kafka% -DENABLE_JNI=%enable_jni% -DMINIFI_OPENSSL=ON ^
        -DENABLE_COAP=%enable_coap% -DENABLE_AWS=%enable_aws% -DENABLE_PDH=%enable_pdh% -DENABLE_AZURE=%enable_azure% -DENABLE_SFTP=%enable_sftp% -DENABLE_SPLUNK=%enable_splunk% -DENABLE_GCP=%enable_gcp% ^
        -DENABLE_NANOFI=%enable_nanofi% -DENABLE_OPENCV=%enable_opencv% -DENABLE_PROMETHEUS=%enable_prometheus% -DENABLE_ELASTICSEARCH=%enable_elastic% -DUSE_SHARED_LIBS=OFF -DENABLE_CONTROLLER=OFF  ^
        -DENABLE_BUSTACHE=%enable_bustache% -DENABLE_ENCRYPT_CONFIG=%enable_encrypt_config% -DENABLE_LUA_SCRIPTING=%enable_lua_scripting% -DENABLE_SMB=%enable_smb% ^
        -DENABLE_MQTT=%enable_mqtt% -DENABLE_OPC=%enable_opc% -DENABLE_OPENWSMAN=%enable_openwsman% -DENABLE_OPS=%enable_ops% -DENABLE_PCAP=%enable_pcap% ^
        -DENABLE_PYTHON_SCRIPTING=%enable_python_scripting% -DENABLE_SENSORS=%enable_sensors% -DENABLE_USB_CAMERA=%enable_usb_camera% -DENABLE_GRAFANA_LOKI=%enable_grafana_loki% ^
        -DBUILD_ROCKSDB=ON -DUSE_SYSTEM_UUID=OFF -DENABLE_LIBARCHIVE=ON -DENABLE_WEL=ON -DMINIFI_FAIL_ON_WARNINGS=OFF -DSKIP_TESTS=%skiptests% ^
        %strict_gsl_checks% %redist% %sccache_arg% %EXTRA_CMAKE_ARGUMENTS% "%scriptdir%" && %buildcmd%
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
