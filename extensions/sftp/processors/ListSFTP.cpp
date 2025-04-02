/**
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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "core/FlowFile.h"
#include "core/ProcessContext.h"
#include "core/Resource.h"
#include "io/BufferStream.h"
#include "rapidjson/ostreamwrapper.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"
#include "utils/file/FileUtils.h"
#include "utils/ProcessorConfigUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

namespace {
uint64_t toUnixTime(const std::optional<std::chrono::system_clock::time_point> time_point) {
  if (!time_point)
    return 0;
  return std::chrono::duration_cast<std::chrono::milliseconds>(time_point->time_since_epoch()).count();
}

std::optional<std::chrono::system_clock::time_point> fromUnixTime(const uint64_t timestamp) {
  if (timestamp == 0)
    return std::nullopt;
  return std::chrono::system_clock::time_point{std::chrono::milliseconds{timestamp}};
}

}  // namespace

void ListSFTP::initialize() {
  logger_->log_trace("Initializing FetchSFTP");

  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

ListSFTP::ListSFTP(std::string_view name, const utils::Identifier& uuid /*= utils::Identifier()*/)
    : SFTPProcessorBase(name, uuid) {
  logger_ = core::logging::LoggerFactory<ListSFTP>::getLogger(uuid_);
}

ListSFTP::~ListSFTP() = default;

void ListSFTP::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  parseCommonPropertiesOnSchedule(context);

  state_manager_ = context.getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  listing_strategy_ = utils::parseProperty(context, ListingStrategy);
  if (!last_listing_strategy_.empty() && last_listing_strategy_ != listing_strategy_) {
    invalidateCache();
  }
  last_listing_strategy_ = listing_strategy_;

  search_recursively_ = utils::parseBoolProperty(context, SearchRecursively);
  follow_symlink_ = utils::parseBoolProperty(context, FollowSymlink);
  ignore_dotted_files_ = utils::parseBoolProperty(context, IgnoreDottedFiles);

  file_filter_regex_ = context.getProperty(FileFilterRegex).value_or("");
  if (!file_filter_regex_.empty()) {
    try {
      compiled_file_filter_regex_ = utils::Regex(file_filter_regex_);
    } catch (const Exception&) {
      logger_->log_error("Failed to compile File Filter Regex \"{}\"", file_filter_regex_.c_str());
    }
  }

  path_filter_regex_ = context.getProperty(PathFilterRegex).value_or("");
  if (!path_filter_regex_.empty()) {
    try {
      compiled_path_filter_regex_ = utils::Regex(path_filter_regex_);
    } catch (const Exception&) {
      logger_->log_error("Failed to compile Path Filter Regex \"{}\"", path_filter_regex_.c_str());
    }
  }


  target_system_timestamp_precision_ = utils::parseProperty(context, TargetSystemTimestampPrecision);
  entity_tracking_initial_listing_target_ = utils::parseProperty(context, EntityTrackingInitialListingTarget);

  minimum_file_age_ = utils::parseDurationProperty(context, MinimumFileAge);
  maximum_file_age_ = utils::parseOptionalDurationProperty(context, MaximumFileAge);

  minimum_file_size_ = utils::parseDataSizeProperty(context, MinimumFileSize);
  maximum_file_size_ = utils::parseOptionalDataSizeProperty(context, MaximumFileSize);

  startKeepaliveThreadIfNeeded();
}

void ListSFTP::invalidateCache() {
  logger_->log_warn("Important properties have been reconfigured, invalidating in-memory cache");

  already_loaded_from_cache_ = false;

  last_run_time_ = std::chrono::steady_clock::time_point();
  last_listed_latest_entry_timestamp_.reset();
  last_processed_latest_entry_timestamp_.reset();
  latest_identifiers_processed_.clear();

  initial_listing_complete_ = false;
  already_listed_entities_.clear();
}

ListSFTP::Child::Child(const std::string& parent_path_, std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>&& sftp_child)
    : parent_path(parent_path_) {
  std::tie(filename, std::ignore, attrs) = std::move(sftp_child);
  directory = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
}

std::string ListSFTP::Child::getPath() const {
  return (parent_path / filename).generic_string();
}

