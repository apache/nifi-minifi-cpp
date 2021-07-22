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

#include "ListSFTP.h"

#include <memory>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <list>
#include <string>
#include <utility>
#include <vector>
#include <tuple>
#include <deque>
#include <regex>

#include "utils/ByteArrayCallback.h"
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "core/FlowFile.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "core/Resource.h"
#include "io/BufferStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"

#include "rapidjson/document.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/writer.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property ListSFTP::ListingStrategy(core::PropertyBuilder::createProperty("Listing Strategy")
    ->withDescription("Specify how to determine new/updated entities. See each strategy descriptions for detail.")
    ->isRequired(true)
    ->withAllowableValues<std::string>({LISTING_STRATEGY_TRACKING_TIMESTAMPS, LISTING_STRATEGY_TRACKING_ENTITIES})
    ->withDefaultValue(LISTING_STRATEGY_TRACKING_TIMESTAMPS)->build());

core::Property ListSFTP::RemotePath(core::PropertyBuilder::createProperty("Remote Path")
    ->withDescription("The fully qualified filename on the remote system")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

core::Property ListSFTP::SearchRecursively(core::PropertyBuilder::createProperty("Search Recursively")
    ->withDescription("If true, will pull files from arbitrarily nested subdirectories; "
                      "otherwise, will not traverse subdirectories")
    ->isRequired(true)->withDefaultValue<bool>(false)->build());

core::Property ListSFTP::FollowSymlink(core::PropertyBuilder::createProperty("Follow symlink")
    ->withDescription("If true, will pull even symbolic files and also nested symbolic subdirectories; "
                      "otherwise, will not read symbolic files and will not traverse symbolic link subdirectories")
    ->isRequired(true)->withDefaultValue<bool>(false)->build());

core::Property ListSFTP::FileFilterRegex(core::PropertyBuilder::createProperty("File Filter Regex")
    ->withDescription("Provides a Java Regular Expression for filtering Filenames; "
                      "if a filter is supplied, only files whose names match that Regular Expression will be fetched")
    ->isRequired(false)->build());

core::Property ListSFTP::PathFilterRegex(core::PropertyBuilder::createProperty("Path Filter Regex")
    ->withDescription("When Search Recursively is true, then only subdirectories whose path matches the given Regular Expression will be scanned")
    ->isRequired(false)->build());

core::Property ListSFTP::IgnoreDottedFiles(core::PropertyBuilder::createProperty("Ignore Dotted Files")
    ->withDescription("If true, files whose names begin with a dot (\".\") will be ignored")
    ->isRequired(true)->withDefaultValue<bool>(true)->build());

core::Property ListSFTP::TargetSystemTimestampPrecision(core::PropertyBuilder::createProperty("Target System Timestamp Precision")
    ->withDescription("Specify timestamp precision at the target system. "
                      "Since this processor uses timestamp of entities to decide which should be listed, "
                      "it is crucial to use the right timestamp precision.")
    ->isRequired(true)
    ->withAllowableValues<std::string>({TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT,
                                            TARGET_SYSTEM_TIMESTAMP_PRECISION_MILLISECONDS,
                                            TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS,
                                            TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES})
    ->withDefaultValue(TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT)->build());

core::Property ListSFTP::EntityTrackingTimeWindow(core::PropertyBuilder::createProperty("Entity Tracking Time Window")
    ->withDescription("Specify how long this processor should track already-listed entities. "
                      "'Tracking Entities' strategy can pick any entity whose timestamp is inside the specified time window. "
                      "For example, if set to '30 minutes', any entity having timestamp in recent 30 minutes will be the listing target when this processor runs. "
                      "A listed entity is considered 'new/updated' and a FlowFile is emitted if one of following condition meets: "
                      "1. does not exist in the already-listed entities, "
                      "2. has newer timestamp than the cached entity, "
                      "3. has different size than the cached entity. "
                      "If a cached entity's timestamp becomes older than specified time window, that entity will be removed from the cached already-listed entities. "
                      "Used by 'Tracking Entities' strategy.")
    ->isRequired(false)->build());

core::Property ListSFTP::EntityTrackingInitialListingTarget(core::PropertyBuilder::createProperty("Entity Tracking Initial Listing Target")
    ->withDescription("Specify how initial listing should be handled. Used by 'Tracking Entities' strategy.")
    ->withAllowableValues<std::string>({ENTITY_TRACKING_INITIAL_LISTING_TARGET_TRACKING_TIME_WINDOW,
                                            ENTITY_TRACKING_INITIAL_LISTING_TARGET_ALL_AVAILABLE})
    ->isRequired(false)->withDefaultValue(ENTITY_TRACKING_INITIAL_LISTING_TARGET_ALL_AVAILABLE)->build());

core::Property ListSFTP::MinimumFileAge(core::PropertyBuilder::createProperty("Minimum File Age")
    ->withDescription("The minimum age that a file must be in order to be pulled; "
                      "any file younger than this amount of time (according to last modification date) will be ignored")
    ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("0 sec")->build());

core::Property ListSFTP::MaximumFileAge(core::PropertyBuilder::createProperty("Maximum File Age")
    ->withDescription("The maximum age that a file must be in order to be pulled; "
                      "any file older than this amount of time (according to last modification date) will be ignored")
    ->isRequired(false)->build());

core::Property ListSFTP::MinimumFileSize(core::PropertyBuilder::createProperty("Minimum File Size")
    ->withDescription("The minimum size that a file must be in order to be pulled")
    ->isRequired(true)->withDefaultValue<core::DataSizeValue>("0 B")->build());

core::Property ListSFTP::MaximumFileSize(core::PropertyBuilder::createProperty("Maximum File Size")
    ->withDescription("The maximum size that a file must be in order to be pulled")
    ->isRequired(false)->build());

core::Relationship ListSFTP::Success("success", "All FlowFiles that are received are routed to success");

const std::map<std::string, uint64_t> ListSFTP::LISTING_LAG_MAP = {
  {ListSFTP::TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS, 1000},
  {ListSFTP::TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES, 60000},
};

constexpr char const* ListSFTP::LISTING_STRATEGY_TRACKING_TIMESTAMPS;
constexpr char const* ListSFTP::LISTING_STRATEGY_TRACKING_ENTITIES;

constexpr char const* ListSFTP::TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT;
constexpr char const* ListSFTP::TARGET_SYSTEM_TIMESTAMP_PRECISION_MILLISECONDS;
constexpr char const* ListSFTP::TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS;
constexpr char const* ListSFTP::TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES;

constexpr char const* ListSFTP::ENTITY_TRACKING_INITIAL_LISTING_TARGET_TRACKING_TIME_WINDOW;
constexpr char const* ListSFTP::ENTITY_TRACKING_INITIAL_LISTING_TARGET_ALL_AVAILABLE;

constexpr char const* ListSFTP::ProcessorName;

void ListSFTP::initialize() {
  logger_->log_trace("Initializing FetchSFTP");

  // Set the supported properties
  std::set<core::Property> properties;
  addSupportedCommonProperties(properties);
  properties.insert(ListingStrategy);
  properties.insert(RemotePath);
  properties.insert(SearchRecursively);
  properties.insert(FollowSymlink);
  properties.insert(FileFilterRegex);
  properties.insert(PathFilterRegex);
  properties.insert(IgnoreDottedFiles);
  properties.insert(TargetSystemTimestampPrecision);
  properties.insert(EntityTrackingTimeWindow);
  properties.insert(EntityTrackingInitialListingTarget);
  properties.insert(MinimumFileAge);
  properties.insert(MaximumFileAge);
  properties.insert(MinimumFileSize);
  properties.insert(MaximumFileSize);
  setSupportedProperties(properties);

  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

ListSFTP::ListSFTP(const std::string& name, const utils::Identifier& uuid /*= utils::Identifier()*/)
    : SFTPProcessorBase(name, uuid)
    , search_recursively_(false)
    , follow_symlink_(false)
    , file_filter_regex_set_(false)
    , path_filter_regex_set_(false)
    , ignore_dotted_files_(false)
    , minimum_file_age_(0U)
    , maximum_file_age_(0U)
    , minimum_file_size_(0U)
    , maximum_file_size_(0U)
    , already_loaded_from_cache_(false)
    , last_listed_latest_entry_timestamp_(0U)
    , last_processed_latest_entry_timestamp_(0U)
    , initial_listing_complete_(false) {
  logger_ = logging::LoggerFactory<ListSFTP>::getLogger();
}

ListSFTP::~ListSFTP() = default;

void ListSFTP::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  parseCommonPropertiesOnSchedule(context);

  state_manager_ = context->getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  std::string value;
  context->getProperty(ListingStrategy.getName(), listing_strategy_);
  if (!last_listing_strategy_.empty() && last_listing_strategy_ != listing_strategy_) {
    invalidateCache();
  }
  last_listing_strategy_ = listing_strategy_;
  if (!context->getProperty(SearchRecursively.getName(), value)) {
    logger_->log_error("Search Recursively attribute is missing or invalid");
  } else {
    search_recursively_ = utils::StringUtils::toBool(value).value_or(false);
  }
  if (!context->getProperty(FollowSymlink.getName(), value)) {
    logger_->log_error("Follow symlink attribute is missing or invalid");
  } else {
    follow_symlink_ = utils::StringUtils::toBool(value).value_or(false);
  }
  if (context->getProperty(FileFilterRegex.getName(), file_filter_regex_)) {
    try {
      compiled_file_filter_regex_ = std::regex(file_filter_regex_);
      file_filter_regex_set_ = true;
    } catch (const std::regex_error &e) {
      logger_->log_error("Failed to compile File Filter Regex \"%s\"", file_filter_regex_.c_str());
      file_filter_regex_set_ = false;
    }
  } else {
    file_filter_regex_set_ = false;
  }
  if (context->getProperty(PathFilterRegex.getName(), path_filter_regex_)) {
    try {
      compiled_path_filter_regex_ = std::regex(path_filter_regex_);
      path_filter_regex_set_ = true;
    } catch (const std::regex_error &e) {
      logger_->log_error("Failed to compile Path Filter Regex \"%s\"", path_filter_regex_.c_str());
      path_filter_regex_set_ = false;
    }
  } else {
    path_filter_regex_set_ = false;
  }
  if (!context->getProperty(IgnoreDottedFiles.getName(), value)) {
    logger_->log_error("Ignore Dotted Files attribute is missing or invalid");
  } else {
    ignore_dotted_files_ = utils::StringUtils::toBool(value).value_or(true);
  }
  context->getProperty(TargetSystemTimestampPrecision.getName(), target_system_timestamp_precision_);
  context->getProperty(EntityTrackingInitialListingTarget.getName(), entity_tracking_initial_listing_target_);
  if (!context->getProperty(MinimumFileAge.getName(), value)) {
    logger_->log_error("Minimum File Age attribute is missing or invalid");
  } else {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, minimum_file_age_, unit) || !core::Property::ConvertTimeUnitToMS(minimum_file_age_, unit, minimum_file_age_)) {
      logger_->log_error("Minimum File Age attribute is invalid");
    }
  }
  if (context->getProperty(MaximumFileAge.getName(), value)) {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, maximum_file_age_, unit) || !core::Property::ConvertTimeUnitToMS(maximum_file_age_, unit, maximum_file_age_)) {
      logger_->log_error("Maximum File Age attribute is invalid");
    }
  }
  if (!context->getProperty(MinimumFileSize.getName(), minimum_file_size_)) {
    logger_->log_error("Minimum File Size attribute is invalid");
  }
  if (context->getProperty(MaximumFileSize.getName(), value)) {
    if (!core::DataSizeValue::StringToInt(value, maximum_file_size_)) {
      logger_->log_error("Maximum File Size attribute is invalid");
    }
  }

  startKeepaliveThreadIfNeeded();
}

