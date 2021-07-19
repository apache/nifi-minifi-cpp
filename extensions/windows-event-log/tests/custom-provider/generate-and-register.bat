@echo off &setlocal enabledelayedexpansion
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

cd %1

rem Define full path to file and remove quotation marks
set man_path=%1"\unit-test-provider.man"
set man_path=%man_path:"=%

set dll_path=%1"\unit-test-provider.dll"
set dll_path=%dll_path:"=%

(
  echo ^<?xml version="1.0" encoding="UTF-8"?^>
  echo ^<instrumentationManifest xsi:schemaLocation="http://schemas.microsoft.com/win/2004/08/events eventman.xsd"
  echo    xmlns="http://schemas.microsoft.com/win/2004/08/events"
  echo    xmlns:win="http://manifests.microsoft.com/win/2004/08/windows/events"
  echo    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
  echo    xmlns:xs="http://www.w3.org/2001/XMLSchema"
  echo    xmlns:trace="http://schemas.microsoft.com/win/2004/08/events/trace"^>
  echo ^<instrumentation^>
  echo   ^<events^>
  echo     ^<provider name="minifi_unit_test_provider"
  echo        symbol="minifi_unit_test_provider"
  echo          guid="{ABCDEF01-8174-F1CA-87BE-DA129FF6001B}"
  echo          resourceFileName="%dll_path%"
  echo          messageFileName="%dll_path%"^>
  echo        ^<events^>
  echo          ^<event symbol="CustomEvent" value="10000" version="1" channel="minifi_unit_test_provider/Log" template="CustomTemplate" /^>
  echo        ^</events^>
  echo        ^<levels/^>
  echo        ^<tasks/^>
  echo        ^<opcodes/^>
  echo        ^<channels^>
  echo          ^<channel name="minifi_unit_test_provider/Log" value="0x10" type="Operational" enabled="true" /^>
  echo        ^</channels^>
  echo        ^<templates^>
  echo          ^<template tid="CustomTemplate"^>
  echo            ^<data name="param1" inType="win:UnicodeString" outType="xs:string" /^>
  echo            ^<data name="param2" inType="win:UnicodeString" outType="xs:string" /^>
  echo            ^<data name="Channel" inType="win:UnicodeString" outType="xs:string" /^>
  echo            ^<binary /^>
  echo          ^</template^>
  echo        ^</templates^>
  echo      ^</provider^>
  echo    ^</events^>
  echo  ^</instrumentation^>
  echo  ^<localization/^>
  echo  ^</instrumentationManifest^>
) > "%man_path%"

mc -css Namespace unit-test-provider.man
mc -um unit-test-provider.man
rc unit-test-provider.rc
csc /target:library /unsafe /win32res:unit-test-provider.res unit-test-provider.cs
wevtutil im unit-test-provider.man
