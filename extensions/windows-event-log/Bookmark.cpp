#include "Bookmark.h"

#include <direct.h>

#include "wel/UnicodeConversion.h"
#include "utils/file/FileUtils.h"
#include "utils/ScopeGuard.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
static const std::string BOOKMARK_KEY = "bookmark";

Bookmark::Bookmark(const std::wstring& channel, const std::wstring& query, const std::string& bookmarkRootDir, const std::string& uuid, bool processOldEvents, std::shared_ptr<core::CoreComponentStateManager> state_manager, std::shared_ptr<logging::Logger> logger)
  : logger_(logger)
  , state_manager_(state_manager) {
  std::unordered_map<std::string, std::string> state_map;
  if (state_manager_->get(state_map) && state_map.count(BOOKMARK_KEY) == 1U) {
    bookmarkXml_ = wel::to_wstring(state_map[BOOKMARK_KEY].c_str());
  } else if (!bookmarkRootDir.empty()) {
    filePath_ = utils::file::FileUtils::concat_path(
      utils::file::FileUtils::concat_path(
        utils::file::FileUtils::concat_path(bookmarkRootDir, "uuid"), uuid), "Bookmark.txt");

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
    if (hBookmark_ = EvtCreateBookmark(bookmarkXml_.c_str())) {
      ok_ = true;
      return;
    }

    LOG_LAST_ERROR(EvtCreateBookmark);

    bookmarkXml_.clear();
    state_map.erase(BOOKMARK_KEY);
    state_manager_->set(state_map);
  }

  if (!(hBookmark_ = EvtCreateBookmark(0))) {
    LOG_LAST_ERROR(EvtCreateBookmark);
    return;
  }

  const auto hEventResults = EvtQuery(0, channel.c_str(), query.c_str(), EvtQueryChannelPath);
  if (!hEventResults) {
    LOG_LAST_ERROR(EvtQuery);
    return;
  }
  const utils::ScopeGuard guard_hEventResults([hEventResults]() { EvtClose(hEventResults); });

  if (!EvtSeek(hEventResults, 0, 0, 0, processOldEvents? EvtSeekRelativeToFirst : EvtSeekRelativeToLast)) {
    LOG_LAST_ERROR(EvtSeek);
    return;
  }

  DWORD dwReturned{};
  EVT_HANDLE hEvent{};
  if (!EvtNext(hEventResults, 1, &hEvent, INFINITE, 0, &dwReturned)) {
    LOG_LAST_ERROR(EvtNext);
    return;
  }

  ok_ = saveBookmark(hEvent);
}

Bookmark::~Bookmark() {
  if (hBookmark_) {
    EvtClose(hBookmark_);
  }
}

Bookmark::operator bool() const {
  return ok_;
}
  
EVT_HANDLE Bookmark::getBookmarkHandleFromXML() {
  if (hBookmark_) {
    EvtClose(hBookmark_);
    hBookmark_ = 0;
  }

  hBookmark_ = EvtCreateBookmark(bookmarkXml_.c_str());
  if (!(hBookmark_ = EvtCreateBookmark(bookmarkXml_.c_str()))) {
    LOG_LAST_ERROR(EvtCreateBookmark);
    return 0;
  }

  return hBookmark_;
}

bool Bookmark::saveBookmarkXml(const std::wstring& bookmarkXml) {
  bookmarkXml_ = bookmarkXml;

  std::unordered_map<std::string, std::string> state_map;
  state_map[BOOKMARK_KEY] = wel::to_string(bookmarkXml_.c_str());

  return state_manager_->set(state_map);
}

bool Bookmark::saveBookmark(EVT_HANDLE hEvent)
{
  std::wstring bookmarkXml;
  if (!getNewBookmarkXml(hEvent, bookmarkXml)) {
    return false;
  }

  return saveBookmarkXml(bookmarkXml);
}

bool Bookmark::getNewBookmarkXml(EVT_HANDLE hEvent, std::wstring& bookmarkXml) {
  if (!EvtUpdateBookmark(hBookmark_, hEvent)) {
    LOG_LAST_ERROR(EvtUpdateBookmark);
    return false;
  }

  // Render the bookmark as an XML string that can be persisted.
  DWORD bufferSize{};
  DWORD bufferUsed{};
  DWORD propertyCount{};
  if (!EvtRender(0, hBookmark_, EvtRenderBookmark, bufferSize, 0, &bufferUsed, &propertyCount)) {
    DWORD status = ERROR_SUCCESS;
    if (ERROR_INSUFFICIENT_BUFFER == (status = GetLastError())) {
      bufferSize = bufferUsed;

      std::vector<wchar_t> buf(bufferSize / 2 + 1);

      if (!EvtRender(0, hBookmark_, EvtRenderBookmark, bufferSize, &buf[0], &bufferUsed, &propertyCount)) {
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

  // Generically is not efficient, but bookmarkXML is small ~100 bytes. 
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