bool ListSFTP::filter(const std::string& parent_path, const std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>& sftp_child) {
  const std::string& filename = std::get<0>(sftp_child);
  const LIBSSH2_SFTP_ATTRIBUTES& attrs = std::get<2>(sftp_child);
  /* This should not happen */
  if (filename.empty()) {
    logger_->log_error("Listing directory \"{}\" returned an empty child", parent_path.c_str());
    return false;
  }
  /* Ignore current dir and parent dir */
  if (filename == "." || filename == "..") {
    return false;
  }
  /* Dotted files */
  if (ignore_dotted_files_ && filename[0] == '.') {
    logger_->log_debug("Ignoring \"{}/{}\" because Ignore Dotted Files is true", parent_path.c_str(), filename.c_str());
    return false;
  }
  if (!(attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)) {
    // TODO(Bakai): maybe do a fallback stat here
    logger_->log_error("Failed to get permissions in stat for \"{}/{}\"", parent_path.c_str(), filename.c_str());
    return false;
  }
  if (LIBSSH2_SFTP_S_ISREG(attrs.permissions)) {
    return filterFile(parent_path, filename, attrs);
  } else if (LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
    return filterDirectory(parent_path, filename, attrs);
  } else {
    logger_->log_debug("Skipping non-regular, non-directory file \"{}/{}\"", parent_path.c_str(), filename.c_str());
    return false;
  }
}

