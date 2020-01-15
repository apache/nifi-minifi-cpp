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

#include "MainHelper.h"

#include "utils/Environment.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"

#ifdef WIN32
FILE* __cdecl _imp____iob_func()
{
  struct _iobuf_VS2012 { // ...\Microsoft Visual Studio 11.0\VC\include\stdio.h #56
    char *_ptr;
    int   _cnt;
    char *_base;
    int   _flag;
    int   _file;
    int   _charbuf;
    int   _bufsiz;
    char *_tmpfname;
  };
  // VS2015 has FILE = struct {void* _Placeholder}

  static struct _iobuf_VS2012 bufs[3];
  static char initialized = 0;

  if (!initialized) {
    bufs[0]._ptr = (char*)stdin->_Placeholder;
    bufs[1]._ptr = (char*)stdout->_Placeholder;
    bufs[2]._ptr = (char*)stderr->_Placeholder;
    initialized = 1;
  }

  return (FILE*)&bufs;
}

FILE* __cdecl __imp___iob_func()
{
  struct _iobuf_VS2012 { // ...\Microsoft Visual Studio 11.0\VC\include\stdio.h #56
    char *_ptr;
    int   _cnt;
    char *_base;
    int   _flag;
    int   _file;
    int   _charbuf;
    int   _bufsiz;
    char *_tmpfname;
};
  // VS2015 has FILE = struct {void* _Placeholder}

  static struct _iobuf_VS2012 bufs[3];
  static char initialized = 0;

  if (!initialized) {
    bufs[0]._ptr = (char*)stdin->_Placeholder;
    bufs[1]._ptr = (char*)stdout->_Placeholder;
    bufs[2]._ptr = (char*)stderr->_Placeholder;
    initialized = 1;
  }

  return (FILE*)&bufs;
}

#endif

bool validHome(const std::string &home_path) {
  struct stat stat_result { };
  const std::string properties_file_path = utils::file::FileUtils::concat_path(home_path, DEFAULT_NIFI_PROPERTIES_FILE);
  return stat(properties_file_path.c_str(), &stat_result) == 0;
}

void setSyslogLogger() {
  std::shared_ptr<logging::LoggerProperties> service_logger = std::make_shared<logging::LoggerProperties>();
  service_logger->set("appender.syslog", "syslog");
  service_logger->set("logger.root", "INFO,syslog");
  logging::LoggerConfiguration::getConfiguration().initialize(service_logger);
}

std::string determineMinifiHome(const std::shared_ptr<logging::Logger>& logger){
  /* Try to determine MINIFI_HOME */
  std::string minifiHome = [&logger]() -> std::string {
    /* If MINIFI_HOME is set as an environment variable, we will use that */
    bool minifiHomeSet = false;
    std::string minifiHome;
    std::tie(minifiHomeSet, minifiHome) = utils::Environment::getEnvironmentVariable(MINIFI_HOME_ENV_KEY);
    if (minifiHomeSet) {
      logger->log_info("Found " MINIFI_HOME_ENV_KEY "=%s in environment", minifiHome);
      return minifiHome;
    } else {
      logger->log_info(MINIFI_HOME_ENV_KEY " is not set; trying to infer it");
    }

    /* Try to determine MINIFI_HOME relative to the location of the minifi executable */
    std::string executablePath = utils::file::FileUtils::get_executable_path();
    if (executablePath.empty()) {
      logger->log_error("Failed to determine location of the minifi executable");
    } else {
      std::string minifiPath, minifiFileName;
      std::tie(minifiPath, minifiFileName) = minifi::utils::file::FileUtils::split_path(executablePath);
      logger->log_info("Inferred " MINIFI_HOME_ENV_KEY "=%s based on the minifi executable location %s", minifiPath, executablePath);
      return minifiPath;
    }

#ifndef WIN32
    /* Try to determine MINIFI_HOME relative to the current working directory */
    std::string cwd = utils::Environment::getCurrentWorkingDirectory();
    if (cwd.empty()) {
      logger->log_error("Failed to determine current working directory");
    } else {
      logger->log_info("Inferred " MINIFI_HOME_ENV_KEY "=%s based on the current working directory %s", cwd, cwd);
      return cwd;
    }
#endif

    return "";
  }();

  if (minifiHome.empty()) {
    logger->log_error("No " MINIFI_HOME_ENV_KEY " could be inferred. "
                      "Please set " MINIFI_HOME_ENV_KEY " or run minifi from a valid location.");
    return "";
  }

  /* Verify that MINIFI_HOME is valid */
  bool minifiHomeValid = false;
  if (validHome(minifiHome)) {
    minifiHomeValid = true;
  } else {
    logger->log_info("%s is not a valid " MINIFI_HOME_ENV_KEY ", because there is no " DEFAULT_NIFI_PROPERTIES_FILE " file in it.", minifiHome);

    std::string minifiHomeWithoutBin, binDir;
    std::tie(minifiHomeWithoutBin, binDir) = minifi::utils::file::FileUtils::split_path(minifiHome);
    if (minifiHomeWithoutBin != "" && (binDir == "bin" || binDir == std::string("bin") + minifi::utils::file::FileUtils::get_separator())) {
      if (validHome(minifiHomeWithoutBin)) {
        logger->log_info("%s is a valid " MINIFI_HOME_ENV_KEY ", falling back to it.", minifiHomeWithoutBin);
        minifiHomeValid = true;
        minifiHome = std::move(minifiHomeWithoutBin);
      } else {
        logger->log_info("%s is not a valid " MINIFI_HOME_ENV_KEY ", because there is no " DEFAULT_NIFI_PROPERTIES_FILE " file in it.", minifiHomeWithoutBin);
      }
    }
  }

  /* Fail if not */
  if (!minifiHomeValid) {
    logger->log_error("Cannot find a valid " MINIFI_HOME_ENV_KEY " containing a " DEFAULT_NIFI_PROPERTIES_FILE " file in it. "
                      "Please set " MINIFI_HOME_ENV_KEY " or run minifi from a valid location.");
    return "";
  }

  /* Set the valid MINIFI_HOME in our environment */
  logger->log_info("Using " MINIFI_HOME_ENV_KEY "=%s", minifiHome);
  utils::Environment::setEnvironmentVariable(MINIFI_HOME_ENV_KEY, minifiHome.c_str());

  return minifiHome;
}
