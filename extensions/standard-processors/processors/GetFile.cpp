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
#include "GetFile.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <cstring>
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/TimeUtil.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "core/TypedValues.h"
#include "utils/FileReaderCallback.h"
#include "utils/RegexUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

core::Property GetFile::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")->withDescription("The maximum number of files to pull in each iteration")->withDefaultValue<uint32_t>(10)->build());

core::Property GetFile::Directory(
    core::PropertyBuilder::createProperty("Input Directory")->withDescription("The input directory from which to pull files")->isRequired(true)->supportsExpressionLanguage(true)
        ->build());

core::Property GetFile::IgnoreHiddenFile(
    core::PropertyBuilder::createProperty("Ignore Hidden Files")->withDescription("Indicates whether or not hidden files should be ignored")->withDefaultValue<bool>(true)->build());

core::Property GetFile::KeepSourceFile(
    core::PropertyBuilder::createProperty("Keep Source File")->withDescription("If true, the file is not deleted after it has been copied to the Content Repository")->withDefaultValue<bool>(false)
        ->build());

core::Property GetFile::MaxAge(
    core::PropertyBuilder::createProperty("Maximum File Age")->withDescription("The maximum age that a file must be in order to be pulled;"
                                                                               " any file older than this amount of time (according to last modification date) will be ignored")
        ->withDefaultValue<core::TimePeriodValue>("0 sec")->build());

core::Property GetFile::MinAge(
    core::PropertyBuilder::createProperty("Minimum File Age")->withDescription("The minimum age that a file must be in order to be pulled;"
                                                                               " any file younger than this amount of time (according to last modification date) will be ignored")
        ->withDefaultValue<core::TimePeriodValue>("0 sec")->build());

core::Property GetFile::MaxSize(
    core::PropertyBuilder::createProperty("Maximum File Size")->withDescription("The maximum size that a file can be in order to be pulled")->withDefaultValue<core::DataSizeValue>("0 B")->build());

core::Property GetFile::MinSize(
    core::PropertyBuilder::createProperty("Minimum File Size")->withDescription("The minimum size that a file can be in order to be pulled")->withDefaultValue<core::DataSizeValue>("0 B")->build());

core::Property GetFile::PollInterval(
    core::PropertyBuilder::createProperty("Polling Interval")->withDescription("Indicates how long to wait before performing a directory listing")->withDefaultValue<core::TimePeriodValue>("0 sec")
        ->build());

core::Property GetFile::Recurse(
    core::PropertyBuilder::createProperty("Recurse Subdirectories")->withDescription("Indicates whether or not to pull files from subdirectories")->withDefaultValue<bool>(true)->build());

core::Property GetFile::FileFilter(
    core::PropertyBuilder::createProperty("File Filter")->withDescription("Only files whose names match the given regular expression will be picked up")->withDefaultValue("[^\\.].*")->build());

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

void GetFile::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  std::string value;
  if (context->getProperty(BatchSize.getName(), value)) {
    core::Property::StringToInt(value, request_.batchSize);
  }
  if (context->getProperty(IgnoreHiddenFile.getName(), value)) {
    request_.ignoreHiddenFile = org::apache::nifi::minifi::utils::StringUtils::toBool(value).value_or(true);
  }
  if (context->getProperty(KeepSourceFile.getName(), value)) {
    request_.keepSourceFile = org::apache::nifi::minifi::utils::StringUtils::toBool(value).value_or(false);
  }

  if (auto max_age = context->getProperty<core::TimePeriodValue>(MaxAge))
    request_.maxAge = max_age->getMilliseconds();
  if (auto min_age = context->getProperty<core::TimePeriodValue>(MinAge))
    request_.minAge = min_age->getMilliseconds();

  if (context->getProperty(MaxSize.getName(), value)) {
    core::Property::StringToInt(value, request_.maxSize);
  }
  if (context->getProperty(MinSize.getName(), value)) {
    core::Property::StringToInt(value, request_.minSize);
  }

  if (const auto poll_interval = context->getProperty<core::TimePeriodValue>(PollInterval)) {
    request_.pollInterval = poll_interval->getMilliseconds();
  }

  if (context->getProperty(Recurse.getName(), value)) {
    request_.recursive = org::apache::nifi::minifi::utils::StringUtils::toBool(value).value_or(true);
  }

  if (context->getProperty(FileFilter.getName(), value)) {
    request_.fileFilter = value;
  }

  if (!context->getProperty(Directory.getName(), value)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Input Directory property is missing");
  }
  if (!utils::file::is_directory(value)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Input Directory \"" + value + "\" is not a directory");
  }
  request_.inputDirectory = value;
}

