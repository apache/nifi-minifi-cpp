// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <Windows.h>
#include <winevt.h>
#include <string>
#include <memory>
#include <type_traits>

#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "utils/expected.h"
#include "EventPath.h"
#include "UniqueEvtHandle.h"

namespace org::apache::nifi::minifi::wel {

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
  void setStateManager(core::StateManager* state_manager) { state_manager_ = state_manager; }
  bool isValid() const noexcept;
  /* non-owning */ EVT_HANDLE getBookmarkHandleFromXML();
  std::expected<std::wstring, std::string> getNewBookmarkXml(EVT_HANDLE hEvent);
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

}  // namespace org::apache::nifi::minifi::wel
