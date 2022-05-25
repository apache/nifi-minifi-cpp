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

#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>
#include <cstdio>
#include <algorithm>

#ifndef WIN32
#include <unistd.h>
#include <sys/stat.h> //NOLINT
#include <pwd.h>
#include <grp.h>

#endif

#include <fcntl.h>

#ifdef WIN32
#include <stdio.h>
#include <direct.h>
#include <sys/stat.h>  // stat // NOLINT
#include <sys/types.h> // NOLINT
#include <sys/utime.h>  // _utime64
#include <tchar.h>  // _tcscpy,_tcscat,_tcscmp
#include <windows.h>  // winapi

#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#include <string>

#include "properties/Properties.h"
#include "utils/Id.h"

#include "accctrl.h"
#include "aclapi.h"
#pragma comment(lib, "advapi32.lib")

#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>

#endif

#include "core/logging/LoggerFactory.h"
#include "utils/StringUtils.h"
#include "utils/file/PathUtils.h"
#include "utils/gsl.h"

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif


namespace org::apache::nifi::minifi::utils::file {

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
time_t to_time_t(std::filesystem::file_time_type time);

std::chrono::system_clock::time_point to_sys(std::filesystem::file_time_type time);

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
  try {
    if (std::filesystem::exists(path)) {
      if (delete_files_recursively) {
        std::filesystem::remove_all(path);
      } else {
        std::filesystem::remove(path);
      }
    }
  } catch (std::filesystem::filesystem_error const &) {
    return -1;
    // display error message
  }
  return 0;
}

inline std::chrono::time_point<std::chrono::file_clock,
    std::chrono::seconds> last_write_time_point(const std::string &path) {
  std::error_code ec;
  auto result = std::filesystem::last_write_time(path, ec);
  if (ec.value() == 0) {
    return std::chrono::time_point_cast<std::chrono::seconds>(result);
  }
  return std::chrono::time_point<std::chrono::file_clock, std::chrono::seconds>{};
}

inline std::optional<std::filesystem::file_time_type> last_write_time(const std::string &path) {
  std::error_code ec;
  auto result = std::filesystem::last_write_time(path, ec);
  if (ec.value() == 0) {
    return result;
  }
  return std::nullopt;
}

inline std::optional<std::string> format_time(const std::filesystem::file_time_type& time, const std::string& format) {
  auto last_write_time_t = to_time_t(time);
  std::array<char, 128U> result;
  if (std::strftime(result.data(), result.size(), format.c_str(), gmtime(&last_write_time_t)) != 0) {
    return std::string(result.data());
  }
  return std::nullopt;
}

inline std::optional<std::string> get_last_modified_time_formatted_string(const std::string& path, const std::string& format_string) {
  return last_write_time(path) | utils::flatMap([format_string](auto time) { return format_time(time, format_string); });
}

