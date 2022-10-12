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
#include <optional>
#include <regex>
#include <string>
#include <utility>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Enum.h"
#include "utils/ListingStateManager.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::processors {

class ListFile : public core::Processor {
 public:
  explicit ListFile(std::string name, const utils::Identifier& uuid = {})
    : core::Processor(std::move(name), uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Retrieves a listing of files from the local filesystem. For each file that is listed, "
      "creates a FlowFile that represents the file so that it can be fetched in conjunction with FetchFile.";

  EXTENSIONAPI static const core::Property InputDirectory;
  EXTENSIONAPI static const core::Property RecurseSubdirectories;
  EXTENSIONAPI static const core::Property FileFilter;
  EXTENSIONAPI static const core::Property PathFilter;
  EXTENSIONAPI static const core::Property MinimumFileAge;
  EXTENSIONAPI static const core::Property MaximumFileAge;
  EXTENSIONAPI static const core::Property MinimumFileSize;
  EXTENSIONAPI static const core::Property MaximumFileSize;
  EXTENSIONAPI static const core::Property IgnoreHiddenFiles;
  static auto properties() {
    return std::array{
      InputDirectory,
      RecurseSubdirectories,
      FileFilter,
      PathFilter,
      MinimumFileAge,
      MaximumFileAge,
      MinimumFileSize,
      MaximumFileSize,
      IgnoreHiddenFiles
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &session_factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  struct ListedFile : public utils::ListedObject {
    [[nodiscard]] std::chrono::time_point<std::chrono::system_clock> getLastModified() const override {
      return std::chrono::time_point_cast<std::chrono::milliseconds>(utils::file::FileUtils::to_sys(last_modified_time));
    }

    [[nodiscard]] std::string getKey() const override {
      return absolute_path;
    }

    std::string filename;
    std::string absolute_path;
    std::filesystem::file_time_type last_modified_time;
    std::string relative_path;
    std::string full_file_path;
    uint64_t file_size = 0;
  };

  bool fileMatchesFilters(const ListedFile& listed_file);
  std::shared_ptr<core::FlowFile> createFlowFile(core::ProcessSession& session, const ListedFile& listed_file);

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListFile>::getLogger();
  std::string input_directory_;
  std::unique_ptr<minifi::utils::ListingStateManager> state_manager_;
  bool recurse_subdirectories_ = true;
  std::optional<std::regex> file_filter_;
  std::optional<std::regex> path_filter_;
  std::optional<std::chrono::milliseconds> minimum_file_age_;
  std::optional<std::chrono::milliseconds> maximum_file_age_;
  std::optional<uint64_t> minimum_file_size_;
  std::optional<uint64_t> maximum_file_size_;
  bool ignore_hidden_files_ = true;
};

}  // namespace org::apache::nifi::minifi::processors
