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

#include <string>
#include <memory>
#include <windows.h>
#include <winevt.h>
#include <type_traits>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

#define LOG_LAST_ERROR(func) logger_->log_error("!"#func" error %x", GetLastError())

class Bookmark
{
public:
  Bookmark(const std::wstring& channel, const std::wstring& query, const std::string& bookmarkRootDir, const std::string& uuid, bool processOldEvents, std::shared_ptr<core::CoreComponentStateManager> state_manager, std::shared_ptr<logging::Logger> logger);
  ~Bookmark();
  explicit operator bool() const noexcept;

  /* non-owning */ EVT_HANDLE getBookmarkHandleFromXML();
  bool getNewBookmarkXml(EVT_HANDLE hEvent, std::wstring& bookmarkXml);
  bool saveBookmarkXml(const std::wstring& bookmarkXml);
private:
  bool saveBookmark(EVT_HANDLE hEvent);
  bool getBookmarkXmlFromFile(std::wstring& bookmarkXml);

  struct evt_deleter {
    void operator()(EVT_HANDLE) const noexcept;
  };
  using unique_evt_handle = std::unique_ptr<std::remove_pointer<EVT_HANDLE>::type, evt_deleter>;

private:
  std::shared_ptr<logging::Logger> logger_;
  std::shared_ptr<core::CoreComponentStateManager> state_manager_;
  std::string filePath_;
  bool ok_{};
  unique_evt_handle hBookmark_;
  std::wstring bookmarkXml_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
