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
#include <optional>

#include "SFTPProcessorBase.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "utils/ArrayUtils.h"
#include "utils/Id.h"
#include "controllers/keyvalue/KeyValueStateStorage.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

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

  explicit ListSFTP(std::string name, const utils::Identifier& uuid = {});
  ~ListSFTP() override;

  EXTENSIONAPI static constexpr const char* Description = "Performs a listing of the files residing on an SFTP server. "
      "For each file that is found on the remote server, a new FlowFile will be created with "
      "the filename attribute set to the name of the file on the remote server. "
      "This can then be used in conjunction with FetchSFTP in order to fetch those files.";

  EXTENSIONAPI static const core::Property ListingStrategy;
  EXTENSIONAPI static const core::Property RemotePath;
  EXTENSIONAPI static const core::Property SearchRecursively;
  EXTENSIONAPI static const core::Property FollowSymlink;
  EXTENSIONAPI static const core::Property FileFilterRegex;
  EXTENSIONAPI static const core::Property PathFilterRegex;
  EXTENSIONAPI static const core::Property IgnoreDottedFiles;
  EXTENSIONAPI static const core::Property TargetSystemTimestampPrecision;
  EXTENSIONAPI static const core::Property EntityTrackingTimeWindow;
  EXTENSIONAPI static const core::Property EntityTrackingInitialListingTarget;
  EXTENSIONAPI static const core::Property MinimumFileAge;
  EXTENSIONAPI static const core::Property MaximumFileAge;
  EXTENSIONAPI static const core::Property MinimumFileSize;
  EXTENSIONAPI static const core::Property MaximumFileSize;
  static auto properties() {
    return utils::array_cat(SFTPProcessorBase::properties(), std::array{
      ListingStrategy,
      RemotePath,
      SearchRecursively,
      FollowSymlink,
      FileFilterRegex,
      PathFilterRegex,
      IgnoreDottedFiles,
      TargetSystemTimestampPrecision,
      EntityTrackingTimeWindow,
      EntityTrackingInitialListingTarget,
      MinimumFileAge,
      MaximumFileAge,
      MinimumFileSize,
      MaximumFileSize
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

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
  core::StateManager* state_manager_{};
  std::string listing_strategy_;
  bool search_recursively_{};
  bool follow_symlink_{};
  std::string file_filter_regex_;
  std::string path_filter_regex_;
  std::optional<utils::Regex> compiled_file_filter_regex_;
  std::optional<utils::Regex> compiled_path_filter_regex_;
  bool ignore_dotted_files_{};
  std::string target_system_timestamp_precision_;
  std::string entity_tracking_initial_listing_target_;
  std::chrono::milliseconds minimum_file_age_{};
  std::chrono::milliseconds maximum_file_age_{};
  uint64_t minimum_file_size_{};
  uint64_t maximum_file_size_{};

  std::string last_listing_strategy_;
  std::string last_hostname_;
  std::string last_username_;
  std::filesystem::path last_remote_path_;

  struct Child {
    Child() = default;
    Child(const std::string& parent_path_, std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>&& sftp_child);
    [[nodiscard]] std::string getPath() const;

    bool directory{false};
    std::filesystem::path parent_path;
    std::filesystem::path filename;
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
  };

  bool already_loaded_from_cache_{};

  std::chrono::steady_clock::time_point last_run_time_{};
  std::optional<std::chrono::system_clock::time_point> last_listed_latest_entry_timestamp_;
  std::optional<std::chrono::system_clock::time_point> last_processed_latest_entry_timestamp_;
  std::set<std::string> latest_identifiers_processed_;

  bool initial_listing_complete_{};
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
      std::chrono::milliseconds entity_tracking_time_window,
      std::vector<Child>&& files);
};

}  // namespace org::apache::nifi::minifi::processors
