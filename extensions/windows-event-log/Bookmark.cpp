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

#include <vector>
#include <unordered_map>
#include <utility>
#include <fstream>

#include "wel/UnicodeConversion.h"
#include "utils/file/FileUtils.h"
#include "utils/OsUtils.h"

namespace org::apache::nifi::minifi::processors {
static const std::string BOOKMARK_KEY = "bookmark";

Bookmark::Bookmark(const std::wstring& channel,
    const std::wstring& query,
    const std::filesystem::path& bookmarkRootDir,
    const utils::Identifier& uuid,
    bool processOldEvents,
    core::StateManager* state_manager,
    std::shared_ptr<core::logging::Logger> logger)
    : logger_(std::move(logger)),
      state_manager_(state_manager) {
  std::unordered_map<std::string, std::string> state_map;
  if (state_manager_->get(state_map) && state_map.count(BOOKMARK_KEY) == 1U) {
    bookmarkXml_ = wel::to_wstring(state_map[BOOKMARK_KEY].c_str());
  } else if (!bookmarkRootDir.empty()) {
    filePath_ = bookmarkRootDir / "uuid" / uuid.to_string().view() / "Bookmark.txt";

    std::wstring bookmarkXml;
    if (getBookmarkXmlFromFile(bookmarkXml)) {
      if (saveBookmarkXml(bookmarkXml) && state_manager_->persist()) {
        logger_->log_info("State migration successful");
        auto migrated_path = filePath_;
        migrated_path += "-migrated";
        std::filesystem::rename(filePath_, migrated_path);
      } else {
        logger_->log_warn("Could not migrate state from specified State Directory %s", bookmarkRootDir.string());
      }
    }
  } else {
    logger_->log_info("Found no stored state");
  }

  if (!bookmarkXml_.empty()) {
    hBookmark_ = unique_evt_handle{EvtCreateBookmark(bookmarkXml_.c_str())};
    if (hBookmark_) {
      ok_ = true;
      return;
    }

    LOG_LAST_ERROR(EvtCreateBookmark);

    bookmarkXml_.clear();
    state_map.erase(BOOKMARK_KEY);
    state_manager_->set(state_map);
  }

  hBookmark_ = unique_evt_handle{EvtCreateBookmark(nullptr)};
  if (!hBookmark_) {
    LOG_LAST_ERROR(EvtCreateBookmark);
    return;
  }

  const auto hEventResults = unique_evt_handle{ EvtQuery(nullptr, channel.c_str(), query.c_str(), EvtQueryChannelPath) };
  if (!hEventResults) {
    LOG_LAST_ERROR(EvtQuery);
    return;
  }

  if (!EvtSeek(hEventResults.get(), 0, nullptr, 0, processOldEvents? EvtSeekRelativeToFirst : EvtSeekRelativeToLast)) {
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

bool Bookmark::saveBookmark(EVT_HANDLE event_handle) {
  auto bookmark_xml = getNewBookmarkXml(event_handle);
  if (!bookmark_xml) {
    logger_->log_error("%s", bookmark_xml.error());
    return false;
  }

  return saveBookmarkXml(*bookmark_xml);
}

nonstd::expected<std::wstring, std::string> Bookmark::getNewBookmarkXml(EVT_HANDLE hEvent) {
  if (!EvtUpdateBookmark(hBookmark_.get(), hEvent)) {
    return nonstd::make_unexpected(fmt::format("EvtUpdateBookmark failed due to %s", utils::OsUtils::windowsErrorToErrorCode(GetLastError()).message()));
  }
  // Render the bookmark as an XML string that can be persisted.
  logger_->log_trace("Rendering new bookmark");
  DWORD bufferSize{};
  DWORD bufferUsed{};
  DWORD propertyCount{};
  bool event_render_succeeded_without_buffer = EvtRender(nullptr, hBookmark_.get(), EvtRenderBookmark, bufferSize, nullptr, &bufferUsed, &propertyCount);
  if (event_render_succeeded_without_buffer)
    return nonstd::make_unexpected("EvtRender failed to determine the required buffer size.");

  auto last_error = GetLastError();
  if (last_error != ERROR_INSUFFICIENT_BUFFER)
    return nonstd::make_unexpected(fmt::format("EvtRender failed due to %s", utils::OsUtils::windowsErrorToErrorCode(last_error).message()));

  bufferSize = bufferUsed;
  std::vector<wchar_t> buf(bufferSize / 2 + 1);

  if (!EvtRender(nullptr, hBookmark_.get(), EvtRenderBookmark, bufferSize, buf.data(), &bufferUsed, &propertyCount)) {
    return nonstd::make_unexpected(fmt::format("EvtRender failed due to %s", utils::OsUtils::windowsErrorToErrorCode(GetLastError()).message()));
  }

  return std::wstring(buf.data());
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

}  // namespace org::apache::nifi::minifi::processors
