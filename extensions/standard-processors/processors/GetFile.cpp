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

#include <queue>
#include <memory>
#include <string>

#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/TimeUtil.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "core/TypedValues.h"
#include "utils/FileReaderCallback.h"
#include "utils/RegexUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

const core::Property GetFile::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")->withDescription("The maximum number of files to pull in each iteration")->withDefaultValue<uint32_t>(10)->build());

const core::Property GetFile::Directory(
    core::PropertyBuilder::createProperty("Input Directory")->withDescription("The input directory from which to pull files")->isRequired(true)->supportsExpressionLanguage(true)
        ->build());

const core::Property GetFile::IgnoreHiddenFile(
    core::PropertyBuilder::createProperty("Ignore Hidden Files")->withDescription("Indicates whether or not hidden files should be ignored")->withDefaultValue<bool>(true)->build());

const core::Property GetFile::KeepSourceFile(
    core::PropertyBuilder::createProperty("Keep Source File")->withDescription("If true, the file is not deleted after it has been copied to the Content Repository")->withDefaultValue<bool>(false)
        ->build());

const core::Property GetFile::MaxAge(
    core::PropertyBuilder::createProperty("Maximum File Age")->withDescription("The maximum age that a file must be in order to be pulled;"
                                                                               " any file older than this amount of time (according to last modification date) will be ignored")
        ->withDefaultValue<core::TimePeriodValue>("0 sec")->build());

const core::Property GetFile::MinAge(
    core::PropertyBuilder::createProperty("Minimum File Age")->withDescription("The minimum age that a file must be in order to be pulled;"
                                                                               " any file younger than this amount of time (according to last modification date) will be ignored")
        ->withDefaultValue<core::TimePeriodValue>("0 sec")->build());

const core::Property GetFile::MaxSize(
    core::PropertyBuilder::createProperty("Maximum File Size")->withDescription("The maximum size that a file can be in order to be pulled")->withDefaultValue<core::DataSizeValue>("0 B")->build());

const core::Property GetFile::MinSize(
    core::PropertyBuilder::createProperty("Minimum File Size")->withDescription("The minimum size that a file can be in order to be pulled")->withDefaultValue<core::DataSizeValue>("0 B")->build());

const core::Property GetFile::PollInterval(
    core::PropertyBuilder::createProperty("Polling Interval")->withDescription("Indicates how long to wait before performing a directory listing")->withDefaultValue<core::TimePeriodValue>("0 sec")
        ->build());

const core::Property GetFile::Recurse(
    core::PropertyBuilder::createProperty("Recurse Subdirectories")->withDescription("Indicates whether or not to pull files from subdirectories")->withDefaultValue<bool>(true)->build());

const core::Property GetFile::FileFilter(
    core::PropertyBuilder::createProperty("File Filter")->withDescription("Only files whose names match the given regular expression will be picked up")->withDefaultValue(".*")->build());

const core::Relationship GetFile::Success("success", "All files are routed to success");

void GetFile::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
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

  if (auto directory_str = context->getProperty(Directory)) {
    if (!utils::file::is_directory(*directory_str)) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Input Directory \"" + value + "\" is not a directory");
    }
    request_.inputDirectory = *directory_str;
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Input Directory property is missing");
  }
}

void GetFile::onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* session) {
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

  std::queue<std::filesystem::path> list_of_file_names = pollListing(request_.batchSize);
  while (!list_of_file_names.empty()) {
    auto file_name = list_of_file_names.front();
    list_of_file_names.pop();
    getSingleFile(*session, file_name);
  }
}

void GetFile::getSingleFile(core::ProcessSession& session, const std::filesystem::path& file_path) const {
  logger_->log_info("GetFile process %s", file_path.string());
  auto flow_file = session.create();
  gsl_Expects(flow_file);

  flow_file->setAttribute(core::SpecialFlowAttribute::FILENAME, file_path.filename().string());
  flow_file->setAttribute(core::SpecialFlowAttribute::ABSOLUTE_PATH, std::filesystem::absolute(file_path.parent_path() / "").string());
  auto relative_path = std::filesystem::relative(file_path.parent_path(), request_.inputDirectory);
  flow_file->setAttribute(core::SpecialFlowAttribute::PATH, (relative_path / "").string());

  try {
    session.write(flow_file, utils::FileReaderCallback{file_path});
    session.transfer(flow_file, Success);
    if (!request_.keepSourceFile) {
      std::error_code remove_error;
      if (!std::filesystem::remove(file_path, remove_error)) {
        logger_->log_error("GetFile could not delete file '%s', error: %s", file_path.string(), remove_error.message());
      }
    }
  } catch (const utils::FileReaderCallbackIOError& io_error) {
    logger_->log_error("IO error while processing file '%s': %s", file_path.string(), io_error.what());
    flow_file->setDeleted(true);
  }
}

bool GetFile::isListingEmpty() const {
  std::lock_guard<std::mutex> lock(directory_listing_mutex_);

  return directory_listing_.empty();
}

void GetFile::putListing(const std::filesystem::path& file_path) {
  logger_->log_trace("Adding file to queue: %s", file_path.string());

  std::lock_guard<std::mutex> lock(directory_listing_mutex_);

  directory_listing_.push(file_path);
}

std::queue<std::filesystem::path> GetFile::pollListing(uint64_t batch_size) {
  std::lock_guard<std::mutex> lock(directory_listing_mutex_);

  std::queue<std::filesystem::path> list;
  while (!directory_listing_.empty() && (batch_size == 0 || list.size() < batch_size)) {
    list.push(directory_listing_.front());
    directory_listing_.pop();
  }
  return list;
}

bool GetFile::fileMatchesRequestCriteria(const std::filesystem::path& full_name, const std::filesystem::path& name, const GetFileRequest &request) {
  logger_->log_trace("Checking file: %s", full_name.string());

  std::error_code ec;
  uint64_t file_size = std::filesystem::file_size(full_name, ec);
  if (ec) {
    logger_->log_error("file_size of %s: %s", full_name.string(), ec.message());
    return false;
  }
  const auto modifiedTime = std::filesystem::last_write_time(full_name, ec);
  if (ec) {
    logger_->log_error("last_write_time of %s: %s", full_name.string(), ec.message());
    return false;
  }

  if (request.minSize > 0 && file_size < request.minSize)
    return false;

  if (request.maxSize > 0 && file_size > request.maxSize)
    return false;

  auto fileAge = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::file_clock::now() - modifiedTime);
  if (request.minAge > 0ms && fileAge < request.minAge)
    return false;
  if (request.maxAge > 0ms && fileAge > request.maxAge)
    return false;

  if (request.ignoreHiddenFile && utils::file::is_hidden(full_name))
    return false;

  utils::Regex rgx(request.fileFilter);
  if (!utils::regexMatch(name.string(), rgx)) {
    return false;
  }

  auto* const getfile_metrics = static_cast<GetFileMetrics*>(metrics_.get());
  getfile_metrics->input_bytes += file_size;
  ++getfile_metrics->accepted_files;
  return true;
}

void GetFile::performListing(const GetFileRequest &request) {
  auto callback = [this, request](const std::filesystem::path& dir, const std::filesystem::path& filename) -> bool {
    auto fullpath = dir / filename;
    if (fileMatchesRequestCriteria(fullpath, filename, request)) {
      putListing(fullpath);
    }
    return isRunning();
  };
  utils::file::list_dir(request.inputDirectory, callback, logger_, request.recursive);
}

REGISTER_RESOURCE(GetFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