void ListSFTP::invalidateCache() {
  logger_->log_warn("Important properties have been reconfigured, invalidating in-memory cache");

  already_loaded_from_cache_ = false;

  last_run_time_ = std::chrono::time_point<std::chrono::steady_clock>();
  last_listed_latest_entry_timestamp_ = 0U;
  last_processed_latest_entry_timestamp_ = 0U;
  latest_identifiers_processed_.clear();

  initial_listing_complete_ = false;
  already_listed_entities_.clear();
}

ListSFTP::Child::Child()
    :directory(false) {
  memset(&attrs, 0x00, sizeof(attrs));
}

ListSFTP::Child::Child(const std::string& parent_path_, std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>&& sftp_child) {
  parent_path = parent_path_;
  std::tie(filename, std::ignore, attrs) = std::move(sftp_child);
  directory = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
}

std::string ListSFTP::Child::getPath() const {
  return utils::file::FileUtils::concat_path(parent_path, filename, true /*force_posix*/);
}

bool ListSFTP::filter(const std::string& parent_path, const std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>& sftp_child) {
  const std::string& filename = std::get<0>(sftp_child);
  const LIBSSH2_SFTP_ATTRIBUTES& attrs = std::get<2>(sftp_child);
  /* This should not happen */
  if (filename.empty()) {
    logger_->log_error("Listing directory \"%s\" returned an empty child", parent_path.c_str());
    return false;
  }
  /* Ignore current dir and parent dir */
  if (filename == "." || filename == "..") {
    return false;
  }
  /* Dotted files */
  if (ignore_dotted_files_ && filename[0] == '.') {
    logger_->log_debug("Ignoring \"%s/%s\" because Ignore Dotted Files is true", parent_path.c_str(), filename.c_str());
    return false;
  }
  if (!(attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)) {
    // TODO(Bakai): maybe do a fallback stat here
    logger_->log_error("Failed to get permissions in stat for \"%s/%s\"", parent_path.c_str(), filename.c_str());
    return false;
  }
  if (LIBSSH2_SFTP_S_ISREG(attrs.permissions)) {
    return filterFile(parent_path, filename, attrs);
  } else if (LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
    return filterDirectory(parent_path, filename, attrs);
  } else {
    logger_->log_debug("Skipping non-regular, non-directory file \"%s/%s\"", parent_path.c_str(), filename.c_str());
    return false;
  }
}

