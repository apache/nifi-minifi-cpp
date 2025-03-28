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

#include <array>
#include <memory>
#include <string>
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <set>
#include <tuple>
#include <vector>
#include <optional>
#include <utility>
#include <string_view>

#include "SFTPProcessorBase.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "utils/ArrayUtils.h"
#include "utils/Id.h"
#include "controllers/keyvalue/KeyValueStateStorage.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

class ListSFTP : public SFTPProcessorBase {
 public:
  static constexpr std::string_view LISTING_STRATEGY_TRACKING_TIMESTAMPS = "Tracking Timestamps";
  static constexpr std::string_view LISTING_STRATEGY_TRACKING_ENTITIES = "Tracking Entities";

  static constexpr std::string_view TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT = "Auto Detect";
  static constexpr std::string_view TARGET_SYSTEM_TIMESTAMP_PRECISION_MILLISECONDS = "Milliseconds";
  static constexpr std::string_view TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS = "Seconds";
  static constexpr std::string_view TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES = "Minutes";

  static constexpr std::string_view ENTITY_TRACKING_INITIAL_LISTING_TARGET_TRACKING_TIME_WINDOW = "Tracking Time Window";
  static constexpr std::string_view ENTITY_TRACKING_INITIAL_LISTING_TARGET_ALL_AVAILABLE = "All Available";

  explicit ListSFTP(std::string_view name, const utils::Identifier& uuid = {});
  ~ListSFTP() override;

  EXTENSIONAPI static constexpr const char* Description = "Performs a listing of the files residing on an SFTP server. "
      "For each file that is found on the remote server, a new FlowFile will be created with "
      "the filename attribute set to the name of the file on the remote server. "
      "This can then be used in conjunction with FetchSFTP in order to fetch those files.";