void GetFile::onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* session) {
  metrics_->iterations_++;

  const bool is_dir_empty_before_poll = isListingEmpty();
  logger_->log_debug("Listing is %s before polling directory", is_dir_empty_before_poll ? "empty" : "not empty");
  if (is_dir_empty_before_poll) {
    if (request_.pollInterval == 0ms || (std::chrono::system_clock::now() - last_listing_time_.load()) > request_.pollInterval) {
      performListing(request_);
      last_listing_time_.store(std::chrono::system_clock::now());
    }
  }

  const bool is_dir_empty_after_poll = isListingEmpty();
  logger_->log_debug("Listing is %s after polling directory", is_dir_empty_after_poll ? "empty" : "not empty");
  if (is_dir_empty_after_poll) {
    yield();
    return;
  }

  std::queue<std::string> list_of_file_names = pollListing(request_.batchSize);
  while (!list_of_file_names.empty()) {
    std::string file_name = list_of_file_names.front();
    list_of_file_names.pop();
    getSingleFile(*session, file_name);
  }
}

void GetFile::getSingleFile(core::ProcessSession& session, const std::string& file_name) const {
  logger_->log_info("GetFile process %s", file_name);
  auto flow_file = session.create();
  gsl_Expects(flow_file);
  std::string path;
  std::string name;
  std::tie(path, name) = utils::file::split_path(file_name);
  flow_file->setAttribute(core::SpecialFlowAttribute::FILENAME, name);
  flow_file->setAttribute(core::SpecialFlowAttribute::PATH, path);
  flow_file->addAttribute(core::SpecialFlowAttribute::ABSOLUTE_PATH, file_name);

  try {
    utils::FileReaderCallback file_reader_callback{file_name};
    session.write(flow_file, &file_reader_callback);
    session.transfer(flow_file, Success);
    if (!request_.keepSourceFile) {
      auto remove_status = remove(file_name.c_str());
      if (remove_status != 0) {
        logger_->log_error("GetFile could not delete file '%s', error %d: %s", file_name, errno, strerror(errno));
      }
    }
  } catch (const utils::FileReaderCallbackIOError& io_error) {
    logger_->log_error("IO error while processing file '%s': %s", file_name, io_error.what());
    flow_file->setDeleted(true);
  }
}

bool GetFile::isListingEmpty() const {
  std::lock_guard<std::mutex> lock(directory_listing_mutex_);

  return directory_listing_.empty();
}

void GetFile::putListing(std::string fileName) {
  logger_->log_trace("Adding file to queue: %s", fileName);

  std::lock_guard<std::mutex> lock(directory_listing_mutex_);

  directory_listing_.push(fileName);
}

std::queue<std::string> GetFile::pollListing(uint64_t batch_size) {
  std::lock_guard<std::mutex> lock(directory_listing_mutex_);

  std::queue<std::string> list;
  while (!directory_listing_.empty() && (batch_size == 0 || list.size() < batch_size)) {
    list.push(directory_listing_.front());
    directory_listing_.pop();
  }
  return list;
}

bool GetFile::fileMatchesRequestCriteria(std::string fullName, std::string name, const GetFileRequest &request) {
  logger_->log_trace("Checking file: %s", fullName);

#ifdef WIN32
  struct _stat64 statbuf;
  if (_stat64(fullName.c_str(), &statbuf) != 0) {
    return false;
  }
#else
  struct stat statbuf;
  if (stat(fullName.c_str(), &statbuf) != 0) {
    return false;
  }
#endif
  uint64_t file_size = gsl::narrow<uint64_t>(statbuf.st_size);
  auto modifiedTime = std::chrono::system_clock::time_point() + std::chrono::seconds(gsl::narrow<uint64_t>(statbuf.st_mtime));

  if (request.minSize > 0 && file_size < request.minSize)
    return false;

  if (request.maxSize > 0 && file_size > request.maxSize)
    return false;

  auto fileAge = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - modifiedTime);
  if (request.minAge > 0ms && fileAge < request.minAge)
    return false;
  if (request.maxAge > 0ms && fileAge > request.maxAge)
    return false;

  if (request.ignoreHiddenFile && utils::file::is_hidden(fullName))
    return false;

  utils::Regex rgx(request.fileFilter);
  if (!utils::regexSearch(name, rgx)) {
    return false;
  }

  metrics_->input_bytes_ += file_size;
  metrics_->accepted_files_++;
  return true;
}

void GetFile::performListing(const GetFileRequest &request) {
  auto callback = [this, request](const std::string& dir, const std::string& filename) -> bool {
    std::string fullpath = dir + utils::file::get_separator() + filename;
    if (fileMatchesRequestCriteria(fullpath, filename, request)) {
      putListing(fullpath);
    }
    return isRunning();
  };
  utils::file::list_dir(request.inputDirectory, callback, logger_, request.recursive);
}

int16_t GetFile::getMetricNodes(std::vector<std::shared_ptr<state::response::ResponseNode>> &metric_vector) {
  metric_vector.push_back(metrics_);
  return 0;
}

REGISTER_RESOURCE(GetFile, "Creates FlowFiles from files in a directory. MiNiFi will ignore files for which it doesn't have read permissions.");

}  // namespace org::apache::nifi::minifi::processors
