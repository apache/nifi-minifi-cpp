/**
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
#ifndef LIBMINIFI_INCLUDE_UTILS_FILE_FILEUTILS_H_
#define LIBMINIFI_INCLUDE_UTILS_FILE_FILEUTILS_H_

#include <fstream>
#include <memory>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

#ifdef USE_BOOST
#include <dirent.h>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#ifndef WIN32
#include <sys/stat.h>
#endif
#else
#include <errno.h>

#include <cstdlib>
#include <cstring>

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")
#else
#include <dirent.h> // NOLINT
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>

#endif
#endif
#include <cstdio>

#ifndef WIN32
#include <unistd.h>

#endif
#include <fcntl.h>

#ifdef WIN32
#include <direct.h>
#include <sys/stat.h>  // stat // NOLINT
#include <sys/types.h> // NOLINT
#include <sys/utime.h>  // _utime64
#include <tchar.h>  // _tcscpy,_tcscat,_tcscmp
#include <windows.h>  // winapi

#include <algorithm>  // replace
#include <string>  // string

#include "properties/Properties.h"
#include "utils/Id.h"

#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>

#endif

#include "core/logging/LoggerConfiguration.h"
#include "utils/StringUtils.h"
#include "utils/file/PathUtils.h"
#include "utils/gsl.h"

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif


namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {

namespace FileUtils = ::org::apache::nifi::minifi::utils::file;

namespace detail {
static inline int platform_create_dir(const std::string& path) {
#ifdef WIN32
  return _mkdir(path.c_str());
#else
  return mkdir(path.c_str(), 0777);
#endif
}
}  // namespace detail

/*
 * Get the platform-specific path separator.
 * @param force_posix returns the posix path separator ('/'), even when not on posix. Useful when dealing with remote posix paths.
 * @return the path separator character
 */
#ifdef WIN32
inline char get_separator(bool force_posix = false) {
  if (!force_posix) {
    return '\\';
  }
  return '/';
}
#else
inline char get_separator(bool /*force_posix*/ = false) {
  return '/';
}
#endif

inline std::string normalize_path_separators(std::string path, bool force_posix = false) {
  const auto normalize_separators = [force_posix](const char c) {
    if (c == '\\' || c == '/') { return get_separator(force_posix); }
    return c;
  };
  std::transform(std::begin(path), std::end(path), std::begin(path), normalize_separators);
  return path;
}

inline std::string get_temp_directory() {
#ifdef WIN32
  char tempBuffer[MAX_PATH];
  const auto ret = GetTempPath(MAX_PATH, tempBuffer);
  if (ret > MAX_PATH || ret == 0)
    throw std::runtime_error("Couldn't locate temporary directory path");
  return tempBuffer;
#else
  return "/tmp";
#endif
}

inline int64_t delete_dir(const std::string &path, bool delete_files_recursively = true) {
  // Empty path is interpreted as the root of the current partition on Windows, which should not be allowed
  if (path.empty()) {
    return -1;
  }
#ifdef USE_BOOST
  try {
    if (boost::filesystem::exists(path)) {
      if (delete_files_recursively) {
        boost::filesystem::remove_all(path);
      } else {
        boost::filesystem::remove(path);
      }
    }
  } catch(boost::filesystem::filesystem_error const & e) {
    return -1;
    // display error message
  }
  return 0;
#elif defined(WIN32)
  WIN32_FIND_DATA FindFileData;
  HANDLE hFind;
  DWORD Attributes;
  std::string str;


  std::stringstream pathstr;
  pathstr << path << "\\*";
  str = pathstr.str();
  // List files
  hFind = FindFirstFile(str.c_str(), &FindFileData);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (strcmp(FindFileData.cFileName, ".") != 0 && strcmp(FindFileData.cFileName, "..") != 0) {
        std::stringstream strs;
        strs << path << "\\" << FindFileData.cFileName;
        str = strs.str();

        Attributes = GetFileAttributes(str.c_str());
        if (Attributes & FILE_ATTRIBUTE_DIRECTORY) {
          // is directory
          delete_dir(str, delete_files_recursively);
        } else {
          remove(str.c_str());
          // not directory
        }
      }
    }while (FindNextFile(hFind, &FindFileData));
    FindClose(hFind);

    RemoveDirectory(path.c_str());
  }
  return 0;
