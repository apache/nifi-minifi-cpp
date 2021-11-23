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

#include "Bookmark.h"

#include <direct.h>
#include <vector>
#include <unordered_map>
#include <utility>
#include <fstream>

#include "wel/UnicodeConversion.h"
#include "utils/file/FileUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
static const std::string BOOKMARK_KEY = "bookmark";

Bookmark::Bookmark(const std::wstring& channel,
    const std::wstring& query,
    const std::string& bookmarkRootDir,
    const utils::Identifier& uuid,
    bool processOldEvents,
    core::CoreComponentStateManager* state_manager,
    std::shared_ptr<core::logging::Logger> logger)
    : logger_(std::move(logger)),
      state_manager_(state_manager) {
  std::unordered_map<std::string, std::string> state_map;
  if (state_manager_->get(state_map) && state_map.count(BOOKMARK_KEY) == 1U) {
    bookmarkXml_ = wel::to_wstring(state_map[BOOKMARK_KEY].c_str());
  } else if (!bookmarkRootDir.empty()) {
    filePath_ = utils::file::concat_path(
      utils::file::concat_path(
        utils::file::concat_path(bookmarkRootDir, "uuid"), uuid.to_string()), "Bookmark.txt");

    std::wstring bookmarkXml;
    if (getBookmarkXmlFromFile(bookmarkXml)) {
      if (saveBookmarkXml(bookmarkXml) && state_manager_->persist()) {
        logger_->log_info("State migration successful");
        rename(filePath_.c_str(), (filePath_ + "-migrated").c_str());
      } else {
        logger_->log_warn("Could not migrate state from specified State Directory %s", bookmarkRootDir);
      }
    }
  } else {
    logger_->log_info("Found no stored state");
  }

  if (!bookmarkXml_.empty()) {
    if (hBookmark_ = unique_evt_handle{ EvtCreateBookmark(bookmarkXml_.c_str()) }) {
      ok_ = true;
      return;
    }

    LOG_LAST_ERROR(EvtCreateBookmark);

    bookmarkXml_.clear();
    state_map.erase(BOOKMARK_KEY);
    state_manager_->set(state_map);
  }

  if (!(hBookmark_ = unique_evt_handle{ EvtCreateBookmark(0) })) {
    LOG_LAST_ERROR(EvtCreateBookmark);
    return;
  }

  const auto hEventResults = unique_evt_handle{ EvtQuery(0, channel.c_str(), query.c_str(), EvtQueryChannelPath) };
  if (!hEventResults) {
    LOG_LAST_ERROR(EvtQuery);
    return;
  }

  if (!EvtSeek(hEventResults.get(), 0, 0, 0, processOldEvents? EvtSeekRelativeToFirst : EvtSeekRelativeToLast)) {
    LOG_LAST_ERROR(EvtSeek);
    return;
  }

  const unique_evt_handle hEvent = [this, &hEventResults] {
    DWORD dwReturned{};
    EVT_HANDLE hEvent{ nullptr };
    if (!EvtNext(hEventResults.get(), 1, &hEvent, INFINITE, 0, &dwReturned)) {
      LOG_LAST_ERROR(EvtNext);
    }
    return unique_evt_handle{ hEvent };
  }();

  ok_ = hEvent && saveBookmark(hEvent.get());
}

Bookmark::~Bookmark() = default;

Bookmark::operator bool() const noexcept {
  return ok_;
}

EVT_HANDLE Bookmark::getBookmarkHandleFromXML() {
  hBookmark_ = unique_evt_handle{ EvtCreateBookmark(bookmarkXml_.c_str()) };
  if (!hBookmark_) {
    LOG_LAST_ERROR(EvtCreateBookmark);
  }

  return hBookmark_.get();
}

bool Bookmark::saveBookmarkXml(const std::wstring& bookmarkXml) {
  bookmarkXml_ = bookmarkXml;

  std::unordered_map<std::string, std::string> state_map;
  state_map[BOOKMARK_KEY] = wel::to_string(bookmarkXml_.c_str());

  return state_manager_->set(state_map);
}

bool Bookmark::saveBookmark(EVT_HANDLE hEvent) {
  std::wstring bookmarkXml;
  if (!getNewBookmarkXml(hEvent, bookmarkXml)) {
    return false;
  }

  return saveBookmarkXml(bookmarkXml);
}

bool Bookmark::getNewBookmarkXml(EVT_HANDLE hEvent, std::wstring& bookmarkXml) {
  if (!EvtUpdateBookmark(hBookmark_.get(), hEvent)) {
    LOG_LAST_ERROR(EvtUpdateBookmark);
    return false;
  }
  // Render the bookmark as an XML string that can be persisted.
  logger_->log_trace("Rendering new bookmark");
  DWORD bufferSize{};
  DWORD bufferUsed{};
  DWORD propertyCount{};
  if (!EvtRender(nullptr, hBookmark_.get(), EvtRenderBookmark, bufferSize, nullptr, &bufferUsed, &propertyCount)) {
    DWORD status = ERROR_SUCCESS;
    if (ERROR_INSUFFICIENT_BUFFER == (status = GetLastError())) {
      bufferSize = bufferUsed;

      std::vector<wchar_t> buf(bufferSize / 2 + 1);

      if (!EvtRender(nullptr, hBookmark_.get(), EvtRenderBookmark, bufferSize, &buf[0], &bufferUsed, &propertyCount)) {
        LOG_LAST_ERROR(EvtRender);
        return false;
      }

      bookmarkXml = buf.data();

      return true;
    }
    if (ERROR_SUCCESS != (status = GetLastError())) {
      LOG_LAST_ERROR(EvtRender);
      return false;
    }
  }

  return false;
}

bool Bookmark::getBookmarkXmlFromFile(std::wstring& bookmarkXml) {
  bookmarkXml.clear();

  std::wifstream file(filePath_);
  if (!file.is_open()) {
    return false;
  }

  // Generally is not efficient, but bookmarkXML is small ~100 bytes.
  wchar_t c;
  do {
    file.read(&c, 1);
    if (!file) {
      break;
    }

    bookmarkXml += c;
  } while (true);

  file.close();

  if (bookmarkXml.empty()) {
    return true;
  }

  // '!' should be at the end of bookmark.
  auto pos = bookmarkXml.find(L'!');
  if (std::wstring::npos == pos) {
    logger_->log_error("No '!' in bookmarXml '%ls'", bookmarkXml.c_str());
    bookmarkXml.clear();
    return false;
  }

  // Remove '!'.
  bookmarkXml.resize(pos);

  return true;
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
