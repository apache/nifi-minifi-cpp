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
#include "processors/GetFile.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <limits.h>
#ifndef WIN32
#include <regex.h>
#else
#include <regex>
#endif
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <iostream>
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/TimeUtil.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/TypedValues.h"

#define R_OK    4       /* Test for read permission.  */
#define W_OK    2       /* Test for write permission.  */
#define F_OK    0       /* Test for existence.  */

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property GetFile::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")->withDescription("The maximum number of files to pull in each iteration")->withDefaultValue<uint32_t>(10)
        ->build());

core::Property GetFile::Directory(
    core::PropertyBuilder::createProperty("Input Directory")->withDescription("The input directory from which to pull files")->isRequired(true)->supportsExpressionLanguage(true)->withDefaultValue(".")
        ->build());

core::Property GetFile::IgnoreHiddenFile(
    core::PropertyBuilder::createProperty("Ignore Hidden Files")->withDescription("Indicates whether or not hidden files should be ignored")->withDefaultValue<bool>(true)
        ->build());

core::Property GetFile::KeepSourceFile(
    core::PropertyBuilder::createProperty("Keep Source File")->withDescription("If true, the file is not deleted after it has been copied to the Content Repository")->withDefaultValue<bool>(false)
        ->build());

core::Property GetFile::MaxAge(
    core::PropertyBuilder::createProperty("Maximum File Age")->withDescription("The maximum age that a file must be in order to be pulled;"
                               " any file older than this amount of time (according to last modification date) will be ignored")->withDefaultValue<core::TimePeriodValue>("0 sec")
        ->build());

core::Property GetFile::MinAge(
    core::PropertyBuilder::createProperty("Minimum File Age")->withDescription("The minimum age that a file must be in order to be pulled;"
                               " any file younger than this amount of time (according to last modification date) will be ignored")->withDefaultValue<core::TimePeriodValue>("0 sec")
        ->build());

core::Property GetFile::MaxSize(
    core::PropertyBuilder::createProperty("Minimum File Size")->withDescription("The maximum size that a file can be in order to be pulled")->withDefaultValue<core::DataSizeValue>("0 B")
        ->build());

core::Property GetFile::MinSize(
    core::PropertyBuilder::createProperty("Minimum File Size")->withDescription("The minimum size that a file can be in order to be pulled")->withDefaultValue<core::DataSizeValue>("0 B")
        ->build());

core::Property GetFile::PollInterval(
    core::PropertyBuilder::createProperty("Polling Interval")->withDescription("Indicates how long to wait before performing a directory listing")->withDefaultValue<core::TimePeriodValue>("0 sec")
        ->build());

core::Property GetFile::Recurse(
    core::PropertyBuilder::createProperty("Recurse Subdirectories")->withDescription("Indicates whether or not to pull files from subdirectories")->withDefaultValue<bool>(true)
        ->build());

core::Property GetFile::FileFilter(
    core::PropertyBuilder::createProperty("File Filter")->withDescription("Only files whose names match the given regular expression will be picked up")->withDefaultValue("[^\\.].*")
        ->build());

core::Relationship GetFile::Success("success", "All files are routed to success");

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

void GetFile::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
  std::string value;
  if (context->getProperty(BatchSize.getName(), value)) {
    core::Property::StringToInt(value, request_.batchSize);
  }
  if (context->getProperty(IgnoreHiddenFile.getName(), value)) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, request_.ignoreHiddenFile);
  }
  if (context->getProperty(KeepSourceFile.getName(), value)) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, request_.keepSourceFile);
  }

  context->getProperty(MaxAge.getName(), request_.maxAge);
  context->getProperty(MinAge.getName(), request_.minAge);

  if (context->getProperty(MaxSize.getName(), value)) {
    core::Property::StringToInt(value, request_.maxSize);
  }
  if (context->getProperty(MinSize.getName(), value)) {
    core::Property::StringToInt(value, request_.minSize);
  }

  context->getProperty(PollInterval.getName(), request_.pollInterval);

  if (context->getProperty(Recurse.getName(), value)) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, request_.recursive);
  }

  if (context->getProperty(FileFilter.getName(), value)) {
    request_.fileFilter = value;
  }
}