#else
  DIR *current_directory = opendir(path.c_str());
  int r = -1;
  if (current_directory) {
    struct dirent *p;
    r = 0;
    while (!r && (p = readdir(current_directory))) {
      int r2 = -1;
      std::stringstream newpath;
      if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
        continue;
      }
      struct stat statbuf;

      newpath << path << "/" << p->d_name;

      if (!stat(newpath.str().c_str(), &statbuf)) {
        if (S_ISDIR(statbuf.st_mode)) {
          if (delete_files_recursively) {
            r2 = delete_dir(newpath.str(), delete_files_recursively);
          }
        } else {
          r2 = unlink(newpath.str().c_str());
        }
      }
      r = r2;
    }
    closedir(current_directory);
  }

  if (!r) {
    return rmdir(path.c_str());
  }

  return 0;
#endif
}

inline uint64_t last_write_time(const std::string &path) {
#ifdef USE_BOOST
  boost::system::error_code ec;
  auto result = boost::filesystem::last_write_time(path, ec);
  if (ec.value() == 0) {
    return result;
  }
#elif defined(WIN32)
  struct _stat64 result;
  if (_stat64(path.c_str(), &result) == 0) {
    return result.st_mtime;
  }
#else
  struct stat result;
  if (stat(path.c_str(), &result) == 0) {
    return result.st_mtime;
  }
#endif
  return 0;
}

inline std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> last_write_time_point(const std::string &path) {
  return std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>{std::chrono::seconds{last_write_time(path)}};
}

inline uint64_t file_size(const std::string &path) {
#ifdef WIN32
  struct _stat64 result;
  if (_stat64(path.c_str(), &result) == 0) {
    return result.st_size;
  }
#else
  struct stat result;
  if (stat(path.c_str(), &result) == 0) {
    return result.st_size;
  }
#endif
  return 0;
}

inline bool set_last_write_time(const std::string &path, uint64_t write_time) {
#ifdef USE_BOOST
  boost::filesystem::last_write_time(path, write_time);
  return true;
#elif defined(WIN32)
  struct __utimbuf64 utim;
  utim.actime = write_time;
  utim.modtime = write_time;
  return _utime64(path.c_str(), &utim) == 0U;
#else
  struct utimbuf utim;
  utim.actime = write_time;
  utim.modtime = write_time;
  return utime(path.c_str(), &utim) == 0U;
#endif
}

#ifndef WIN32
inline bool get_permissions(const std::string &path, uint32_t &permissions) {
  struct stat result;
  if (stat(path.c_str(), &result) == 0) {
    permissions = result.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
    return true;
  }
  return false;
}

inline int set_permissions(const std::string &path, const uint32_t permissions) {
#ifdef USE_BOOST
  boost::system::error_code ec;
  boost::filesystem::permissions(path, static_cast<boost::filesystem::perms>(permissions), ec);
  return ec.value();
#else
  return chmod(path.c_str(), permissions);
#endif
}
#endif

#ifndef WIN32
inline bool get_uid_gid(const std::string &path, uint64_t &uid, uint64_t &gid) {
  struct stat result;
  if (stat(path.c_str(), &result) == 0) {
    uid = result.st_uid;
    gid = result.st_gid;
    return true;
  }
  return false;
}
#endif

inline bool is_directory(const char * path) {
#ifndef WIN32
  struct stat dir_stat;
  if (stat(path, &dir_stat) != 0) {
      return false;
  }
  return S_ISDIR(dir_stat.st_mode) != 0;
#else
  struct _stat64 dir_stat;
  if (_stat64(path, &dir_stat) != 0) {
      return false;
  }
  return S_ISDIR(dir_stat.st_mode) != 0;
#endif
}

inline bool exists(const std::string& path) {
#ifdef USE_BOOST
  return boost::filesystem::exists(path);
#elif defined(WIN32)
  struct _stat64 statbuf;
  return _stat64(path.c_str(), &statbuf) == 0;
#else
  struct stat statbuf;
  return stat(path.c_str(), &statbuf) == 0;
#endif
}

inline int create_dir(const std::string& path, bool recursive = true) {
#ifdef USE_BOOST
  boost::filesystem::path dir(path);
  boost::system::error_code ec;
  if (!recursive) {
    boost::filesystem::create_directory(dir, ec);
  } else {
    boost::filesystem::create_directories(dir, ec);
  }
  if (ec.value() == 0 || (ec.value() == EEXIST && is_directory(path.c_str()))) {
    return 0;
  }
  return ec.value();
#else
  if (!recursive) {
    if (detail::platform_create_dir(path) != 0 && errno != EEXIST) {
      return -1;
    }
    return 0;
  }
  if (detail::platform_create_dir(path) == 0) {
    return 0;
  }

  switch (errno) {
  case ENOENT: {
    size_t found = path.find_last_of(get_separator());

    if (found == std::string::npos) {
      return -1;
    }

    const std::string dir = path.substr(0, found);
    int res = create_dir(dir, recursive);
    if (res < 0) {
      return -1;
    }
    return detail::platform_create_dir(path);
  }
  case EEXIST: {
    if (is_directory(path.c_str())) {
      return 0;
    }
    return -1;
  }
  default:
    return -1;
  }
  return -1;
#endif
}

