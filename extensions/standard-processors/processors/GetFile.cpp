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


#include <memory>
#include <queue>
#include <string>

#include "ListFile.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "core/TypedValues.h"
#include "utils/RegexUtils.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"
#include "utils/ConfigurationUtils.h"
#include "utils/file/FileReaderCallback.h"
#include "utils/file/FileUtils.h"
#include "utils/ProcessorConfigUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

void GetFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void GetFile::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  buffer_size_ = utils::configuration::getBufferSize(*context.getConfiguration());

  request_.batchSize = utils::parseU64Property(context, BatchSize);
  request_.ignoreHiddenFile = utils::parseBoolProperty(context, IgnoreHiddenFile);
  request_.keepSourceFile = utils::parseBoolProperty(context, KeepSourceFile);
  request_.maxAge = utils::parseDurationProperty(context, MaxAge);
  request_.minAge = utils::parseDurationProperty(context, MinAge);
  request_.maxSize = utils::parseDataSizeProperty(context, MaxSize);
  request_.minSize = utils::parseDataSizeProperty(context, MinSize);
  request_.pollInterval = utils::parseDurationProperty(context, PollInterval);
  request_.recursive = utils::parseBoolProperty(context, Recurse);
  request_.fileFilter = utils::parseProperty(context, FileFilter);

  if (auto directory_str = context.getProperty(Directory, nullptr)) {
    if (!utils::file::is_directory(*directory_str)) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, utils::string::join_pack("Input Directory \"", *directory_str, "\" is not a directory"));
    }
    request_.inputDirectory = *directory_str;
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Input Directory property is missing");
  }
}

void GetFile::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  const bool is_dir_empty_before_poll = isListingEmpty();
  logger_->log_debug("Listing is {} before polling directory", is_dir_empty_before_poll ? "empty" : "not empty");
  if (is_dir_empty_before_poll) {
    if (request_.pollInterval == 0ms || (std::chrono::system_clock::now() - last_listing_time_.load()) > request_.pollInterval) {
      performListing(request_);
      last_listing_time_.store(std::chrono::system_clock::now());
    }
  }

  const bool is_dir_empty_after_poll = isListingEmpty();
  logger_->log_debug("Listing is {} after polling directory", is_dir_empty_after_poll ? "empty" : "not empty");
  if (is_dir_empty_after_poll) {
    context.yield();
    return;
  }

  std::queue<std::filesystem::path> list_of_file_names = pollListing(request_.batchSize);
  while (!list_of_file_names.empty()) {
    auto file_name = list_of_file_names.front();
    list_of_file_names.pop();
    getSingleFile(session, file_name);
  }
}

void GetFile::getSingleFile(core::ProcessSession& session, const std::filesystem::path& file_path) const {
  logger_->log_info("GetFile process {}", file_path);
  auto flow_file = session.create();
  gsl_Expects(flow_file);

  flow_file->setAttribute(core::SpecialFlowAttribute::FILENAME, file_path.filename().string());
  flow_file->setAttribute(core::SpecialFlowAttribute::ABSOLUTE_PATH, std::filesystem::absolute(file_path.parent_path() / "").string());
  auto relative_path = std::filesystem::relative(file_path.parent_path(), request_.inputDirectory);
  flow_file->setAttribute(core::SpecialFlowAttribute::PATH, (relative_path / "").string());

  try {
    session.write(flow_file, utils::FileReaderCallback{file_path, buffer_size_});
    session.transfer(flow_file, Success);
    if (!request_.keepSourceFile) {
      std::error_code remove_error;
      if (!std::filesystem::remove(file_path, remove_error)) {
        logger_->log_error("GetFile could not delete file '{}', error: {}", file_path, remove_error.message());
      }
    }
  } catch (const utils::FileReaderCallbackIOError& io_error) {
    logger_->log_error("IO error while processing file '{}': {}", file_path, io_error.what());
    flow_file->setDeleted(true);
  }
}

bool GetFile::isListingEmpty() const {
  std::lock_guard<std::mutex> lock(directory_listing_mutex_);

  return directory_listing_.empty();
}

void GetFile::putListing(const std::filesystem::path& file_path) {
  logger_->log_trace("Adding file to queue: {}", file_path);

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
  logger_->log_trace("Checking file: {}", full_name);

  std::error_code ec;
  uint64_t file_size = std::filesystem::file_size(full_name, ec);
  if (ec) {
    logger_->log_error("file_size of {}: {}", full_name, ec.message());
    return false;
  }
  const auto modifiedTime = std::filesystem::last_write_time(full_name, ec);
  if (ec) {
    logger_->log_error("last_write_time of {}: {}", full_name, ec.message());
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

  auto* const getfile_metrics = dynamic_cast<GetFileMetrics*>(metrics_.get());
  gsl_Assert(getfile_metrics);
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
    return true;
  };
  utils::file::list_dir(request.inputDirectory, callback, logger_, request.recursive);
}

REGISTER_RESOURCE(GetFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
