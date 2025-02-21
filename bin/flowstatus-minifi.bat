@echo off &setlocal enabledelayedexpansion
rem
rem    Licensed to the Apache Software Foundation (ASF) under one or more
rem    contributor license agreements.  See the NOTICE file distributed with
rem    this work for additional information regarding copyright ownership.
rem    The ASF licenses this file to You under the Apache License, Version 2.0
rem    (the "License"); you may not use this file except in compliance with
rem    the License.  You may obtain a copy of the License at
rem
rem       http://www.apache.org/licenses/LICENSE-2.0
rem
rem    Unless required by applicable law or agreed to in writing, software
rem    distributed under the License is distributed on an "AS IS" BASIS,
rem    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
rem    See the License for the specific language governing permissions and
rem    limitations under the License.
rem

set "bin_dir=%~dp0"

if not exist "%bin_dir%\minificontroller.exe" (
    echo MiNiFi Controller is not installed
    exit /b
)

if "%~1"=="" (
    echo MiNiFi flowStatus operation requires a flow status query parameter like "processor:TailFile:health,stats,bulletins"
    goto usage
)

if "%~2"=="" (
    "%bin_dir%\minificontroller.exe" --flowstatus "%~1"
    exit /b
)

if "%~3"=="" (
    "%bin_dir%\minificontroller.exe" --port "%~1" --flowstatus "%~2"
    exit /b
)

"%bin_dir%\minificontroller.exe" --host "%~1" --port "%~2" --flowstatus "%~3"
exit /b

:usage
echo Usage: flowStatus.bat [^<host^>] [^<port^>] ^<flowstatus^>
exit /b
