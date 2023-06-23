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

#pragma once
#include <filesystem>
#include <utility>
#include <string>
#include "windows.h"
#include "lm.h"
#include "utils/OsUtils.h"
#include "utils/expected.h"
#include "TestUtils.h"
#include "ListSmb.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::smb::test {

class TempSmbShare {
 public:
  TempSmbShare(TempSmbShare&& other) = default;

  ~TempSmbShare() {
    if (!net_name_.empty())
      NetShareDel(nullptr, net_name_.data(), 0);
  }

  static nonstd::expected<TempSmbShare, std::error_code> create(std::wstring net_name, std::wstring path) {
    SHARE_INFO_502 shareInfo{};
    std::wstring remark = L"SMB share to test SMB capabilities of minifi";
    shareInfo.shi502_netname = net_name.data();
    shareInfo.shi502_type = STYPE_DISKTREE;
    shareInfo.shi502_remark = remark.data();
    shareInfo.shi502_permissions = ACCESS_ALL;
    shareInfo.shi502_max_uses = -1;
    shareInfo.shi502_current_uses = 0;
    shareInfo.shi502_path = path.data();
    shareInfo.shi502_passwd = nullptr;
    shareInfo.shi502_reserved = 0;
    shareInfo.shi502_security_descriptor = nullptr;

    DWORD netshare_result = NetShareAdd(nullptr, 502, reinterpret_cast<LPBYTE>(&shareInfo), nullptr);
    if (netshare_result == NERR_Success) {
      auto asd = TempSmbShare(std::move(net_name), std::move(path));
      return asd;
    }
    return nonstd::make_unexpected(utils::OsUtils::windowsErrorToErrorCode(netshare_result));
  }

  std::filesystem::path getPath() const {
    return path_;
  }

 private:
  TempSmbShare(std::wstring net_name, std::wstring path) : net_name_(std::move(net_name)), path_(std::move(path)) {}

  std::wstring net_name_;
  std::wstring path_;
};

}  // namespace org::apache::nifi::minifi::extensions::smb::test
