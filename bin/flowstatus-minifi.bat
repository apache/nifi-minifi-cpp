@echo off &setlocal enabledelayedexpansion
rem SPDX-License-Identifier: Apache-2.0
rem SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
rem SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