bool ListSFTP::filterFile(const std::string& parent_path, const std::string& filename, const LIBSSH2_SFTP_ATTRIBUTES& attrs) {
  if (!(attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) ||
      !(attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) ||
      !(attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME)) {
    // TODO(Bakai): maybe do a fallback stat here
    logger_->log_error("Failed to get all attributes in stat for \"{}/{}\"", parent_path.c_str(), filename.c_str());
    return false;
  }

  /* Age */
  auto file_age = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - std::chrono::system_clock::from_time_t(gsl::narrow<time_t>(attrs.mtime)));
  if (file_age < minimum_file_age_) {
    logger_->log_debug("Ignoring \"{}/{}\" because it is younger than the Minimum File Age: {} < {}",
        parent_path.c_str(),
        filename.c_str(),
        file_age,
        minimum_file_age_);
    return false;
  }
  if (maximum_file_age_ && file_age > *maximum_file_age_) {
    logger_->log_debug("Ignoring \"{}/{}\" because it is older than the Maximum File Age: {} > {}",
                       parent_path.c_str(),
                       filename.c_str(),
                       file_age,
                       *maximum_file_age_);
    return false;
  }

  /* Size */
  if (attrs.filesize < minimum_file_size_) {
    logger_->log_debug("Ignoring \"{}/{}\" because it is smaller than the Minimum File Size: {} B < {} B",
                       parent_path.c_str(),
                       filename.c_str(),
                       attrs.filesize,
                       minimum_file_size_);
    return false;
  }
  if (maximum_file_size_ && attrs.filesize > *maximum_file_size_) {
    logger_->log_debug("Ignoring \"{}/{}\" because it is larger than the Maximum File Size: {} B > {} B",
                       parent_path.c_str(),
                       filename.c_str(),
                       attrs.filesize,
                       *maximum_file_size_);
    return false;
  }

  /* File Filter Regex */
  if (compiled_file_filter_regex_) {
    bool match = false;
    match = utils::regexMatch(filename, *compiled_file_filter_regex_);
    if (!match) {
      logger_->log_debug(R"(Ignoring "{}/{}" because it did not match the File Filter Regex "{}")",
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
  if (compiled_path_filter_regex_) {
    std::string dir_path = utils::string::join_pack(parent_path, "/", filename);
    bool match = false;
    match = utils::regexMatch(dir_path, *compiled_path_filter_regex_);
    if (!match) {
      logger_->log_debug(R"(Not recursing into "{}" because it did not match the Path Filter Regex "{}")",
                         dir_path.c_str(),
                         path_filter_regex_);
      return false;
    }
  }

  return true;
}

bool ListSFTP::createAndTransferFlowFileFromChild(
    core::ProcessSession& session,
    const std::string& hostname,
    uint16_t port,
    const std::string& username,
    const ListSFTP::Child& child) {
  /* Convert mtime to string */
  if (child.attrs.mtime > gsl::narrow<uint64_t>(std::numeric_limits<int64_t>::max())) {
    logger_->log_error("Modification date {} of \"{}/{}\" larger than int64_t max", child.attrs.mtime, child.parent_path.string(), child.filename);
    return true;
  }
  auto mtime_str = utils::timeutils::getDateTimeStr(date::sys_seconds{std::chrono::seconds(child.attrs.mtime)});

  /* Create FlowFile */
  auto flow_file = session.create();
  if (flow_file == nullptr) {
    logger_->log_error("Failed to create FlowFileRecord");
    return false;
  }

  /* Set attributes */
  session.putAttribute(*flow_file, ATTRIBUTE_SFTP_REMOTE_HOST, hostname);
  session.putAttribute(*flow_file, ATTRIBUTE_SFTP_REMOTE_PORT, std::to_string(port));
  session.putAttribute(*flow_file, ATTRIBUTE_SFTP_LISTING_USER, username);

  /* uid and gid */
  session.putAttribute(*flow_file, ATTRIBUTE_FILE_OWNER, std::to_string(child.attrs.uid));
  session.putAttribute(*flow_file, ATTRIBUTE_FILE_GROUP, std::to_string(child.attrs.gid));

  /* permissions */
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(4) << std::oct << (child.attrs.permissions & 0777);
  session.putAttribute(*flow_file, ATTRIBUTE_FILE_PERMISSIONS, ss.str());

  /* filesize */
  session.putAttribute(*flow_file, ATTRIBUTE_FILE_SIZE, std::to_string(child.attrs.filesize));

  /* mtime */
  session.putAttribute(*flow_file, ATTRIBUTE_FILE_LASTMODIFIEDTIME, mtime_str);

  flow_file->setAttribute(core::SpecialFlowAttribute::FILENAME, child.filename.generic_string());
  flow_file->setAttribute(core::SpecialFlowAttribute::PATH, child.parent_path.generic_string());

  session.transfer(flow_file, Success);

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

bool ListSFTP::persistTrackingTimestampsCache(core::ProcessContext& /*context*/, const std::string& hostname, const std::string& username, const std::string& remote_path) {
  std::unordered_map<std::string, std::string> state;
  state["listing_strategy"] = LISTING_STRATEGY_TRACKING_TIMESTAMPS;
  state["hostname"] = hostname;
  state["username"] = username;
  state["remote_path"] = remote_path;
  state["listing.timestamp"] = std::to_string(toUnixTime(last_listed_latest_entry_timestamp_));
  state["processed.timestamp"] = std::to_string(toUnixTime(last_processed_latest_entry_timestamp_));
  size_t i = 0;
  for (const auto& identifier : latest_identifiers_processed_) {
    state["id." + std::to_string(i)] = identifier;
    ++i;
  }
  return state_manager_->set(state);
}

bool ListSFTP::updateFromTrackingTimestampsCache(core::ProcessContext& /*context*/, const std::string& hostname, const std::string& username, const std::string& remote_path) {
  std::string state_listing_strategy;
  std::string state_hostname;
  std::string state_username;
  std::string state_remote_path;
  uint64_t state_listing_timestamp = 0;
  uint64_t state_processed_timestamp = 0;
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
        "Listing Strategy: \"{}\" vs. \"{}\", "
        "Hostname: \"{}\" vs. \"{}\", "
        "Username: \"{}\" vs. \"{}\", "
        "Remote Path: \"{}\" vs. \"{}\"",
        state_listing_strategy, listing_strategy_,
        state_hostname, hostname,
        state_username, username,
        state_remote_path, remote_path);
    return false;
  }

  last_listed_latest_entry_timestamp_ = fromUnixTime(state_listing_timestamp);
  last_processed_latest_entry_timestamp_ = fromUnixTime(state_processed_timestamp);
  latest_identifiers_processed_ = std::move(state_ids);

  return true;
}

void ListSFTP::listByTrackingTimestamps(
    core::ProcessContext& context,
    core::ProcessSession& session,
    const std::string& hostname,
    uint16_t port,
    const std::string& username,
    const std::string& remote_path,
    std::vector<Child>&& files) {
  auto min_timestamp_to_list = last_listed_latest_entry_timestamp_;

  /* Load state from cache file if needed */
  if (!already_loaded_from_cache_) {
    if (updateFromTrackingTimestampsCache(context, hostname, username, remote_path)) {
      logger_->log_debug("Successfully loaded state");
    } else {
      logger_->log_debug("Failed to load state");
    }
    already_loaded_from_cache_ = true;
  }

  std::chrono::steady_clock::time_point current_run_time = std::chrono::steady_clock::now();
  auto now = std::chrono::system_clock::now();

  /* Order children by timestamp and try to detect timestamp precision if needed  */
  std::map<std::chrono::system_clock::time_point, std::list<Child>> ordered_files;
  bool target_system_has_seconds = false;
  for (auto&& file : files) {
    std::chrono::system_clock::time_point timestamp{std::chrono::seconds(file.attrs.mtime)};
    target_system_has_seconds |= std::chrono::round<std::chrono::minutes>(timestamp) != timestamp;

    bool new_file = !min_timestamp_to_list.has_value() || (timestamp >= min_timestamp_to_list && timestamp >= last_processed_latest_entry_timestamp_);
    if (new_file) {
      auto& files_for_timestamp = ordered_files[timestamp];
      files_for_timestamp.emplace_back(std::move(file));
    } else {
      logger_->log_trace("Skipping \"{}\", because it is not new.", file.getPath().c_str());
    }
  }

  std::optional<std::chrono::system_clock::time_point> latest_listed_entry_timestamp_this_cycle;
  size_t flow_files_created = 0U;
  if (!ordered_files.empty()) {
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
    std::chrono::milliseconds listing_lag{utils::at(LISTING_LAG_MAP, remote_system_timestamp_precision)};
    logger_->log_debug("The listing lag is {}", listing_lag);

    /* If the latest listing time is equal to the last listing time, there are no entries with a newer timestamp than previously seen */
    if (latest_listed_entry_timestamp_this_cycle == last_listed_latest_entry_timestamp_ && latest_listed_entry_timestamp_this_cycle) {
      const auto& latest_files = ordered_files.at(*latest_listed_entry_timestamp_this_cycle);
      auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_run_time - last_run_time_);
      /* If a precision-specific listing lag has not yet elapsed since out last execution, we wait. */
      if (elapsed_time < listing_lag) {
        logger_->log_debug("The latest listed entry timestamp is the same as the last listed entry timestamp ({}) "
                           "and the listing lag has not yet elapsed ({} < {}). Yielding.",
                           latest_listed_entry_timestamp_this_cycle, elapsed_time, listing_lag);
        context.yield();
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
        logger_->log_debug("The latest listed entry timestamp is the same as the last listed entry timestamp ({}) "
                           "and all files for that timestamp has been processed. Yielding.", latest_listed_entry_timestamp_this_cycle);
        context.yield();
        return;
      }
    } else {
      /* Determine the minimum reliable timestamp based on precision */
      auto minimum_reliable_timestamp = now - listing_lag;
      if (remote_system_timestamp_precision == TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS) {
        minimum_reliable_timestamp = std::chrono::floor<std::chrono::seconds>(minimum_reliable_timestamp);
      } else {
        minimum_reliable_timestamp = std::chrono::floor<std::chrono::minutes>(minimum_reliable_timestamp);
      }
      /* If the latest timestamp is not old enough, we wait another cycle */
      if (latest_listed_entry_timestamp_this_cycle && minimum_reliable_timestamp < latest_listed_entry_timestamp_this_cycle) {
        logger_->log_debug("Skipping files with latest timestamp because their modification date is not smaller than the minimum reliable timestamp: {} >= {}",
                           latest_listed_entry_timestamp_this_cycle,
                           minimum_reliable_timestamp);
        ordered_files.erase(*latest_listed_entry_timestamp_this_cycle);
      }
    }

    for (auto& files_for_timestamp : ordered_files) {
      if (files_for_timestamp.first == last_processed_latest_entry_timestamp_) {
        /* Filter out previously processed entities. */
        for (auto it = files_for_timestamp.second.begin(); it != files_for_timestamp.second.end();) {
          if (latest_identifiers_processed_.contains(it->getPath())) {
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
          logger_->log_error("Failed to emit FlowFile for \"{}\"", file.filename.generic_string());
          context.yield();
          return;
        }
      }
    }
  }

  /* If we have a listing timestamp, it is worth persisting the state */
  if (latest_listed_entry_timestamp_this_cycle) {
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
    context.yield();
    return;
  }
}

bool ListSFTP::persistTrackingEntitiesCache(core::ProcessContext& /*context*/, const std::string& hostname, const std::string& username, const std::string& remote_path) {
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

bool ListSFTP::updateFromTrackingEntitiesCache(core::ProcessContext& /*context*/, const std::string& hostname, const std::string& username, const std::string& remote_path) {
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
      logger_->log_error("State for entity \"{}\" is missing or invalid, skipping", name);
      continue;
    }
  }

  if (state_listing_strategy != listing_strategy_ ||
      state_hostname != hostname ||
      state_username != username ||
      state_remote_path != remote_path) {
    logger_->log_error(
        "Processor state was persisted with different settings than the current ones, ignoring. "
        "Listing Strategy: \"{}\" vs. \"{}\", "
        "Hostname: \"{}\" vs. \"{}\", "
        "Username: \"{}\" vs. \"{}\", "
        "Remote Path: \"{}\" vs. \"{}\"",
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
    core::ProcessContext& context,
    core::ProcessSession& session,
    const std::string& hostname,
    uint16_t port,
    const std::string& username,
    const std::string& remote_path,
    std::chrono::milliseconds entity_tracking_time_window,
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
      ? 0U : (now * 1000 - entity_tracking_time_window.count());

  /* Skip files not in the tracking window */
  for (auto it = files.begin(); it != files.end(); ) {
    if (it->attrs.mtime * 1000 < min_timestamp_to_list) {
      logger_->log_trace("Skipping \"{}\" because it has an older timestamp than the minimum timestamp to list: {} < {}",
          it->getPath(), it->attrs.mtime * 1000, min_timestamp_to_list);
      it = files.erase(it);
    } else {
      ++it;
    }
  }

  if (files.empty()) {
    logger_->log_debug("No entities to list within the tracking time window");
    context.yield();
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
       logger_->log_trace("Found new file \"{}\"", child.getPath());
       return true;
     }

     if (child.attrs.mtime * 1000 > already_listed_it->second.timestamp) {
       logger_->log_trace("Found file \"{}\" with newer timestamp: {} -> {}",
           child.getPath(),
           already_listed_it->second.timestamp,
           child.attrs.mtime * 1000);
       return true;
     }

     if (child.attrs.filesize != already_listed_it->second.size) {
       logger_->log_trace("Found file \"{}\" with different size: {} -> {}",
                          child.getPath(),
                          already_listed_it->second.size,
                          child.attrs.filesize);
       return true;
     }

     logger_->log_trace("Skipping file \"{}\" because it has not changed", child.getPath());
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
    context.yield();
    return;
  }

  /* Remove expired entities */
  for (const auto& old_entity_id : old_entity_ids) {
    already_listed_entities_.erase(old_entity_id);
  }

  for (const auto& updated_entity : updated_entities) {
    /* Create the FlowFile for this path */
    if (!createAndTransferFlowFileFromChild(session, hostname, port, username, updated_entity)) {
      logger_->log_error("Failed to emit FlowFile for \"{}\"", updated_entity.getPath());
      context.yield();
      return;
    }
    already_listed_entities_[updated_entity.getPath()] = ListedEntity(updated_entity.attrs.mtime * 1000, updated_entity.attrs.filesize);
  }

  initial_listing_complete_ = true;

  persistTrackingEntitiesCache(context, hostname, username, remote_path);
}

void ListSFTP::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  /* Parse common properties */
  SFTPProcessorBase::CommonProperties common_properties;
  if (!parseCommonPropertiesOnTrigger(context, nullptr /*flow_file*/, common_properties)) {
    context.yield();
    return;
  }

  std::string remote_path_str = context.getProperty(RemotePath).value_or("");
  /* Remove trailing slashes */
  while (remote_path_str.size() > 1 && remote_path_str.ends_with('/')) {
    remote_path_str.pop_back();
  }
  std::filesystem::path remote_path{remote_path_str, std::filesystem::path::format::generic_format};

  std::chrono::milliseconds entity_tracking_time_window = 3h;  /* The default is 3 hours */
  if (const auto entity_tracking_time_window_str = context.getProperty(EntityTrackingTimeWindow)) {
    if (auto parsed_entity_time_window = utils::timeutils::StringToDuration<std::chrono::milliseconds>(*entity_tracking_time_window_str)) {
      entity_tracking_time_window = parsed_entity_time_window.value();
    } else {
      logger_->log_error("Entity Tracking Time Window attribute is invalid");
    }
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
    context.yield();
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
  root.parent_path = remote_path.parent_path();
  root.filename = remote_path.filename();
  root.directory = true;
  directories.emplace_back(std::move(root));

  /* Process directories */
  while (!directories.empty()) {
    auto directory = std::move(directories.front());
    directories.pop_front();

    std::string new_parent_path;
    if (directory.parent_path.empty()) {
      new_parent_path = directory.filename.generic_string();
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
    listByTrackingTimestamps(context, session, common_properties.hostname, common_properties.port, common_properties.username, remote_path.generic_string(), std::move(files));
  } else if (listing_strategy_ == LISTING_STRATEGY_TRACKING_ENTITIES) {
    listByTrackingEntities(context, session, common_properties.hostname, common_properties.port,
        common_properties.username, remote_path.generic_string(), entity_tracking_time_window, std::move(files));
  } else {
    logger_->log_error("Unknown Listing Strategy: \"{}\"", listing_strategy_.c_str());
    context.yield();
    return;
  }

  put_connection_back_to_cache();
}

REGISTER_RESOURCE(ListSFTP, Processor);

}  // namespace org::apache::nifi::minifi::processors