bool ListSFTP::filterFile(const std::string& parent_path, const std::string& filename, const LIBSSH2_SFTP_ATTRIBUTES& attrs) {
  if (!(attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) ||
      !(attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) ||
      !(attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME)) {
    // TODO(Bakai): maybe do a fallback stat here
    logger_->log_error("Failed to get all attributes in stat for \"%s/%s\"", parent_path.c_str(), filename.c_str());
    return false;
  }

  /* Age */
  time_t now = time(nullptr);
  uint64_t file_age = (now - attrs.mtime) * 1000;
  if (file_age < minimum_file_age_) {
    logger_->log_debug("Ignoring \"%s/%s\" because it is younger than the Minimum File Age: %ld ms < %lu ms",
        parent_path.c_str(),
        filename.c_str(),
        file_age,
        minimum_file_age_);
    return false;
  }
  if (maximum_file_age_ != 0U && file_age > maximum_file_age_) {
    logger_->log_debug("Ignoring \"%s/%s\" because it is older than the Maximum File Age: %ld ms > %lu ms",
                       parent_path.c_str(),
                       filename.c_str(),
                       file_age,
                       maximum_file_age_);
    return false;
  }

  /* Size */
  if (attrs.filesize < minimum_file_size_) {
    logger_->log_debug("Ignoring \"%s/%s\" because it is smaller than the Minimum File Size: %lu B < %lu B",
                       parent_path.c_str(),
                       filename.c_str(),
                       attrs.filesize,
                       minimum_file_size_);
    return false;
  }
  if (maximum_file_size_ != 0U && attrs.filesize > maximum_file_size_) {
    logger_->log_debug("Ignoring \"%s/%s\" because it is larger than the Maximum File Size: %lu B > %lu B",
                       parent_path.c_str(),
                       filename.c_str(),
                       attrs.filesize,
                       maximum_file_size_);
    return false;
  }

  /* File Filter Regex */
  if (file_filter_regex_set_) {
    bool match = false;
    match = std::regex_search(filename, compiled_file_filter_regex_);
    if (!match) {
      logger_->log_debug("Ignoring \"%s/%s\" because it did not match the File Filter Regex \"%s\"",
                         parent_path.c_str(),
                         filename.c_str(),
                         file_filter_regex_);
      return false;
    }
  }

  return true;
}

