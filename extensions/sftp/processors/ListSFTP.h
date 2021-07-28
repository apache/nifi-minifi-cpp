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
#pragma once

#include <memory>
#include <string>
#include <map>
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <set>
#include <tuple>
#include <vector>
#include <regex>

#include "SFTPProcessorBase.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "utils/Id.h"
#include "controllers/keyvalue/PersistableKeyValueStoreService.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class ListSFTP : public SFTPProcessorBase {
 public:
  static constexpr char const *LISTING_STRATEGY_TRACKING_TIMESTAMPS = "Tracking Timestamps";
  static constexpr char const *LISTING_STRATEGY_TRACKING_ENTITIES = "Tracking Entities";

  static constexpr char const *TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT = "Auto Detect";
  static constexpr char const *TARGET_SYSTEM_TIMESTAMP_PRECISION_MILLISECONDS = "Milliseconds";
  static constexpr char const *TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS = "Seconds";
  static constexpr char const *TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES = "Minutes";

  static constexpr char const *ENTITY_TRACKING_INITIAL_LISTING_TARGET_TRACKING_TIME_WINDOW = "Tracking Time Window";
  static constexpr char const *ENTITY_TRACKING_INITIAL_LISTING_TARGET_ALL_AVAILABLE = "All Available";

  static constexpr char const* ProcessorName = "ListSFTP";


  /*!
   * Create a new processor
   */
  explicit ListSFTP(const std::string& name, const utils::Identifier& uuid = {});
  virtual ~ListSFTP();

  // Supported Properties
  static core::Property ListingStrategy;
  static core::Property RemotePath;
  static core::Property SearchRecursively;
  static core::Property FollowSymlink;
  static core::Property FileFilterRegex;
  static core::Property PathFilterRegex;
  static core::Property IgnoreDottedFiles;
  static core::Property TargetSystemTimestampPrecision;
  static core::Property EntityTrackingTimeWindow;
  static core::Property EntityTrackingInitialListingTarget;
  static core::Property MinimumFileAge;
  static core::Property MaximumFileAge;
  static core::Property MinimumFileSize;
  static core::Property MaximumFileSize;

  // Supported Relationships
  static core::Relationship Success;

  // Writes Attributes
  static constexpr char const* ATTRIBUTE_SFTP_REMOTE_HOST = "sftp.remote.host";
  static constexpr char const* ATTRIBUTE_SFTP_REMOTE_PORT = "sftp.remote.port";
  static constexpr char const* ATTRIBUTE_SFTP_LISTING_USER = "sftp.listing.user";
  static constexpr char const* ATTRIBUTE_FILE_OWNER = "file.owner";
  static constexpr char const* ATTRIBUTE_FILE_GROUP = "file.group";
  static constexpr char const* ATTRIBUTE_FILE_PERMISSIONS = "file.permissions";
  static constexpr char const* ATTRIBUTE_FILE_SIZE = "file.size";
  static constexpr char const* ATTRIBUTE_FILE_LASTMODIFIEDTIME = "file.lastModifiedTime";

  static const std::map<std::string, uint64_t> LISTING_LAG_MAP;

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }


  std::shared_ptr<core::CoreComponentStateManager> state_manager_;
  std::string listing_strategy_;
  bool search_recursively_;
  bool follow_symlink_;
  std::string file_filter_regex_;
  std::string path_filter_regex_;
  bool file_filter_regex_set_;
  bool path_filter_regex_set_;
  std::regex compiled_file_filter_regex_;
  std::regex compiled_path_filter_regex_;
  bool ignore_dotted_files_;
  std::string target_system_timestamp_precision_;
  std::string entity_tracking_initial_listing_target_;
  uint64_t minimum_file_age_;
  uint64_t maximum_file_age_;
  uint64_t minimum_file_size_;
  uint64_t maximum_file_size_;

  std::string last_listing_strategy_;
  std::string last_hostname_;
  std::string last_username_;
  std::string last_remote_path_;

  struct Child {
    Child();
    Child(const std::string& parent_path_, std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>&& sftp_child);
    std::string getPath() const;

    bool directory;
    std::string parent_path;
    std::string filename;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
  };

  bool already_loaded_from_cache_;

  std::chrono::time_point<std::chrono::steady_clock> last_run_time_;
  uint64_t last_listed_latest_entry_timestamp_;
  uint64_t last_processed_latest_entry_timestamp_;
  std::set<std::string> latest_identifiers_processed_;

  bool initial_listing_complete_;
  struct ListedEntity {
    uint64_t timestamp;
    uint64_t size;

    ListedEntity();
    ListedEntity(uint64_t timestamp, uint64_t size);
  };
  std::unordered_map<std::string, ListedEntity> already_listed_entities_;

  void invalidateCache();

  bool filter(const std::string& parent_path, const std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>& sftp_child);
  bool filterFile(const std::string& parent_path, const std::string& filename, const LIBSSH2_SFTP_ATTRIBUTES& attrs);
  bool filterDirectory(const std::string& parent_path, const std::string& filename, const LIBSSH2_SFTP_ATTRIBUTES& attrs);

  bool createAndTransferFlowFileFromChild(
      const std::shared_ptr<core::ProcessSession>& session,
      const std::string& hostname,
      uint16_t port,
      const std::string& username,
      const Child& child);

  bool persistTrackingTimestampsCache(const std::shared_ptr<core::ProcessContext>& context, const std::string& hostname, const std::string& username, const std::string& remote_path);
  bool updateFromTrackingTimestampsCache(const std::shared_ptr<core::ProcessContext>& context, const std::string& hostname, const std::string& username, const std::string& remote_path);

  bool persistTrackingEntitiesCache(const std::shared_ptr<core::ProcessContext>& context, const std::string& hostname, const std::string& username, const std::string& remote_path);
  bool updateFromTrackingEntitiesCache(const std::shared_ptr<core::ProcessContext>& context, const std::string& hostname, const std::string& username, const std::string& remote_path);

  void listByTrackingTimestamps(
      const std::shared_ptr<core::ProcessContext>& context,
      const std::shared_ptr<core::ProcessSession>& session,
      const std::string& hostname,
      uint16_t port,
      const std::string& username,
      const std::string& remote_path,
      std::vector<Child>&& files);

  void listByTrackingEntities(
      const std::shared_ptr<core::ProcessContext>& context,
      const std::shared_ptr<core::ProcessSession>& session,
      const std::string& hostname,
      uint16_t port,
      const std::string& username,
      const std::string& remote_path,
      uint64_t entity_tracking_time_window,
      std::vector<Child>&& files);
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