inline int copy_file(const std::string &path_from, const std::string& dest_path) {
  std::ifstream src(path_from, std::ios::binary);
  if (!src.is_open())
    return -1;
  std::ofstream dest(dest_path, std::ios::binary);
  dest << src.rdbuf();
  return 0;
}

inline void addFilesMatchingExtension(const std::shared_ptr<logging::Logger> &logger, const std::string &originalPath, const std::string &extension, std::vector<std::string> &accruedFiles) {
#ifndef WIN32

  struct stat s;
  if (stat(originalPath.c_str(), &s) == 0) {
    if (s.st_mode & S_IFDIR) {
      DIR *d;
      d = opendir(originalPath.c_str());
      if (!d) {
        return;
      }
      // only perform a listing while we are not empty
      logger->log_debug("Performing file listing on %s", originalPath);

      struct dirent *entry;
      entry = readdir(d);
      while (entry != nullptr) {
        std::string d_name = entry->d_name;
        std::string path = originalPath + "/" + d_name;
        struct stat statbuf { };
        if (stat(path.c_str(), &statbuf) != 0) {
          logger->log_warn("Failed to stat %s", path);
          return;
        }
        if (S_ISDIR(statbuf.st_mode)) {
          // if this is a directory
          if (d_name != ".." && d_name != ".") {
            addFilesMatchingExtension(logger, path, extension, accruedFiles);
          }
        } else {
          if (utils::StringUtils::endsWith(path, extension)) {
            logger->log_info("Adding %s to paths", path);
            accruedFiles.push_back(path);
          }
        }
        entry = readdir(d);
      }
      closedir(d);
    } else if (s.st_mode & S_IFREG) {
      if (utils::StringUtils::endsWith(originalPath, extension)) {
        logger->log_info("Adding %s to paths", originalPath);
        accruedFiles.push_back(originalPath);
      }
    } else {
      logger->log_error("Could not stat", originalPath);
    }

  } else {
    logger->log_error("Could not access %s", originalPath);
  }
#else
  HANDLE hFind;
  WIN32_FIND_DATA FindFileData;

  std::string pathToSearch = originalPath + "\\*" + extension;
  if ((hFind = FindFirstFileA(pathToSearch.c_str(), &FindFileData)) != INVALID_HANDLE_VALUE) {
    do {
      struct _stat64 statbuf {};

      std::string path = originalPath + "\\" + FindFileData.cFileName;
      logger->log_info("Adding %s to paths", path);
      if (_stat64(path.c_str(), &statbuf) != 0) {
        logger->log_warn("Failed to stat %s", path);
        break;
      }
      logger->log_info("Adding %s to paths", path);
      if (S_ISDIR(statbuf.st_mode)) {
        addFilesMatchingExtension(logger, path, extension, accruedFiles);
      } else {
        if (utils::StringUtils::endsWith(path, extension)) {
          logger->log_info("Adding %s to paths", path);
          accruedFiles.push_back(path);
        }
      }
    }while (FindNextFileA(hFind, &FindFileData));
    FindClose(hFind);
  }
#endif
}

inline std::string concat_path(const std::string& root, const std::string& child, bool force_posix = false) {
  if (root.empty()) {
    return child;
  }
  std::stringstream new_path;
  if (root.back() == get_separator(force_posix)) {
    new_path << root << child;
  } else {
    new_path << root << get_separator(force_posix) << child;
  }
  return new_path.str();
}

/*
 * Provides a platform-independent function to list a directory
 * Callback is called for every file found: first argument is the path of the directory, second is the filename
 * Return value of the callback is used to continue (true) or stop (false) listing
 */
inline void list_dir(const std::string& dir, std::function<bool(const std::string&, const std::string&)> callback,
                     const std::shared_ptr<logging::Logger> &logger, std::function<bool(const std::string&)> dir_callback) {
  logger->log_debug("Performing file listing against %s", dir);
#ifndef WIN32
  DIR *d = opendir(dir.c_str());
  if (!d) {
    logger->log_warn("Failed to open directory: %s", dir.c_str());
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(d)) != NULL) {
    std::string d_name = entry->d_name;
    std::string path = concat_path(dir, d_name);

    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) != 0) {
      logger->log_warn("Failed to stat %s", path);
      continue;
    }

    if (S_ISDIR(statbuf.st_mode)) {
      // if this is a directory
      if (strcmp(d_name.c_str(), "..") != 0 && strcmp(d_name.c_str(), ".") != 0) {
        if (dir_callback(dir)) {
          list_dir(path, callback, logger, dir_callback);
        }
      }
    } else {
      if (!callback(dir, d_name)) {
        break;
      }
    }
  }
  closedir(d);