bool ListSFTP::filterDirectory(const std::string& parent_path, const std::string& filename, const LIBSSH2_SFTP_ATTRIBUTES& /*attrs*/) {
  if (!search_recursively_) {
    return false;
  }

  /* Path Filter Regex */
  if (path_filter_regex_set_) {
    std::string dir_path = utils::file::FileUtils::concat_path(parent_path, filename, true /*force_posix*/);
    bool match = false;
    match = std::regex_search(dir_path, compiled_path_filter_regex_);
    if (!match) {
      logger_->log_debug("Not recursing into \"%s\" because it did not match the Path Filter Regex \"%s\"",
                         dir_path.c_str(),
                         path_filter_regex_);
      return false;
    }
  }

  return true;
}

bool ListSFTP::createAndTransferFlowFileFromChild(
    const std::shared_ptr<core::ProcessSession>& session,
    const std::string& hostname,
    uint16_t port,
    const std::string& username,
    const ListSFTP::Child& child) {
  /* Convert mtime to string */
  if (child.attrs.mtime > gsl::narrow<uint64_t>(std::numeric_limits<int64_t>::max())) {
    logger_->log_error("Modification date %lu of \"%s/%s\" larger than int64_t max", child.attrs.mtime, child.parent_path.c_str(), child.filename.c_str());
    return true;
  }
  std::string mtime_str;
  if (!utils::timeutils::getDateTimeStr(gsl::narrow<int64_t>(child.attrs.mtime), mtime_str)) {
    logger_->log_error("Failed to convert modification date %lu of \"%s/%s\" to string", child.attrs.mtime, child.parent_path.c_str(), child.filename.c_str());
    return true;
  }

  /* Create FlowFile */
  auto flow_file = session->create();
  if (flow_file == nullptr) {
    logger_->log_error("Failed to create FlowFileRecord");
    return false;
  }

  /* Set attributes */
  session->putAttribute(flow_file, ATTRIBUTE_SFTP_REMOTE_HOST, hostname);
  session->putAttribute(flow_file, ATTRIBUTE_SFTP_REMOTE_PORT, std::to_string(port));
  session->putAttribute(flow_file, ATTRIBUTE_SFTP_LISTING_USER, username);

  /* uid and gid */
  session->putAttribute(flow_file, ATTRIBUTE_FILE_OWNER, std::to_string(child.attrs.uid));
  session->putAttribute(flow_file, ATTRIBUTE_FILE_GROUP, std::to_string(child.attrs.gid));

  /* permissions */
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(4) << std::oct << (child.attrs.permissions & 0777);
  session->putAttribute(flow_file, ATTRIBUTE_FILE_PERMISSIONS, ss.str());

  /* filesize */
  session->putAttribute(flow_file, ATTRIBUTE_FILE_SIZE, std::to_string(child.attrs.filesize));

  /* mtime */
  session->putAttribute(flow_file, ATTRIBUTE_FILE_LASTMODIFIEDTIME, mtime_str);

  flow_file->setAttribute(core::SpecialFlowAttribute::FILENAME, child.filename);
  flow_file->setAttribute(core::SpecialFlowAttribute::PATH, child.parent_path);

  session->transfer(flow_file, Success);

  return true;
}

ListSFTP::ListedEntity::ListedEntity()
    : timestamp(0U)
    , size(0U) {
}

ListSFTP::ListedEntity::ListedEntity(uint64_t timestamp_, uint64_t size_)
    : timestamp(timestamp_)
    , size(size_) {
}

bool ListSFTP::persistTrackingTimestampsCache(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::string& hostname, const std::string& username, const std::string& remote_path) {
  std::unordered_map<std::string, std::string> state;
  state["listing_strategy"] = LISTING_STRATEGY_TRACKING_TIMESTAMPS;
  state["hostname"] = hostname;
  state["username"] = username;
  state["remote_path"] = remote_path;
  state["listing.timestamp"] = std::to_string(last_listed_latest_entry_timestamp_);
  state["processed.timestamp"] = std::to_string(last_processed_latest_entry_timestamp_);
  size_t i = 0;
  for (const auto& identifier : latest_identifiers_processed_) {
    state["id." + std::to_string(i)] = identifier;
    ++i;
  }
  return state_manager_->set(state);
}