void GetFile::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  // Perform directory list

  metrics_->iterations_++;

  logger_->log_debug("Is listing empty %i", isListingEmpty());
  if (isListingEmpty()) {
    if (request_.pollInterval == 0 || (getTimeMillis() - last_listing_time_) > request_.pollInterval) {
      std::string directory;
      const std::shared_ptr<core::FlowFile> flow_file;
      if (!context->getProperty(Directory, directory, flow_file)) {
        logger_->log_warn("Resolved missing Input Directory property value");
      }
      performListing(directory, request_);
      last_listing_time_.store(getTimeMillis());
    }
  }
  logger_->log_debug("Is listing empty %i", isListingEmpty());

  if (!isListingEmpty()) {
    try {
      std::queue<std::string> list;
      pollListing(list, request_);
      while (!list.empty()) {
        std::string fileName = list.front();
        list.pop();
        logger_->log_info("GetFile process %s", fileName);
        std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<FlowFileRecord>(session->create());
        if (flowFile == nullptr)
          return;
        std::size_t found = fileName.find_last_of("/\\");
        std::string path = fileName.substr(0, found);
        std::string name = fileName.substr(found + 1);
        flowFile->updateKeyedAttribute(FILENAME, name);
        flowFile->updateKeyedAttribute(PATH, path);
        flowFile->addKeyedAttribute(ABSOLUTE_PATH, fileName);
        session->import(fileName, flowFile, request_.keepSourceFile);
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
  logger_->log_trace("Adding file to queue: %s", fileName);

  std::lock_guard<std::mutex> lock(mutex_);

  _dirList.push(fileName);
}

void GetFile::pollListing(std::queue<std::string> &list, const GetFileRequest &request) {
  std::lock_guard<std::mutex> lock(mutex_);

  while (!_dirList.empty() && (request.batchSize == 0 || list.size() < request.batchSize)) {
    list.push(_dirList.front());
    _dirList.pop();
  }
}

bool GetFile::acceptFile(std::string fullName, std::string name, const GetFileRequest &request) {
  logger_->log_trace("Checking file: %s", fullName);

  struct stat statbuf;

  if (stat(fullName.c_str(), &statbuf) == 0) {
    if (request.minSize > 0 && statbuf.st_size < (int32_t) request.minSize)
      return false;

    if (request.maxSize > 0 && statbuf.st_size > (int32_t) request.maxSize)
      return false;

    uint64_t modifiedTime = ((uint64_t) (statbuf.st_mtime) * 1000);
    uint64_t fileAge = getTimeMillis() - modifiedTime;
    if (request.minAge > 0 && fileAge < request.minAge)
      return false;
    if (request.maxAge > 0 && fileAge > request.maxAge)
      return false;

    if (request.ignoreHiddenFile && name.c_str()[0] == '.')
      return false;

    if (access(fullName.c_str(), R_OK) != 0)
      return false;

    if (request.keepSourceFile == false && access(fullName.c_str(), W_OK) != 0)
      return false;
#ifndef WIN32
    regex_t regex;
    int ret = regcomp(&regex, request.fileFilter.c_str(), 0);
    if (ret)
      return false;
    ret = regexec(&regex, name.c_str(), (size_t) 0, NULL, 0);
    regfree(&regex);
    if (ret)
      return false;
#else
    std::regex regex(request.fileFilter);
    if (!std::regex_match(name, regex)) {
      return false;
    }
#endif
    metrics_->input_bytes_ += statbuf.st_size;
    metrics_->accepted_files_++;
    return true;
  }

  return false;
}

void GetFile::performListing(std::string dir, const GetFileRequest &request) {
  auto callback = [this, request](const std::string& dir, const std::string& filename) -> bool{
    std::string fullpath = dir + utils::file::FileUtils::get_separator() + filename;
    if (acceptFile(fullpath, filename, request)) {
      putListing(fullpath);
    }
    return isRunning();
  };
  utils::file::FileUtils::list_dir(dir, callback, logger_, request.recursive);
}

int16_t GetFile::getMetricNodes(std::vector<std::shared_ptr<state::response::ResponseNode>> &metric_vector) {
  metric_vector.push_back(metrics_);
  return 0;
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