#else
  HANDLE hFind;
  WIN32_FIND_DATA FindFileData;

  std::string pathToSearch = dir + "\\*.*";
  hFind = FindFirstFileA(pathToSearch.c_str(), &FindFileData);

  if (hFind == INVALID_HANDLE_VALUE) {
    logger->log_warn("Failed to open directory: %s", dir.c_str());
    return;
  }

  do {
    struct _stat64 statbuf {};
    if (strcmp(FindFileData.cFileName, ".") != 0 && strcmp(FindFileData.cFileName, "..") != 0) {
      std::string path = concat_path(dir, FindFileData.cFileName);
      if (_stat64(path.c_str(), &statbuf) != 0) {
        logger->log_warn("Failed to stat %s", path);
        continue;
      }
      if (S_ISDIR(statbuf.st_mode)) {
        if (dir_callback(dir)) {
          list_dir(path, callback, logger, dir_callback);
        }
      } else {
        if (!callback(dir, FindFileData.cFileName)) {
          break;
        }
      }
    }
  } while (FindNextFileA(hFind, &FindFileData));
  FindClose(hFind);
#endif
}

inline void list_dir(const std::string& dir, std::function<bool(const std::string&, const std::string&)> callback,
                     const std::shared_ptr<logging::Logger> &logger, bool recursive = true) {
  list_dir(dir, callback, logger, [&] (const std::string&) {
    return recursive;
  });
}

inline void list_dir(const std::string& dir, std::function<bool(const std::string&, const utils::optional<std::string>&)> callback,
                     const std::shared_ptr<logging::Logger> &logger) {
  list_dir(dir, [&] (const std::string& dir, const std::string& file) {
    return callback(dir, file);
  }, logger, [&] (const std::string&) {
    return callback(dir, nullopt);
  });
}

inline std::vector<std::pair<std::string, std::string>> list_dir_all(const std::string& dir, const std::shared_ptr<logging::Logger> &logger,
    bool recursive = true)  {
  std::vector<std::pair<std::string, std::string>> fileList;
  auto lambda = [&fileList] (const std::string &path, const std::string &filename) {
    fileList.push_back(make_pair(path, filename));
    return true;
  };

  list_dir(dir, lambda, logger, recursive);

  return fileList;
}

inline std::string create_temp_directory(char* format) {
#ifdef WIN32
  const std::string tempDirectory = concat_path(get_temp_directory(),
      minifi::utils::IdGenerator::getIdGenerator()->generate().to_string());
  create_dir(tempDirectory);
  return tempDirectory;
#else
  if (mkdtemp(format) == nullptr) { return ""; }
  return format;
#endif
}