inline bool set_last_write_time(const std::string &path, std::filesystem::file_time_type new_time) {
  std::error_code ec;
  std::filesystem::last_write_time(path, new_time, ec);
  return ec.value() == 0;
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

inline bool get_permissions(const std::string &path, uint32_t &permissions) {
  std::error_code ec;
  permissions = static_cast<uint32_t>(std::filesystem::status(path, ec).permissions());
  return ec.value() == 0;
}

inline int set_permissions(const std::string &path, const uint32_t permissions) {
  std::error_code ec;
  std::filesystem::permissions(path, static_cast<std::filesystem::perms>(permissions), ec);
  return ec.value();
}

inline std::optional<std::string> get_permission_string(const std::string &path) {
  std::error_code ec;
  auto permissions = std::filesystem::status(path, ec).permissions();
  if (ec.value() != 0) {
    return std::nullopt;
  }

  std::string permission_string;
  permission_string += (permissions & std::filesystem::perms::owner_read) != std::filesystem::perms::none ? "r" : "-";
  permission_string += (permissions & std::filesystem::perms::owner_write) != std::filesystem::perms::none ? "w" : "-";
  permission_string += (permissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none ? "x" : "-";
  permission_string += (permissions & std::filesystem::perms::group_read) != std::filesystem::perms::none ? "r" : "-";
  permission_string += (permissions & std::filesystem::perms::group_write) != std::filesystem::perms::none ? "w" : "-";
  permission_string += (permissions & std::filesystem::perms::group_exec) != std::filesystem::perms::none ? "x" : "-";
  permission_string += (permissions & std::filesystem::perms::others_read) != std::filesystem::perms::none ? "r" : "-";
  permission_string += (permissions & std::filesystem::perms::others_write) != std::filesystem::perms::none ? "w" : "-";
  permission_string += (permissions & std::filesystem::perms::others_exec) != std::filesystem::perms::none ? "x" : "-";
  return permission_string;
}

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

inline bool is_directory(const std::filesystem::path &path) {
  std::error_code ec;
  bool result = std::filesystem::is_directory(path, ec);
  if (ec.value() == 0) {
    return result;
  }
  return false;
}

inline bool exists(const std::string &path) {
  std::error_code ec;
  bool result = std::filesystem::exists(path, ec);
  if (ec.value() == 0) {
    return result;
  }
  return false;
}

inline int create_dir(const std::string &path, bool recursive = true) {
  std::filesystem::path dir(path);
  std::error_code ec;
  if (!recursive) {
    std::filesystem::create_directory(dir, ec);
  } else {
    std::filesystem::create_directories(dir, ec);
  }
  if (ec.value() == 0 || (ec.value() == EEXIST && is_directory(path.c_str()))) {
    return 0;
  }
  return ec.value();
}

inline int copy_file(const std::string &path_from, const std::string& dest_path) {
  std::ifstream src(path_from, std::ios::binary);
  if (!src.is_open())
    return -1;
  std::ofstream dest(dest_path, std::ios::binary);
  dest << src.rdbuf();
  return 0;
}

inline void addFilesMatchingExtension(const std::shared_ptr<core::logging::Logger> &logger,
                                      const std::string &originalPath,
                                      const std::string &extension,
                                      std::vector<std::string> &accruedFiles) {
  if (!utils::file::exists(originalPath)) {
    logger->log_warn("Failed to open directory: %s", originalPath.c_str());
    return;
  }

  if (utils::file::is_directory(originalPath)) {
    // only perform a listing while we are not empty
    logger->log_debug("Looking for files with %s extension in %s", extension, originalPath);

    for (const auto &entry: std::filesystem::directory_iterator(originalPath,
                                                                std::filesystem::directory_options::skip_permission_denied)) {
      std::string d_name = entry.path().filename().string();
      std::string path = entry.path().string();

      if (utils::file::is_directory(path)) {          // if this is a directory
        addFilesMatchingExtension(logger, path, extension, accruedFiles);
      } else {
        if (utils::StringUtils::endsWith(path, extension)) {
          logger->log_info("Adding %s to paths", path);
          accruedFiles.push_back(path);
        }
      }
    }
  } else if (std::filesystem::is_regular_file(originalPath)) {
    if (utils::StringUtils::endsWith(originalPath, extension)) {
      logger->log_info("Adding %s to paths", originalPath);
      accruedFiles.push_back(originalPath);
    }
  } else {
    logger->log_error("Could not access %s", originalPath);
  }
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

/**
 * Provides a platform-independent function to list a directory
 * @param dir The directory to start the enumeration from.
 * @param callback Callback is called for every file found: first argument is the path of the directory, second is the filename.
 * Return value of the callback is used to continue (true) or stop (false) listing
 * @param logger
 * @param dir_callback Called for every child directory, its return value decides if we should descend and recursively
 * process that directory or not.
 */
inline void list_dir(const std::string& dir,
                     std::function<bool(const std::string&, const std::string&)> callback,
                     const std::shared_ptr<core::logging::Logger> &logger,
                     std::function<bool(const std::string&)> dir_callback) {
  logger->log_debug("Performing file listing against %s", dir);
  if (!utils::file::exists(dir)) {
    logger->log_warn("Failed to open directory: %s", dir.c_str());
    return;
  }

  for (const auto &entry: std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied)) {
    std::string d_name = entry.path().filename().string();
    std::string path = entry.path().string();

    if (utils::file::is_directory(path)) {  // if this is a directory
      if (dir_callback(path)) {
        list_dir(path, callback, logger, dir_callback);
      }
    } else {
      if (!callback(dir, d_name)) {
        break;
      }
    }
  }
}

inline void list_dir(const std::string& dir,
                     std::function<bool(const std::string&, const std::string&)> callback,
                     const std::shared_ptr<core::logging::Logger> &logger,
                     bool recursive = true) {
  list_dir(dir, callback, logger, [&] (const std::string&) {
    return recursive;
  });
}

inline std::vector<std::pair<std::string, std::string>> list_dir_all(const std::string& dir, const std::shared_ptr<core::logging::Logger> &logger,
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

inline std::string get_content(const std::string &file_name) {
  std::ifstream file(file_name, std::ifstream::binary);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return content;
}

void put_content(const std::filesystem::path& filename, std::string_view new_contents);

bool contains(const std::filesystem::path& file_path, std::string_view text_to_search);


inline std::optional<std::string> get_file_owner(const std::string& file_path) {
#ifndef WIN32
  struct stat info;
  if (stat(file_path.c_str(), &info) != 0) {
    return std::nullopt;
  }

  struct passwd pw;
  pw.pw_name = 0;
  struct passwd *result = nullptr;
  char localbuf[1024] = {};
  if (getpwuid_r(info.st_uid, &pw, localbuf, sizeof(localbuf), &result) != 0 || pw.pw_name == 0) {
    return std::nullopt;
  }

  return std::string(pw.pw_name);
#else
  DWORD return_code = 0;
  PSID sid_owner = NULL;
  BOOL bool_return = TRUE;
  LPTSTR account_name = NULL;
  LPTSTR domain_name = NULL;
  DWORD account_name_dword = 1;
  DWORD domain_name_dword = 1;
  SID_NAME_USE sid_type = SidTypeUnknown;
  HANDLE file_handle;
  PSECURITY_DESCRIPTOR sec_descriptor = NULL;

  // Get the handle of the file object.
  file_handle = CreateFile(
    TEXT(file_path.c_str()),
    GENERIC_READ,
    FILE_SHARE_READ,
    NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL);

  // Check GetLastError for CreateFile error code.
  if (file_handle == INVALID_HANDLE_VALUE) {
    return std::nullopt;
  }

  // Get the owner SID of the file.
  return_code = GetSecurityInfo(
    file_handle,
    SE_FILE_OBJECT,
    OWNER_SECURITY_INFORMATION,
    &sid_owner,
    NULL,
    NULL,
    NULL,
    &sec_descriptor);

  // Check GetLastError for GetSecurityInfo error condition.
  if (return_code != ERROR_SUCCESS) {
    return std::nullopt;
  }

  // First call to LookupAccountSid to get the buffer sizes.
  bool_return = LookupAccountSid(
    NULL,
    sid_owner,
    account_name,
    (LPDWORD)&account_name_dword,
    domain_name,
    (LPDWORD)&domain_name_dword,
    &sid_type);

  // Reallocate memory for the buffers.
  account_name = (LPTSTR)GlobalAlloc(
    GMEM_FIXED,
    account_name_dword);

  // Check GetLastError for GlobalAlloc error condition.
  if (account_name == NULL) {
    return std::nullopt;
  }
  auto cleanup_account_name = gsl::finally([&account_name] { GlobalFree(account_name); });

  domain_name = (LPTSTR)GlobalAlloc(
    GMEM_FIXED,
    domain_name_dword);

  // Check GetLastError for GlobalAlloc error condition.
  if (domain_name == NULL) {
    return std::nullopt;
  }
  auto cleanup_domain_name = gsl::finally([&domain_name] { GlobalFree(domain_name); });

  // Second call to LookupAccountSid to get the account name.
  bool_return = LookupAccountSid(
    NULL,                   // name of local or remote computer
    sid_owner,              // security identifier
    account_name,               // account name buffer
    (LPDWORD)&account_name_dword,   // size of account name buffer
    domain_name,             // domain name
    (LPDWORD)&domain_name_dword,  // size of domain name buffer
    &sid_type);                 // SID type

  // Check GetLastError for LookupAccountSid error condition.
  if (bool_return == FALSE) {
    return std::nullopt;
  }

  auto result = std::string(account_name);
  return result;
#endif
}

#ifndef WIN32
inline std::optional<std::string> get_file_group(const std::string& file_path) {
  struct stat info;
  if (stat(file_path.c_str(), &info) != 0) {
    return std::nullopt;
  }

  struct group gr;
  gr.gr_name = 0;
  struct group *result = nullptr;
  char localbuf[1024] = {};
  if ((getgrgid_r(info.st_uid, &gr, localbuf, sizeof(localbuf), &result) != 0) || gr.gr_name == 0) {
    return std::nullopt;
  }

  return std::string(gr.gr_name);
}
#endif

inline std::optional<std::string> get_relative_path(const std::string& path, const std::string& base_path) {
  if (!utils::StringUtils::startsWith(path, base_path)) {
    return std::nullopt;
  }

  return std::filesystem::relative(path, base_path).string();
}

}  // namespace org::apache::nifi::minifi::utils::file