bool ListSFTP::updateFromTrackingTimestampsCache(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::string& hostname, const std::string& username, const std::string& remote_path) {
  std::string state_listing_strategy;
  std::string state_hostname;
  std::string state_username;
  std::string state_remote_path;
  uint64_t state_listing_timestamp;
  uint64_t state_processed_timestamp;
  std::set<std::string> state_ids;

  std::unordered_map<std::string, std::string> state_map;
  if (!state_manager_->get(state_map)) {
    logger_->log_info("Found no stored state");
    return false;
  }

  try {
    state_listing_strategy = state_map.at("listing_strategy");
  } catch (...) {
    logger_->log_error("listing_strategy is missing from state");
    return false;
  }
  try {
    state_hostname = state_map.at("hostname");
  } catch (...) {
    logger_->log_error("hostname is missing from state");
    return false;
  }
  try {
    state_username = state_map.at("username");
  } catch (...) {
    logger_->log_error("username is missing from state");
    return false;
  }
  try {
    state_remote_path = state_map.at("remote_path");
  } catch (...) {
    logger_->log_error("remote_path is missing from state");
    return false;
  }
  try {
    state_listing_timestamp = stoull(state_map.at("listing.timestamp"));
  } catch (...) {
    logger_->log_error("listing.timestamp is missing from state or is invalid");
    return false;
  }
  try {
    state_processed_timestamp = stoull(state_map.at("processed.timestamp"));
  } catch (...) {
    logger_->log_error("processed.timestamp is missing from state or is invalid");
    return false;
  }
  for (const auto &kv : state_map) {
    if (kv.first.compare(0, strlen("id."), "id.") == 0) {
      state_ids.emplace(kv.second);
    }
  }

  if (state_listing_strategy != listing_strategy_ ||
      state_hostname != hostname ||
      state_username != username ||
      state_remote_path != remote_path) {
    logger_->log_error(
        "Processor state was persisted with different settings than the current ones, ignoring. "
        "Listing Strategy: \"%s\" vs. \"%s\", "
        "Hostname: \"%s\" vs. \"%s\", "
        "Username: \"%s\" vs. \"%s\", "
        "Remote Path: \"%s\" vs. \"%s\"",
        state_listing_strategy, listing_strategy_,
        state_hostname, hostname,
        state_username, username,
        state_remote_path, remote_path);
    return false;
  }

  last_listed_latest_entry_timestamp_ = state_listing_timestamp;
  last_processed_latest_entry_timestamp_ = state_processed_timestamp;
  latest_identifiers_processed_ = std::move(state_ids);

  return true;
}