  EXTENSIONAPI static constexpr auto ListingStrategy = core::PropertyDefinitionBuilder<2>::createProperty("Listing Strategy")
      .withDescription("Specify how to determine new/updated entities. See each strategy descriptions for detail.")
      .isRequired(true)
      .withAllowedValues({LISTING_STRATEGY_TRACKING_TIMESTAMPS, LISTING_STRATEGY_TRACKING_ENTITIES})
      .withDefaultValue(LISTING_STRATEGY_TRACKING_TIMESTAMPS)
      .build();
  EXTENSIONAPI static constexpr auto RemotePath = core::PropertyDefinitionBuilder<>::createProperty("Remote Path")
      .withDescription("The fully qualified filename on the remote system")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto SearchRecursively = core::PropertyDefinitionBuilder<>::createProperty("Search Recursively")
      .withDescription("If true, will pull files from arbitrarily nested subdirectories; "
                      "otherwise, will not traverse subdirectories")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto FollowSymlink = core::PropertyDefinitionBuilder<>::createProperty("Follow symlink")
      .withDescription("If true, will pull even symbolic files and also nested symbolic subdirectories; "
          "otherwise, will not read symbolic files and will not traverse symbolic link subdirectories")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto FileFilterRegex = core::PropertyDefinitionBuilder<>::createProperty("File Filter Regex")
      .withDescription("Provides a Java Regular Expression for filtering Filenames; "
          "if a filter is supplied, only files whose names match that Regular Expression will be fetched")
      .isRequired(false)
      .build();
  EXTENSIONAPI static constexpr auto PathFilterRegex = core::PropertyDefinitionBuilder<>::createProperty("Path Filter Regex")
      .withDescription("When Search Recursively is true, then only subdirectories whose path matches the given Regular Expression will be scanned")
      .isRequired(false)
      .build();
  EXTENSIONAPI static constexpr auto IgnoreDottedFiles = core::PropertyDefinitionBuilder<>::createProperty("Ignore Dotted Files")
      .withDescription("If true, files whose names begin with a dot (\".\") will be ignored")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto TargetSystemTimestampPrecision = core::PropertyDefinitionBuilder<4>::createProperty("Target System Timestamp Precision")
      .withDescription("Specify timestamp precision at the target system. "
          "Since this processor uses timestamp of entities to decide which should be listed, "
          "it is crucial to use the right timestamp precision.")
      .isRequired(true)
      .withAllowedValues({
          TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT,
          TARGET_SYSTEM_TIMESTAMP_PRECISION_MILLISECONDS,
          TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS,
          TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES})
      .withDefaultValue(TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT)
      .build();
  EXTENSIONAPI static constexpr auto EntityTrackingTimeWindow = core::PropertyDefinitionBuilder<>::createProperty("Entity Tracking Time Window")
      .withDescription("Specify how long this processor should track already-listed entities. "
          "'Tracking Entities' strategy can pick any entity whose timestamp is inside the specified time window. "
          "For example, if set to '30 minutes', any entity having timestamp in recent 30 minutes will be the listing target when this processor runs. "
          "A listed entity is considered 'new/updated' and a FlowFile is emitted if one of following condition meets: "
          "1. does not exist in the already-listed entities, "
          "2. has newer timestamp than the cached entity, "
          "3. has different size than the cached entity. "
          "If a cached entity's timestamp becomes older than specified time window, that entity will be removed from the cached already-listed entities. "
          "Used by 'Tracking Entities' strategy.")
      .isRequired(false)
      .build();
  EXTENSIONAPI static constexpr auto EntityTrackingInitialListingTarget = core::PropertyDefinitionBuilder<2>::createProperty("Entity Tracking Initial Listing Target")
      .withDescription("Specify how initial listing should be handled. Used by 'Tracking Entities' strategy.")
      .withAllowedValues({
          ENTITY_TRACKING_INITIAL_LISTING_TARGET_TRACKING_TIME_WINDOW,
          ENTITY_TRACKING_INITIAL_LISTING_TARGET_ALL_AVAILABLE})
      .isRequired(false)
      .withDefaultValue(ENTITY_TRACKING_INITIAL_LISTING_TARGET_ALL_AVAILABLE)
      .build();
  EXTENSIONAPI static constexpr auto MinimumFileAge = core::PropertyDefinitionBuilder<>::createProperty("Minimum File Age")
      .withDescription("The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
      .withDefaultValue("0 sec")
      .build();
  EXTENSIONAPI static constexpr auto MaximumFileAge = core::PropertyDefinitionBuilder<>::createProperty("Maximum File Age")
      .withDescription("The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored")
      .isRequired(false)
      .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
      .build();
  EXTENSIONAPI static constexpr auto MinimumFileSize = core::PropertyDefinitionBuilder<>::createProperty("Minimum File Size")
      .withDescription("The minimum size that a file must be in order to be pulled")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::DATA_SIZE_VALIDATOR)
      .withDefaultValue("0 B")
      .build();
  EXTENSIONAPI static constexpr auto MaximumFileSize = core::PropertyDefinitionBuilder<>::createProperty("Maximum File Size")
      .withDescription("The maximum size that a file must be in order to be pulled")
      .isRequired(false)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(SFTPProcessorBase::Properties, std::to_array<core::PropertyReference>({
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
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All FlowFiles that are received are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

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

  static constexpr std::array<std::pair<std::string_view, uint64_t>, 2> LISTING_LAG_MAP = {{
    {ListSFTP::TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS, 1000},
    {ListSFTP::TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES, 60000},
  }};

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

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
  std::optional<std::chrono::milliseconds> maximum_file_age_{};
  uint64_t minimum_file_size_{};
  std::optional<uint64_t> maximum_file_size_{};

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
      core::ProcessSession& session,
      const std::string& hostname,
      uint16_t port,
      const std::string& username,
      const Child& child);

  bool persistTrackingTimestampsCache(core::ProcessContext& context, const std::string& hostname, const std::string& username, const std::string& remote_path);
  bool updateFromTrackingTimestampsCache(core::ProcessContext& context, const std::string& hostname, const std::string& username, const std::string& remote_path);

  bool persistTrackingEntitiesCache(core::ProcessContext& context, const std::string& hostname, const std::string& username, const std::string& remote_path);
  bool updateFromTrackingEntitiesCache(core::ProcessContext& context, const std::string& hostname, const std::string& username, const std::string& remote_path);

  void listByTrackingTimestamps(
      core::ProcessContext& context,
      core::ProcessSession& session,
      const std::string& hostname,
      uint16_t port,
      const std::string& username,
      const std::string& remote_path,
      std::vector<Child>&& files);

  void listByTrackingEntities(
      core::ProcessContext& context,
      core::ProcessSession& session,
      const std::string& hostname,
      uint16_t port,
      const std::string& username,
      const std::string& remote_path,
      std::chrono::milliseconds entity_tracking_time_window,
      std::vector<Child>&& files);
};

}  // namespace org::apache::nifi::minifi::processors
