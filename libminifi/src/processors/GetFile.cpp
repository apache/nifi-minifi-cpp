/**
 * @file GetFile.cpp
 * GetFile class implementation
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
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <iostream>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#if  (__GNUC__ >= 4) 
#if (__GNUC_MINOR__ < 9)
#include <regex.h>
#endif
#endif
#include "utils/StringUtils.h"
#include <regex>
#include "utils/TimeUtil.h"
#include "processors/GetFile.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
const std::string GetFile::ProcessorName("GetFile");
core::Property GetFile::BatchSize(
    "Batch Size", "The maximum number of files to pull in each iteration",
    "10");
core::Property GetFile::Directory(
    "Input Directory", "The input directory from which to pull files", ".");
core::Property GetFile::IgnoreHiddenFile(
    "Ignore Hidden Files",
    "Indicates whether or not hidden files should be ignored", "true");
core::Property GetFile::KeepSourceFile(
    "Keep Source File",
    "If true, the file is not deleted after it has been copied to the Content Repository",
    "false");
core::Property GetFile::MaxAge(
    "Maximum File Age",
    "The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored",
    "0 sec");
core::Property GetFile::MinAge(
    "Minimum File Age",
    "The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored",
    "0 sec");
core::Property GetFile::MaxSize(
    "Maximum File Size",
    "The maximum size that a file can be in order to be pulled", "0 B");
core::Property GetFile::MinSize(
    "Minimum File Size",
    "The minimum size that a file must be in order to be pulled", "0 B");
core::Property GetFile::PollInterval(
    "Polling Interval",
    "Indicates how long to wait before performing a directory listing",
    "0 sec");
core::Property GetFile::Recurse(
    "Recurse Subdirectories",
    "Indicates whether or not to pull files from subdirectories", "true");
core::Property GetFile::FileFilter(
    "File Filter",
    "Only files whose names match the given regular expression will be picked up",
    "[^\\.].*");
core::Relationship GetFile::Success(
    "success", "All files are routed to success");

void GetFile::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(BatchSize);
  properties.insert(Directory);
  properties.insert(IgnoreHiddenFile);
  properties.insert(KeepSourceFile);
  properties.insert(MaxAge);
  properties.insert(MinAge);
  properties.insert(MaxSize);
  properties.insert(MinSize);
  properties.insert(PollInterval);
  properties.insert(Recurse);
  properties.insert(FileFilter);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void GetFile::onTrigger(
    core::ProcessContext *context,
    core::ProcessSession *session) {
  std::string value;

  logger_->log_info("onTrigger GetFile");
  if (context->getProperty(Directory.getName(), value)) {
    _directory = value;
  }
  if (context->getProperty(BatchSize.getName(), value)) {
    core::Property::StringToInt(value, _batchSize);
  }
  if (context->getProperty(IgnoreHiddenFile.getName(), value)) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(
        value, _ignoreHiddenFile);
  }
  if (context->getProperty(KeepSourceFile.getName(), value)) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(
        value, _keepSourceFile);
  }

  logger_->log_info("onTrigger GetFile");
  if (context->getProperty(MaxAge.getName(), value)) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, _maxAge,
                                                                unit)
        && core::Property::ConvertTimeUnitToMS(
            _maxAge, unit, _maxAge)) {

    }
  }
  if (context->getProperty(MinAge.getName(), value)) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, _minAge,
                                                                unit)
        && core::Property::ConvertTimeUnitToMS(
            _minAge, unit, _minAge)) {

    }
  }
  if (context->getProperty(MaxSize.getName(), value)) {
    core::Property::StringToInt(value, _maxSize);
  }
  if (context->getProperty(MinSize.getName(), value)) {
    core::Property::StringToInt(value, _minSize);
  }
  if (context->getProperty(PollInterval.getName(), value)) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value,
                                                                _pollInterval,
                                                                unit)
        && core::Property::ConvertTimeUnitToMS(
            _pollInterval, unit, _pollInterval)) {

    }
  }
  if (context->getProperty(Recurse.getName(), value)) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(value,
                                                                _recursive);
  }

  if (context->getProperty(FileFilter.getName(), value)) {
    _fileFilter = value;
  }

  // Perform directory list
  logger_->log_info("Is listing empty %i", isListingEmpty());
  if (isListingEmpty()) {
    if (_pollInterval == 0
        || (getTimeMillis() - _lastDirectoryListingTime) > _pollInterval) {
      performListing(_directory);
    }
  }
  logger_->log_info("Is listing empty %i", isListingEmpty());

  if (!isListingEmpty()) {
    try {
      std::queue<std::string> list;
      pollListing(list, _batchSize);
      while (!list.empty()) {

        std::string fileName = list.front();
        list.pop();
        logger_->log_info("GetFile process %s", fileName.c_str());
        std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<
            FlowFileRecord>(session->create());
        if (flowFile == nullptr)
          return;
        std::size_t found = fileName.find_last_of("/\\");
        std::string path = fileName.substr(0, found);
        std::string name = fileName.substr(found + 1);
        flowFile->updateKeyedAttribute(FILENAME, name);
        flowFile->updateKeyedAttribute(PATH, path);
        flowFile->addKeyedAttribute(ABSOLUTE_PATH, fileName);
        session->import(fileName, flowFile, _keepSourceFile);
        session->transfer(flowFile, Success);
      }
    } catch (std::exception &exception) {
      logger_->log_debug("GetFile Caught Exception %s", exception.what());
      throw;
    } catch (...) {
      throw;
    }
  }

}

bool GetFile::isListingEmpty() {
  std::lock_guard<std::mutex> lock(mutex_);

  return _dirList.empty();
}

void GetFile::putListing(std::string fileName) {
  std::lock_guard<std::mutex> lock(mutex_);

  _dirList.push(fileName);
}

void GetFile::pollListing(std::queue<std::string> &list, int maxSize) {
  std::lock_guard<std::mutex> lock(mutex_);

  while (!_dirList.empty() && (maxSize == 0 || list.size() < maxSize)) {
    std::string fileName = _dirList.front();
    _dirList.pop();
    list.push(fileName);
  }

  return;
}

bool GetFile::acceptFile(std::string fullName, std::string name) {
  struct stat statbuf;

  if (stat(fullName.c_str(), &statbuf) == 0) {
    if (_minSize > 0 && statbuf.st_size < _minSize)
      return false;

    if (_maxSize > 0 && statbuf.st_size > _maxSize)
      return false;

    uint64_t modifiedTime = ((uint64_t) (statbuf.st_mtime) * 1000);
    uint64_t fileAge = getTimeMillis() - modifiedTime;
    if (_minAge > 0 && fileAge < _minAge)
      return false;
    if (_maxAge > 0 && fileAge > _maxAge)
      return false;

    if (_ignoreHiddenFile && fullName.c_str()[0] == '.')
      return false;

    if (access(fullName.c_str(), R_OK) != 0)
      return false;

    if (_keepSourceFile == false && access(fullName.c_str(), W_OK) != 0)
      return false;

#ifdef __GNUC__
#if (__GNUC__ >= 4)
#if (__GNUC_MINOR__ < 9)
    regex_t regex;
    int ret = regcomp(&regex, _fileFilter.c_str(), 0);
    if (ret)
      return false;
    ret = regexec(&regex, name.c_str(), (size_t) 0, NULL, 0);
    regfree(&regex);
    if (ret)
      return false;
#else
    try {
      std::regex re(_fileFilter);

      if (!std::regex_match(name, re)) {
        return false;
      }
    } catch (std::regex_error e) {
      logger_->log_error("Invalid File Filter regex: %s.", e.what());
      return false;
    }
#endif
#endif
#else
    logger_->log_info("Cannot support regex filtering");
#endif
    return true;
  }

  return false;
}

void GetFile::performListing(std::string dir) {
  logger_->log_info("Performing file listing against %s", dir.c_str());
  DIR *d;
  d = opendir(dir.c_str());
  if (!d)
    return;
  // only perform a listing while we are not empty
  logger_->log_info("Performing file listing against %s", dir.c_str());
  while (isRunning()) {
    struct dirent *entry;
    entry = readdir(d);
    if (!entry)
      break;
    std::string d_name = entry->d_name;
    if ((entry->d_type & DT_DIR)) {
      // if this is a directory
      if (_recursive && strcmp(d_name.c_str(), "..") != 0
          && strcmp(d_name.c_str(), ".") != 0) {
        std::string path = dir + "/" + d_name;
        performListing(path);
      }
    } else {
      std::string fileName = dir + "/" + d_name;
      if (acceptFile(fileName, d_name)) {
        // check whether we can take this file
        putListing(fileName);
      }
    }
  }
  closedir(d);
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
