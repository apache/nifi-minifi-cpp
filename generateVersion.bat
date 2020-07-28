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

set version=%1
set version=%version:"=%
set src_dir=%2
set src_dir=%src_dir:"=%
set out_dir=%3
set out_dir=%out_dir:"=%
set compiler=%4
set compiler=%compiler:"=%
set compiler_version=%5
set compiler_version=%compiler_version:"=%
set flags=%6
set flags=%flags:"=%
set extensions=%7
set extensions=%extensions:"=%
set extensions=%extensions:\=%
set buildident=%8
set buildident=%buildident:"=%
set buildrev=%9
set buildrev=%buildrev:"=%


call :GetUnixTime builddate


set /a count=0
for %%i in (%extensions%) do (
    set /a count+=1
    set "extension!count!=%%i"
)

(
    echo #include ^<string^>
	echo #include ^<vector^>
	echo #include "agent/agent_version.h"

	echo namespace org {
	echo namespace apache {
	echo namespace nifi {
	echo namespace minifi {

	echo const char* const AgentBuild::VERSION = "%version%";
	echo const char* const AgentBuild::BUILD_IDENTIFIER = "%buildident%";
	echo const char* const AgentBuild::BUILD_REV = "%buildrev%";
	echo const char* const AgentBuild::BUILD_DATE = "%builddate%";
	echo const char* const AgentBuild::COMPILER = "%compiler%";
	echo const char* const AgentBuild::COMPILER_VERSION = "%compiler_version%";
	echo const char* const AgentBuild::COMPILER_FLAGS = "%flags%";
	echo std^:^:vector^<std^:^:string^> AgentBuild::getExtensions^(^) {
  	echo 	static std^:^:vector^<std^:^:string^> extensions;
  	echo 	if ^(extensions.empty^(^)^){
	) > "%out_dir%/agent_version.cpp"

	for /l %%i in (1,1,%count%) do (
	   (
				echo extensions.push_back^("!extension%%i!"^);
		)>> "%out_dir%/agent_version.cpp"
	)

	
(
	echo 	 extensions.push_back^("minifi-system"^);
	echo   }
  	echo   return extensions;
	echo }

	echo } /* namespace minifi */
	echo } /* namespace nifi */
	echo } /* namespace apache */
	echo } /* namespace org */
) >> "%out_dir%/agent_version.cpp"

 goto :EOF

 rem see license regarding the following snippet of code taken from 
 rem https://github.com/ritchielawrence/batchfunctionlibrary

:GetUnixTime
setlocal enableextensions
for /f %%x in ('wmic path win32_utctime get /format:list ^| findstr "="') do (
    set %%x)
set /a z=(14-100%Month%%%100)/12, y=10000%Year%%%10000-z
set /a ut=y*365+y/4-y/100+y/400+(153*(100%Month%%%100+12*z-3)+2)/5+Day-719469
set /a ut=ut*86400+100%Hour%%%100*3600+100%Minute%%%100*60+100%Second%%%100
endlocal & set "%1=%ut%" & goto :EOF