void ListSFTP::listByTrackingTimestamps(
    const std::shared_ptr<core::ProcessContext>& context,
    const std::shared_ptr<core::ProcessSession>& session,
    const std::string& hostname,
    uint16_t port,
    const std::string& username,
    const std::string& remote_path,
    std::vector<Child>&& files) {
  uint64_t min_timestamp_to_list = last_listed_latest_entry_timestamp_;

  /* Load state from cache file if needed */
  if (!already_loaded_from_cache_) {
    if (updateFromTrackingTimestampsCache(context, hostname, username, remote_path)) {
      logger_->log_debug("Successfully loaded state");
    } else {
      logger_->log_debug("Failed to load state");
    }
    already_loaded_from_cache_ = true;
  }

  std::chrono::time_point<std::chrono::steady_clock> current_run_time = std::chrono::steady_clock::now();
  time_t now = time(nullptr);

  /* Order children by timestamp and try to detect timestamp precision if needed  */
  std::map<uint64_t /*timestamp*/, std::list<Child>> ordered_files;
  bool target_system_has_seconds = false;
  for (auto&& file : files) {
    uint64_t timestamp = file.attrs.mtime * 1000;
    target_system_has_seconds |= timestamp % 60000 != 0;

    bool new_file = min_timestamp_to_list == 0U || (timestamp >= min_timestamp_to_list && timestamp >= last_processed_latest_entry_timestamp_);
    if (new_file) {
      auto& files_for_timestamp = ordered_files[timestamp];
      files_for_timestamp.emplace_back(std::move(file));
    } else {
      logger_->log_trace("Skipping \"%s\", because it is not new.", file.getPath().c_str());
    }
  }

  uint64_t latest_listed_entry_timestamp_this_cycle = 0U;
  size_t flow_files_created = 0U;
  if (ordered_files.size() > 0) {
    latest_listed_entry_timestamp_this_cycle = ordered_files.crbegin()->first;

    std::string remote_system_timestamp_precision;
    if (target_system_timestamp_precision_ == TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT) {
      if (target_system_has_seconds) {
        logger_->log_debug("Precision auto detection detected second precision");
        remote_system_timestamp_precision = TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS;
      } else {
        logger_->log_debug("Precision auto detection detected minute precision");
        remote_system_timestamp_precision = TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES;
      }
    } else if (target_system_timestamp_precision_ == TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES) {
        remote_system_timestamp_precision = TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES;
    } else {
      /*
       * We only have seconds-precision timestamps, TARGET_SYSTEM_TIMESTAMP_PRECISION_MILLISECONDS makes no real sense here,
       * so we will treat it as TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS.
       */
      remote_system_timestamp_precision = TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS;
    }
    uint64_t listing_lag = LISTING_LAG_MAP.at(remote_system_timestamp_precision);
    logger_->log_debug("The listing lag is %lu ms", listing_lag);

    /* If the latest listing time is equal to the last listing time, there are no entries with a newer timestamp than previously seen */
    if (latest_listed_entry_timestamp_this_cycle == last_listed_latest_entry_timestamp_) {
      const auto& latest_files = ordered_files.at(latest_listed_entry_timestamp_this_cycle);
      uint64_t elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_run_time - last_run_time_).count();
      /* If a precision-specific listing lag has not yet elapsed since out last execution, we wait. */
      if (elapsed_time < listing_lag) {
        logger_->log_debug("The latest listed entry timestamp is the same as the last listed entry timestamp (%lu) "
                           "and the listing lag has not yet elapsed (%lu ms < % lu ms). Yielding.",
                           latest_listed_entry_timestamp_this_cycle,
                           elapsed_time,
                           listing_lag);
        context->yield();
        return;
      }
      /*
       * If we have already processed the entities with the newest timestamp,
       * and there are no new entities with that timestamp, there is nothing to do.
       */
      if (latest_listed_entry_timestamp_this_cycle == last_processed_latest_entry_timestamp_ &&
          std::all_of(latest_files.begin(), latest_files.end(), [this](const Child& child) {
            return latest_identifiers_processed_.count(child.getPath()) == 1U;
          })) {
        logger_->log_debug("The latest listed entry timestamp is the same as the last listed entry timestamp (%lu) "
                           "and all files for that timestamp has been processed. Yielding.", latest_listed_entry_timestamp_this_cycle);
        context->yield();
        return;
      }
    } else {
      /* Determine the minimum reliable timestamp based on precision */
      uint64_t minimum_reliable_timestamp = now * 1000 - listing_lag;
      if (remote_system_timestamp_precision == TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS) {
        minimum_reliable_timestamp -= minimum_reliable_timestamp % 1000;
      } else {
        minimum_reliable_timestamp -= minimum_reliable_timestamp % 60000;
      }
      /* If the latest timestamp is not old enough, we wait another cycle */
      if (minimum_reliable_timestamp < latest_listed_entry_timestamp_this_cycle) {
        logger_->log_debug("Skipping files with latest timestamp because their modification date is not smaller than the minimum reliable timestamp: %lu ms >= %lu ms",
            latest_listed_entry_timestamp_this_cycle,
            minimum_reliable_timestamp);
        ordered_files.erase(latest_listed_entry_timestamp_this_cycle);
      }
    }

    for (auto& files_for_timestamp : ordered_files) {
      if (files_for_timestamp.first == last_processed_latest_entry_timestamp_) {
        /* Filter out previously processed entities. */
        for (auto it = files_for_timestamp.second.begin(); it != files_for_timestamp.second.end();) {
          if (latest_identifiers_processed_.count(it->getPath()) != 0U) {
            it = files_for_timestamp.second.erase(it);
          } else {
            ++it;
          }
        }
      }
      for (const auto& file : files_for_timestamp.second) {
        /* Create the FlowFile for this path */
        if (createAndTransferFlowFileFromChild(session, hostname, port, username, file)) {
          flow_files_created++;
        } else {
          logger_->log_error("Failed to emit FlowFile for \"%s\"", file.filename);
          context->yield();
          return;
        }
      }
    }
  }

  /* If we have a listing timestamp, it is worth persisting the state */
  if (latest_listed_entry_timestamp_this_cycle != 0U) {
    bool processed_new_files = flow_files_created > 0U;
    if (processed_new_files) {
      auto last_files_it = ordered_files.crbegin();
      if (last_files_it->first != last_processed_latest_entry_timestamp_) {
        latest_identifiers_processed_.clear();
      }

      for (const auto& last_file : last_files_it->second) {
        latest_identifiers_processed_.insert(last_file.getPath());
      }

      last_processed_latest_entry_timestamp_ = last_files_it->first;
    }

    last_run_time_ = current_run_time;

    if (latest_listed_entry_timestamp_this_cycle != last_listed_latest_entry_timestamp_ || processed_new_files) {
      last_listed_latest_entry_timestamp_ = latest_listed_entry_timestamp_this_cycle;
      persistTrackingTimestampsCache(context, hostname, username, remote_path);
    }
  } else {
    logger_->log_debug("There are no files to list. Yielding.");
    context->yield();
    return;
  }
}

bool ListSFTP::persistTrackingEntitiesCache(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::string& hostname, const std::string& username, const std::string& remote_path) {
  std::unordered_map<std::string, std::string> state;
  state["listing_strategy"] = listing_strategy_;
  state["hostname"] = hostname;
  state["username"] = username;
  state["remote_path"] = remote_path;
  size_t i = 0;
  for (const auto &already_listed_entity : already_listed_entities_) {
    state["entity." + std::to_string(i) + ".name"] = already_listed_entity.first;
    state["entity." + std::to_string(i) + ".timestamp"] = std::to_string(already_listed_entity.second.timestamp);
    state["entity." + std::to_string(i) + ".size"] = std::to_string(already_listed_entity.second.size);
    ++i;
  }
  return state_manager_->set(state);
}

