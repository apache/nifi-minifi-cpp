#include "Bookmark.h"

#include <direct.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

Bookmark::Bookmark(const std::string& uuid, std::shared_ptr<logging::Logger> logger)
  :logger_(logger) {
  if (createUUIDDir(uuid, filePath_)) {
    filePath_ += "Bookmark.txt";
  }

  if (!filePath_.empty()) {
    if (!getBookmarkXmlFromFile()) {
      return;
    }
  }

  if (bookmarkXml_.empty()) {
    if (!(hBookmark_ = EvtCreateBookmark(0))) {
      logger_->log_error("!EvtCreateBookmark error: %d", GetLastError());
      return;
    }
  } else {
    if (!(hBookmark_ = EvtCreateBookmark(bookmarkXml_.c_str()))) {
      logger_->log_error("!EvtCreateBookmark error: %d bookmarkXml_ '%s'", GetLastError(), bookmarkXml_.c_str());
      return;
    }
  }

  ok_ = true;
}

Bookmark::~Bookmark() {
  if (file_.is_open()) {
    file_.close();
  }

  if (hBookmark_) {
    EvtClose(hBookmark_);
  }
}

Bookmark::operator bool() {
  return ok_;
}
  
bool Bookmark::hasBookmarkXml() {
  return !bookmarkXml_.empty();
}

EVT_HANDLE Bookmark::bookmarkHandle() {
  return hBookmark_;
}

bool Bookmark::saveBookmark(EVT_HANDLE hEvent)
{
  if (!EvtUpdateBookmark(hBookmark_, hEvent)) {
    logger_->log_error("!EvtUpdateBookmark error: %d.", GetLastError());
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
        logger_->log_error("!EvtRender error: %d.", GetLastError());
        return false;
      }

      file_.seekp(std::ios::beg);

      // Write new bookmark over old and in the end write '!'. Then new bookmark is read until '!'.
      file_ << &buf[0] << L'!';

      file_.flush();
    }
    else if (ERROR_SUCCESS != (status = GetLastError())) {
      logger_->log_error("!EvtRender error: %d.", GetLastError());
      return false;
    }
  }

  return true;
}

bool Bookmark::createEmptyBookmarkXmlFile() {
  if (file_.is_open()) {
    file_.close();
  }

  file_.open(filePath_, std::ios::out);

  auto ret = file_.is_open();
  if (!ret) {
    logger_->log_error("Cannot open %s", filePath_.c_str());
  }

  return ret;
}

// Creates directory "processor_repository\ConsumeWindowsEventLog\uuid\{uuid}" under "bin" directory.
bool Bookmark::createUUIDDir(const std::string& uuid, std::string& dir)
{
  auto getRootDirectory = [](std::string& rootDir, std::shared_ptr<logging::Logger> logger)
  {
    char dir[MAX_PATH + 1];
    auto length = GetModuleFileName(0, dir, _countof(dir) - 1);
    if (!length) {
      logger->log_error("!GetModuleFileName error: %x", GetLastError());
      return false;
    }
    dir[length] = '\0';

    auto pBackslash = strrchr(dir, '\\');
    if (!pBackslash) {
      logger->log_error("!pBackslash");
      return false;
    }
    *(pBackslash + 1) = '\0';

    rootDir = dir;

    return true;
  };

  dir.clear();

  if (!getRootDirectory(dir, logger_))
    return false;

  for (const auto& curDir : std::vector<std::string>{ "processor_repository", "ConsumeWindowsEventLog", "uuid", uuid }) {
    dir += curDir + '\\';

    if (GetFileAttributes(dir.c_str()) == INVALID_FILE_ATTRIBUTES) {
      if (_mkdir(dir.c_str())) {
        logger_->log_error("!_mkdir directory '%s'", dir.c_str());
        dir.clear();
        return false;
      }
    }
  }

  return true;
}

std::string Bookmark::filePath(const std::string& uuid) {
  static auto s_filePath = [&uuid, this]() {
    std::string uuidDir;
    return createUUIDDir(uuid, uuidDir) ? uuidDir + "Bookmark.txt" : uuidDir;
  }();

  return s_filePath;
}

bool Bookmark::getBookmarkXmlFromFile() {
  bookmarkXml_.clear();

  std::wifstream file(filePath_);
  if (!file.is_open()) {
    return createEmptyBookmarkXmlFile();
  }

  wchar_t c;
  do {
    file.read(&c, 1);
    if (!file) {
      break;
    }

    bookmarkXml_ += c;
  } while (true);

  file.close();

  file_.open(filePath_);
  if (!file_.is_open()) {
    logger_->log_error("Cannot open %s", filePath_.c_str());
    bookmarkXml_.clear();
    return false;
  }

  if (bookmarkXml_.empty()) {
    return true;
  }

  // '!' should be at the end of bookmark.
  auto pos = bookmarkXml_.find(L'!');
  if (std::wstring::npos == pos) {
    logger_->log_error("No '!' in bookmarXml '%s'", bookmarkXml_.c_str());
    bookmarkXml_.clear();
    return createEmptyBookmarkXmlFile();
  }

  // Remove '!'.
  bookmarkXml_.resize(pos);

  return true;
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