inline std::tuple<std::string /*parent_path*/, std::string /*child_path*/> split_path(const std::string& path, bool force_posix = false) {
  if (path.empty()) {
    /* Empty path has no parent and no child*/
    return std::make_tuple("", "");
  }
  bool absolute = false;
  size_t root_pos = 0U;
#ifdef WIN32
  if (!force_posix) {
      if (path[0] == '\\') {
        absolute = true;
        if (path.size() < 2U) {
          return std::make_tuple("", "");
        }
        if (path[1] == '\\') {
          if (path.size() >= 4U &&
             (path[2] == '?' || path[2] == '.') &&
              path[3] == '\\') {
            /* Probably an UNC path */
            root_pos = 4U;
          } else {
            /* Probably a \\server\-type path */
            root_pos = 2U;
          }
          root_pos = path.find_first_of("\\", root_pos);
          if (root_pos == std::string::npos) {
            return std::make_tuple("", "");
          }
        }
      } else if (path.size() >= 3U &&
                 toupper(path[0]) >= 'A' &&
                 toupper(path[0]) <= 'Z' &&
                 path[1] == ':' &&
                 path[2] == '\\') {
        absolute = true;
        root_pos = 2U;
      }
    } else {
#else
  if (true) {
#endif
    if (path[0] == '/') {
      absolute = true;
      root_pos = 0U;
    }
  }
  /* Maybe we are just a single relative child */
  if (!absolute && path.find(get_separator(force_posix)) == std::string::npos) {
    return std::make_tuple("", path);
  }
  /* Ignore trailing separators */
  size_t last_pos = path.size() - 1;
  while (last_pos > root_pos && path[last_pos] == get_separator(force_posix)) {
    last_pos--;
  }
  if (absolute && last_pos == root_pos) {
    /* This means we are only a root */
    return std::make_tuple("", "");
  }
  /* Find parent-child separator */
  size_t last_separator = path.find_last_of(get_separator(force_posix), last_pos);
  if (last_separator == std::string::npos || last_separator < root_pos) {
    return std::make_tuple("", "");
  }
  std::string parent = path.substr(0, last_separator + 1);
  std::string child = path.substr(last_separator + 1);

  return std::make_tuple(std::move(parent), std::move(child));
}

inline std::string get_parent_path(const std::string& path, bool force_posix = false) {
  std::string parent_path;
  std::tie(parent_path, std::ignore) = split_path(path, force_posix);
  return parent_path;
}

inline std::string get_child_path(const std::string& path, bool force_posix = false) {
  std::string child_path;
  std::tie(std::ignore, child_path) = split_path(path, force_posix);
  return child_path;
}

inline bool is_hidden(const std::string& path) {
#ifdef WIN32
  DWORD attributes = GetFileAttributesA(path.c_str());
    return ((attributes != INVALID_FILE_ATTRIBUTES)  && ((attributes & FILE_ATTRIBUTE_HIDDEN) != 0));
#else
  return std::get<1>(split_path(path)).rfind(".", 0) == 0;
#endif
}

/*
 * Returns the absolute path of the current executable
 */
inline std::string get_executable_path() {
#if defined(__linux__)
  std::vector<char> buf(1024U);
  while (true) {
    ssize_t ret = readlink("/proc/self/exe", buf.data(), buf.size());
    if (ret < 0) {
      return "";
    }
    if (static_cast<size_t>(ret) == buf.size()) {
      /* It may have been truncated */
      buf.resize(buf.size() * 2);
      continue;
    }
    return std::string(buf.data(), ret);
  }
#elif defined(__APPLE__)
  std::vector<char> buf(PATH_MAX);
    uint32_t buf_size = buf.size();
    while (_NSGetExecutablePath(buf.data(), &buf_size) != 0) {
      buf.resize(buf_size);
    }
    std::vector<char> resolved_name(PATH_MAX);
    if (realpath(buf.data(), resolved_name.data()) == nullptr) {
      return "";
    }
    return std::string(resolved_name.data());
#elif defined(WIN32)
    HMODULE hModule = GetModuleHandleA(nullptr);
    if (hModule == nullptr) {
      return "";
    }
    std::vector<char> buf(1024U);
    while (true) {
      size_t ret = GetModuleFileNameA(hModule, buf.data(), gsl::narrow<DWORD>(buf.size()));
      if (ret == 0U) {
        return "";
      }
      if (ret == buf.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        /* It has been truncated */
        buf.resize(buf.size() * 2);
        continue;
      }
      return std::string(buf.data());
    }
#else
    return "";
#endif
}

inline std::string resolve(const std::string& base, const std::string& path) {
  if (utils::file::isAbsolutePath(path.c_str())) {
    return path;
  }
  return concat_path(base, path);
}

/*
 * Returns the absolute path to the directory containing the current executable
 */
inline std::string get_executable_dir() {
  auto executable_path = get_executable_path();
  if (executable_path.empty()) {
    return "";
  }
  return get_parent_path(executable_path);
}

inline int close(int file_descriptor) {
#ifdef WIN32
  return _close(file_descriptor);
#else
  return ::close(file_descriptor);
#endif
}

inline int access(const char *path_name, int mode) {
#ifdef WIN32
  return _access(path_name, mode);
#else
  return ::access(path_name, mode);
#endif
}

#ifdef WIN32
inline std::error_code hide_file(const char* const file_name) {
    const bool success = SetFileAttributesA(file_name, FILE_ATTRIBUTE_HIDDEN);
    if (!success) {
      // note: All possible documented error codes from GetLastError are in [0;15999] at the time of writing.
      // The below casting is safe in [0;std::numeric_limits<int>::max()], int max is guaranteed to be at least 32767
      return { static_cast<int>(GetLastError()), std::system_category() };
    }
    return {};
  }
#endif /* WIN32 */

uint64_t computeChecksum(const std::string &file_name, uint64_t up_to_position);

inline std::string get_file_content(const std::string &file_name) {
  std::ifstream file(file_name);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return content;
}

}  // namespace file
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_UTILS_FILE_FILEUTILS_H_
