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

#include <windows.h>
#include <winevt.h>
#include <string>
#include <memory>
#include <type_traits>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "wel/UniqueEvtHandle.h"
#include "core/logging/Logger.h"
#include "utils/expected.h"
#include "wel/EventPath.h"

namespace org::apache::nifi::minifi::processors {

#define LOG_LAST_ERROR(func) logger_->log_error("!"#func" error {:#x}", GetLastError())

class Bookmark {
 public:
  Bookmark(const wel::EventPath& path,
      const std::wstring& query,
      const std::filesystem::path& bookmarkRootDir,
      const utils::Identifier& uuid,
      bool processOldEvents,
      core::StateManager* state_manager,
      std::shared_ptr<core::logging::Logger> logger);
  ~Bookmark();
  explicit operator bool() const noexcept;

  /* non-owning */ EVT_HANDLE getBookmarkHandleFromXML();
  nonstd::expected<std::wstring, std::string> getNewBookmarkXml(EVT_HANDLE hEvent);
  bool saveBookmarkXml(const std::wstring& bookmarkXml);

 private:
  bool saveBookmark(EVT_HANDLE hEvent);
  bool getBookmarkXmlFromFile(std::wstring& bookmarkXml);

  using unique_evt_handle = wel::unique_evt_handle;

  std::shared_ptr<core::logging::Logger> logger_;
  core::StateManager* state_manager_;
  std::filesystem::path filePath_;
  bool ok_{};
  unique_evt_handle hBookmark_;
  std::wstring bookmarkXml_;
};

}  // namespace org::apache::nifi::minifi::processors
