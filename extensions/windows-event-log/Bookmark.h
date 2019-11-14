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
  Bookmark(const std::string& uuid, std::shared_ptr<logging::Logger> logger);
  ~Bookmark();

  operator bool();
  
  bool hasBookmarkXml();

  EVT_HANDLE bookmarkHandle();

  bool saveBookmark(EVT_HANDLE hEvent);

private:
  bool createEmptyBookmarkXmlFile();

  // Creates directory "processor_repository\ConsumeWindowsEventLog\uuid\{uuid}" under "bin" directory.
  bool createUUIDDir(const std::string& uuid, std::string& dir);

  std::string filePath(const std::string& uuid);

  bool getBookmarkXmlFromFile();

private:
  bool ok_{};
  EVT_HANDLE hBookmark_{};
  std::wstring bookmarkXml_;
  std::string filePath_;
  std::wfstream file_;
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