bool ListSFTP::updateFromTrackingEntitiesCache(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::string& hostname, const std::string& username, const std::string& remote_path) {
  std::string state_listing_strategy;
  std::string state_hostname;
  std::string state_username;
  std::string state_remote_path;
  std::unordered_map<std::string, ListedEntity> new_already_listed_entities;

  std::unordered_map<std::string, std::string> state_map;
  if (!state_manager_->get(state_map)) {
    logger_->log_debug("Failed to get state from StateManager");
    return false;
  }

  try {
    state_listing_strategy = state_map.at("listing_strategy");
  } catch (...) {
    logger_->log_error("listing_strategy is missing from state");
    return false;
  }
  try {
    state_hostname = state_map.at("hostname");
  } catch (...) {
    logger_->log_error("hostname is missing from state");
    return false;
  }
  try {
    state_username = state_map.at("username");
  } catch (...) {
    logger_->log_error("username is missing from state");
    return false;
  }
  try {
    state_remote_path = state_map.at("remote_path");
  } catch (...) {
    logger_->log_error("remote_path is missing from state");
    return false;
  }

  for (size_t i = 0U;; i++) {
    std::string name;
    try {
      name = state_map.at("entity." + std::to_string(i) + ".name");
    } catch (...) {
      break;
    }
    try {
      uint64_t timestamp = std::stoull(state_map.at("entity." + std::to_string(i) + ".timestamp"));
      uint64_t size = std::stoull(state_map.at("entity." + std::to_string(i) + ".size"));
      new_already_listed_entities.emplace(std::piecewise_construct,
                                          std::forward_as_tuple(name),
                                          std::forward_as_tuple(timestamp, size));
    } catch (...) {
      logger_->log_error("State for entity \"%s\" is missing or invalid, skipping", name);
      continue;
    }
  }

  if (state_listing_strategy != listing_strategy_ ||
      state_hostname != hostname ||
      state_username != username ||
      state_remote_path != remote_path) {
    logger_->log_error(
        "Processor state was persisted with different settings than the current ones, ignoring. "
        "Listing Strategy: \"%s\" vs. \"%s\", "
        "Hostname: \"%s\" vs. \"%s\", "
        "Username: \"%s\" vs. \"%s\", "
        "Remote Path: \"%s\" vs. \"%s\"",
        state_listing_strategy, listing_strategy_,
        state_hostname, hostname,
        state_username, username,
        state_remote_path, remote_path);
    return false;
  }

  already_listed_entities_ = std::move(new_already_listed_entities);

  return true;
}

void ListSFTP::listByTrackingEntities(
    const std::shared_ptr<core::ProcessContext>& context,
    const std::shared_ptr<core::ProcessSession>& session,
    const std::string& hostname,
    uint16_t port,
    const std::string& username,
    const std::string& remote_path,
    uint64_t entity_tracking_time_window,
    std::vector<Child>&& files) {
  /* Load state from cache file if needed */
  if (!already_loaded_from_cache_) {
    if (updateFromTrackingEntitiesCache(context, hostname, username, remote_path)) {
      logger_->log_debug("Successfully loaded state");
      initial_listing_complete_ = true;
    } else {
      logger_->log_debug("Failed to load state");
    }
    already_loaded_from_cache_ = true;
  }

  time_t now = time(nullptr);
  uint64_t min_timestamp_to_list = (!initial_listing_complete_ && entity_tracking_initial_listing_target_ == ENTITY_TRACKING_INITIAL_LISTING_TARGET_ALL_AVAILABLE)
      ? 0U : (now * 1000 - entity_tracking_time_window);

  /* Skip files not in the tracking window */
  for (auto it = files.begin(); it != files.end(); ) {
    if (it->attrs.mtime * 1000 < min_timestamp_to_list) {
      logger_->log_trace("Skipping \"%s\" because it has an older timestamp than the minimum timestamp to list: %lu < %lu",
          it->getPath(), it->attrs.mtime * 1000, min_timestamp_to_list);
      it = files.erase(it);
    } else {
      ++it;
    }
  }

  if (files.empty()) {
    logger_->log_debug("No entities to list within the tracking time window");
    context->yield();
    return;
  }

  /* Find files that have been updated */
  std::vector<Child> updated_entities;
  std::copy_if(std::make_move_iterator(files.begin()),
               std::make_move_iterator(files.end()),
               std::back_inserter(updated_entities),
               [&](const Child& child) {
     auto already_listed_it = already_listed_entities_.find(child.getPath());
     if (already_listed_it == already_listed_entities_.end()) {
       logger_->log_trace("Found new file \"%s\"", child.getPath());
       return true;
     }

     if (child.attrs.mtime * 1000 > already_listed_it->second.timestamp) {
       logger_->log_trace("Found file \"%s\" with newer timestamp: %lu -> %lu",
           child.getPath(),
           already_listed_it->second.timestamp,
           child.attrs.mtime * 1000);
       return true;
     }

     if (child.attrs.filesize != already_listed_it->second.size) {
       logger_->log_trace("Found file \"%s\" with different size: %lu -> %lu",
                          child.getPath(),
                          already_listed_it->second.size,
                          child.attrs.filesize);
       return true;
     }

     logger_->log_trace("Skipping file \"%s\" because it has not changed", child.getPath());
     return false;
  });

  /* Find entities in the tracking cache that are no longer in the tracking window */
  std::vector<std::string> old_entity_ids;
  for (const auto& already_listed_entity : already_listed_entities_) {
    if (already_listed_entity.second.timestamp < min_timestamp_to_list) {
      old_entity_ids.emplace_back(already_listed_entity.first);
    }
  }

  /* If we have no new files and no expired tracked entities, we have nothing to do */
  if (updated_entities.empty() && old_entity_ids.empty()) {
    context->yield();
    return;
  }

  /* Remove expired entities */
  for (const auto& old_entity_id : old_entity_ids) {
    already_listed_entities_.erase(old_entity_id);
  }

  for (const auto& updated_entity : updated_entities) {
    /* Create the FlowFile for this path */
    if (!createAndTransferFlowFileFromChild(session, hostname, port, username, updated_entity)) {
      logger_->log_error("Failed to emit FlowFile for \"%s\"", updated_entity.getPath());
      context->yield();
      return;
    }
    already_listed_entities_[updated_entity.getPath()] = ListedEntity(updated_entity.attrs.mtime * 1000, updated_entity.attrs.filesize);
  }

  initial_listing_complete_ = true;

  persistTrackingEntitiesCache(context, hostname, username, remote_path);
}

