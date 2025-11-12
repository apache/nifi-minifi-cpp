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
#include <utility>
#include <string>
#include "../../SmbConnectionControllerService.h"
#include "utils/OsUtils.h"
#include "utils/file/FileUtils.h"
#include "ListSmb.h"
#include "unit/Catch.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::smb::test {

struct ListSmbExpectedAttributes {
  std::string expected_filename;
  std::string expected_path;
  std::string expected_service_location;
  std::chrono::system_clock::time_point expected_last_modified_time;
  std::chrono::system_clock::time_point expected_creation_time;
  std::chrono::system_clock::time_point expected_last_access_time;
  std::string expected_size;

  void checkAttributes(core::FlowFile& flow_file) {
    CHECK(flow_file.getAttribute(ListSmb::Filename.name) == expected_filename);
    CHECK(flow_file.getAttribute(ListSmb::Path.name) == expected_path);
    CHECK(flow_file.getAttribute(ListSmb::ServiceLocation.name) == expected_service_location);
    auto last_modified_time_from_attribute = utils::timeutils::parseDateTimeStr(*flow_file.getAttribute(ListSmb::LastModifiedTime.name));
    auto creation_time_from_attribute = utils::timeutils::parseDateTimeStr(*flow_file.getAttribute(ListSmb::CreationTime.name));
    auto last_access_time_from_attribute = utils::timeutils::parseDateTimeStr(*flow_file.getAttribute(ListSmb::LastAccessTime.name));

    CHECK(std::chrono::abs(expected_last_modified_time - *last_modified_time_from_attribute) < 5s);
    CHECK(std::chrono::abs(expected_creation_time - *creation_time_from_attribute) < 5s);
    CHECK(std::chrono::abs(expected_last_access_time - *last_access_time_from_attribute) < 5s);
    CHECK(flow_file.getAttribute(ListSmb::Size.name) == expected_size);
  }
};

class MockSmbConnectionControllerService : public SmbConnectionControllerService {
 public:
  using SmbConnectionControllerService::SmbConnectionControllerService;
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;

  void onEnable() override {}
  void notifyStop() override {}

  std::error_code validateConnection() override {
    if (server_path_)
      return {};
    return std::make_error_code(std::errc::not_connected);
  }
  std::filesystem::path getPath() const override {
    gsl_Expects(server_path_);
    return *server_path_;
  }

  void setPath(std::filesystem::path path) { server_path_ = std::move(path);}

  nonstd::expected<ListSmbExpectedAttributes, std::error_code> addFile(const std::filesystem::path& relative_path,
      std::string_view content,
      std::chrono::file_clock::duration age) {
    auto full_path = getPath() / relative_path;
    std::filesystem::create_directories(full_path.parent_path());
    {
      std::ofstream out_file(full_path, std::ios::binary | std::ios::out);
      if (!out_file.is_open())
        return nonstd::make_unexpected(std::make_error_code(std::errc::bad_file_descriptor));
      out_file << content;
    }
    auto current_time = std::chrono::system_clock::now();
    auto last_write_time_error = utils::file::set_last_write_time(full_path, minifi::utils::file::from_sys(current_time) - age);
    if (!last_write_time_error)
      return nonstd::make_unexpected(std::make_error_code(std::errc::bad_file_descriptor));
    auto path = relative_path.parent_path().empty() ? (std::filesystem::path(".") / "").string() : (relative_path.parent_path() / "").string();
    return ListSmbExpectedAttributes{
        .expected_filename = relative_path.filename().string(),
        .expected_path = path,
        .expected_service_location = server_path_->string(),
        .expected_last_modified_time = current_time-age,
        .expected_creation_time = current_time,
        .expected_last_access_time = current_time,
        .expected_size = fmt::format("{}", content.size())};
  }

 private:
  std::optional<std::filesystem::path> server_path_ = std::nullopt;
};
}  // namespace org::apache::nifi::minifi::extensions::smb::test
