#pragma once

#include <string>
#include <memory>
#include <windows.h>
#include <winevt.h>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class Bookmark
{
public:
  Bookmark(const std::string& bookmarkRootDir, const std::string& uuid, std::shared_ptr<logging::Logger> logger);
  ~Bookmark();

  operator bool() const;
  
  bool hasBookmarkXml() const;

  EVT_HANDLE bookmarkHandle() const;

  bool saveBookmark(EVT_HANDLE hEvent);

  bool getNewBookmarkXml(EVT_HANDLE hEvent, std::wstring& bookmarkXml);

  void saveBookmarkXml(std::wstring& bookmarkXml);
private:
  bool createEmptyBookmarkXmlFile();

  bool createUUIDDir(const std::string& bookmarkRootDir, const std::string& uuid, std::string& dir);

  std::string filePath(const std::string& uuid);

  bool getBookmarkXmlFromFile(std::wstring& bookmarkXml);

private:
  std::shared_ptr<logging::Logger> logger_;
  std::string filePath_;
  bool ok_{};
  EVT_HANDLE hBookmark_{};
  std::wfstream file_;
  bool hasBookmarkXml_{};
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