void ListSFTP::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  /* Parse common properties */
  SFTPProcessorBase::CommonProperties common_properties;
  if (!parseCommonPropertiesOnTrigger(context, nullptr /*flow_file*/, common_properties)) {
    context->yield();
    return;
  }

  /* Parse processor-specific properties */
  std::string remote_path;
  uint64_t entity_tracking_time_window = 0U;

  std::string value;
  context->getProperty(RemotePath.getName(), remote_path);
  /* Remove trailing slashes */
  while (remote_path.size() > 1U && remote_path.back() == '/') {
    remote_path.resize(remote_path.size() - 1);
  }
  if (context->getProperty(EntityTrackingTimeWindow.getName(), value)) {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, entity_tracking_time_window, unit) ||
        !core::Property::ConvertTimeUnitToMS(entity_tracking_time_window, unit, entity_tracking_time_window)) {
      /* The default is 3 hours */
      entity_tracking_time_window = 3 * 3600 * 1000;
      logger_->log_error("Entity Tracking Time Window attribute is invalid");
    }
  } else {
    /* The default is 3 hours */
    entity_tracking_time_window = 3 * 3600 * 1000;
  }

  /* Check whether we need to invalidate the cache based on the new properties */
  if ((!last_hostname_.empty() && last_hostname_ != common_properties.hostname) ||
      (!last_username_.empty() && last_username_ != common_properties.username) ||
      (!last_remote_path_.empty() && last_remote_path_ != remote_path)) {
    invalidateCache();
  }
  last_hostname_ = common_properties.hostname;
  last_username_ = common_properties.username;
  last_remote_path_ = remote_path;

  /* Get SFTPClient from cache or create it */
  const SFTPProcessorBase::ConnectionCacheKey connection_cache_key = {common_properties.hostname,
                                                                      common_properties.port,
                                                                      common_properties.username,
                                                                      proxy_type_,
                                                                      common_properties.proxy_host,
                                                                      common_properties.proxy_port,
                                                                      common_properties.proxy_username};
  auto client = getOrCreateConnection(connection_cache_key,
                                      common_properties.password,
                                      common_properties.private_key_path,
                                      common_properties.private_key_passphrase,
                                      common_properties.proxy_password);
  if (client == nullptr) {
    context->yield();
    return;
  }

  /*
   * Unless we're sure that the connection is good, we don't want to put it back to the cache.
   * So we will only call this when we're sure that the connection is OK.
   */
  auto put_connection_back_to_cache = [this, &connection_cache_key, &client]() {
    addConnectionToCache(connection_cache_key, std::move(client));
  };

  std::deque<Child> directories;
  std::vector<Child> files;

  /* Add initial directory */
  Child root;
  std::tie(root.parent_path, root.filename) = utils::file::FileUtils::split_path(remote_path, true /*force_posix*/);
  root.directory = true;
  directories.emplace_back(std::move(root));

  /* Process directories */
  while (!directories.empty()) {
    auto directory = std::move(directories.front());
    directories.pop_front();

    std::string new_parent_path;
    if (directory.parent_path.empty()) {
      new_parent_path = directory.filename;
    } else {
      new_parent_path = directory.getPath();
    }
    std::vector<std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>> dir_children;
    if (!client->listDirectory(new_parent_path, follow_symlink_, dir_children)) {
      continue;
    }
    for (auto&& dir_child : dir_children) {
      if (filter(new_parent_path, dir_child)) {
        Child child(new_parent_path, std::move(dir_child));
        if (child.directory) {
          directories.emplace_back(std::move(child));
        } else {
          files.emplace_back(std::move(child));
        }
      }
    }
  }

  /* Process the files with the appropriate tracking strategy */
  if (listing_strategy_ == LISTING_STRATEGY_TRACKING_TIMESTAMPS) {
    listByTrackingTimestamps(context, session, common_properties.hostname, common_properties.port, common_properties.username, remote_path, std::move(files));
  } else if (listing_strategy_ == LISTING_STRATEGY_TRACKING_ENTITIES) {
    listByTrackingEntities(context, session, common_properties.hostname, common_properties.port, common_properties.username, remote_path, entity_tracking_time_window, std::move(files));
  } else {
    logger_->log_error("Unknown Listing Strategy: \"%s\"", listing_strategy_.c_str());
    context->yield();
    return;
  }

  put_connection_back_to_cache();
}

REGISTER_RESOURCE(ListSFTP, "Performs a listing of the files residing on an SFTP server. "
                            "For each file that is found on the remote server, a new FlowFile will be created with "
                            "the filename attribute set to the name of the file on the remote server. "
                            "This can then be used in conjunction with FetchSFTP in order to fetch those files.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
