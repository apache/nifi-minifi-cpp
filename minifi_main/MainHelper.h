/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MAIN_MAINHELPER_H_
#define MAIN_MAINHELPER_H_

#include <string>
#include <filesystem>

#include "core/logging/LoggerConfiguration.h"
#include "core/logging/Logger.h"
#include "Defaults.h"

#ifdef WIN32
extern "C" {
  FILE* __cdecl _imp____iob_func();

  FILE* __cdecl __imp___iob_func();
}
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

 //! Main thread sleep interval 1 second
#define SLEEP_INTERVAL 1
//! Main thread stop wait time
#define STOP_WAIT_TIME_MS 30*1000
//! Default YAML location

//! Define home environment variable
#define MINIFI_HOME_ENV_KEY "MINIFI_HOME"

#ifdef _MSC_VER
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
#endif

/**
 * Validates a MINIFI_HOME value.
 * @param home_path
 * @return true if home_path represents a valid MINIFI_HOME
 */
bool validHome(const std::string &home_path);

/**
 * Configures the logger to log everything to syslog/Windows Event Log, and for the minimum log level to INFO
 */
void setSyslogLogger();

/**
 * Determines the full path of MINIFI_HOME
 * @return MINIFI_HOME on success, empty string on failure
 */
std::filesystem::path determineMinifiHome(const std::shared_ptr<org::apache::nifi::minifi::core::logging::Logger>& logger);

std::shared_ptr<org::apache::nifi::minifi::Locations> determineLocations(const std::shared_ptr<org::apache::nifi::minifi::core::logging::Logger>& logger);


#endif /* MAIN_MAINHELPER_H_ */
