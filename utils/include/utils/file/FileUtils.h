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

#include <chrono>
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
#include <io.h>

#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#include <string>

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
#include "utils/expected.h"
#include "utils/file/PathUtils.h"
#include "utils/gsl.h"

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif


namespace org::apache::nifi::minifi::utils::file {

namespace FileUtils = ::org::apache::nifi::minifi::utils::file;

std::chrono::system_clock::time_point to_sys(std::chrono::file_clock::time_point file_time);

std::chrono::file_clock::time_point from_sys(std::chrono::system_clock::time_point sys_time);

inline int64_t delete_dir(const std::filesystem::path& path, bool delete_files_recursively = true) {
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
  } catch (const std::filesystem::filesystem_error&) {
    return -1;
    // display error message
  }
  return 0;
}

inline std::chrono::time_point<std::chrono::file_clock,
    std::chrono::seconds> last_write_time_point(const std::filesystem::path& path) {
  std::error_code ec;
  auto result = std::filesystem::last_write_time(path, ec);
  if (ec.value() == 0) {
    return std::chrono::time_point_cast<std::chrono::seconds>(result);
  }
  return std::chrono::time_point<std::chrono::file_clock, std::chrono::seconds>{};
}

inline std::optional<std::chrono::file_clock::time_point> last_write_time(const std::filesystem::path& path) {
  std::error_code ec;
  auto result = std::filesystem::last_write_time(path, ec);
  if (ec.value() == 0) {
    return result;
  }
  return std::nullopt;
}

inline nonstd::expected<void, std::error_code> set_last_write_time(const std::filesystem::path& path, std::chrono::file_clock::time_point new_time) {
  std::error_code ec;
  std::filesystem::last_write_time(path, new_time, ec);
  if (ec)
    return nonstd::make_unexpected(ec);
  return {};
}

inline uint64_t file_size(const std::filesystem::path& path) {
  std::error_code ec;
  auto file_size = std::filesystem::file_size(path, ec);
  if (ec.value() != 0)
    return 0;
  return file_size;
}

inline bool get_permissions(const std::filesystem::path& path, uint32_t& permissions) {
  std::error_code ec;
  permissions = static_cast<uint32_t>(std::filesystem::status(path, ec).permissions());
  return ec.value() == 0;
}

inline int set_permissions(const std::filesystem::path& path, const uint32_t permissions) {
  std::error_code ec;
  std::filesystem::permissions(path, static_cast<std::filesystem::perms>(permissions), ec);
  return ec.value();
}

inline std::optional<std::string> get_permission_string(const std::filesystem::path& path) {
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
  struct stat result = {};
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

inline uint64_t path_size(const std::filesystem::path& path) {
  uint64_t size = 0;
  if (std::filesystem::is_regular_file(path)) {
    return utils::file::file_size(path);
  } else if (utils::file::is_directory(path)) {
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(path, std::filesystem::directory_options::skip_permission_denied)) {
      if (entry.is_regular_file()) {
        size += entry.file_size();
      }
    }
  }
  return size;
}

inline bool exists(const std::filesystem::path &path) {
  std::error_code ec;
  bool result = std::filesystem::exists(path, ec);
  if (ec.value() == 0) {
    return result;
  }
  return false;
}

inline int create_dir(const std::filesystem::path& path, bool recursive = true) {
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

inline int copy_file(const std::filesystem::path& path_from, const std::filesystem::path& dest_path) {
  std::error_code ec;
  auto copy_success = std::filesystem::copy_file(path_from, dest_path, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec.value() != 0 || !copy_success)
    return -1;
  return 0;
}

inline void addFilesMatchingExtension(const std::shared_ptr<core::logging::Logger> &logger,
                                      const std::filesystem::path& originalPath,
                                      const std::filesystem::path& extension,
                                      std::vector<std::filesystem::path>& accruedFiles) {
  if (!utils::file::exists(originalPath)) {
    logger->log_warn("Failed to open directory: {}", originalPath);
    return;
  }

  if (utils::file::is_directory(originalPath)) {
    // only perform a listing while we are not empty
    logger->log_debug("Looking for files with {} extension in {}", extension, originalPath);

    for (const auto& entry: std::filesystem::directory_iterator(originalPath,
                                                                std::filesystem::directory_options::skip_permission_denied)) {
      if (utils::file::is_directory(entry.path())) {          // if this is a directory
        addFilesMatchingExtension(logger, entry.path(), extension, accruedFiles);
      } else {
        if (entry.path().extension() == extension) {
          logger->log_info("Adding {} to paths", entry.path());
          accruedFiles.push_back(entry.path());
        }
      }
    }
  } else if (std::filesystem::is_regular_file(originalPath)) {
    if (originalPath.extension() == extension) {
      logger->log_info("Adding {} to paths", originalPath);
      accruedFiles.push_back(originalPath);
    }
  } else {
    logger->log_error("Could not access {}", originalPath);
  }
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
inline void list_dir(const std::filesystem::path& dir,
                     const std::function<bool(const std::filesystem::path&, const std::filesystem::path&)>& callback,
                     const std::shared_ptr<core::logging::Logger> &logger,
                     const std::function<bool(const std::filesystem::path&)>& dir_callback) {
  logger->log_debug("Performing file listing against {}", dir);
  if (!utils::file::exists(dir)) {
    logger->log_warn("Failed to open directory: {}", dir);
    return;
  }

  for (const auto &entry: std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied)) {
    auto d_name = entry.path().filename();
    auto path = entry.path();

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

inline void list_dir(const std::filesystem::path& dir,
                     const std::function<bool(const std::filesystem::path&, const std::filesystem::path&)>& callback,
                     const std::shared_ptr<core::logging::Logger> &logger,
                     bool recursive = true) {
  list_dir(dir, callback, logger, [&] (const std::filesystem::path&) {
    return recursive;
  });
}

inline std::vector<std::pair<std::filesystem::path, std::filesystem::path>> list_dir_all(const std::filesystem::path& dir, const std::shared_ptr<core::logging::Logger> &logger,
    bool recursive = true)  {
  std::vector<std::pair<std::filesystem::path, std::filesystem::path>> fileList;
  auto lambda = [&fileList] (const std::filesystem::path& parent_path, const std::filesystem::path& filename) {
    fileList.emplace_back(parent_path, filename);
    return true;
  };

  list_dir(dir, lambda, logger, recursive);

  return fileList;
}

inline std::filesystem::path create_temp_directory(char* format) {
#ifdef WIN32
  const auto tempDirectory = std::filesystem::temp_directory_path() / minifi::utils::IdGenerator::getIdGenerator()->generate().to_string().view();
  create_dir(tempDirectory);
  return tempDirectory;
#else
  if (mkdtemp(format) == nullptr) { return ""; }
  return format;
#endif
}

inline bool is_hidden(const std::filesystem::path& path) {
#ifdef WIN32
  DWORD attributes = GetFileAttributesA(path.string().c_str());
  return ((attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_HIDDEN) != 0));
#else
  return path.filename().string().starts_with('.');
#endif
}

/*
 * Returns the absolute path of the current executable
 */
inline std::filesystem::path get_executable_path() {
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

inline std::filesystem::path resolve(const std::filesystem::path& base, const std::filesystem::path& path) {
  if (path.is_absolute()) {
    return path;
  }
  return base / path;
}

/*
 * Returns the absolute path to the directory containing the current executable
 */
inline std::filesystem::path get_executable_dir() {
  auto executable_path = get_executable_path();
  if (executable_path.empty()) {
    return "";
  }
  return executable_path.parent_path();
}

uint64_t computeChecksum(const std::filesystem::path& file_name, uint64_t up_to_position);

inline std::string get_content(const std::filesystem::path& file_name) {
  std::ifstream file(file_name, std::ifstream::binary);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return content;
}

bool contains(const std::filesystem::path& file_path, std::string_view text_to_search);


inline std::optional<std::string> get_file_owner(const std::filesystem::path& file_path) {
#ifndef WIN32
  struct stat info = {};
  if (stat(file_path.c_str(), &info) != 0) {
    return std::nullopt;
  }

  struct passwd pw = {};
  pw.pw_name = nullptr;
  struct passwd *result = nullptr;
  char localbuf[1024] = {};
  if (getpwuid_r(info.st_uid, &pw, localbuf, sizeof(localbuf), &result) != 0 || pw.pw_name == nullptr) {
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
    TEXT(file_path.string().c_str()),
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

  auto close_file_handle = gsl::finally([&file_handle] { CloseHandle(file_handle); });

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
inline std::optional<std::string> get_file_group(const std::filesystem::path& file_path) {
  struct stat info = {};
  if (stat(file_path.c_str(), &info) != 0) {
    return std::nullopt;
  }

  struct group gr = {};
  gr.gr_name = nullptr;
  struct group *result = nullptr;
  char localbuf[1024] = {};
  if ((getgrgid_r(info.st_uid, &gr, localbuf, sizeof(localbuf), &result) != 0) || gr.gr_name == nullptr) {
    return std::nullopt;
  }

  return std::string(gr.gr_name);
}
#endif

inline std::optional<std::filesystem::path> get_relative_path(const std::filesystem::path& path, const std::filesystem::path& base_path) {
  if (!utils::string::startsWith(path.string(), base_path.string())) {
    return std::nullopt;
  }

  return std::filesystem::relative(path, base_path);
}

inline size_t countNumberOfFiles(const std::filesystem::path& path) {
  using std::filesystem::directory_iterator;
  return std::count_if(directory_iterator(path), directory_iterator(), [](const auto& entry) { return entry.is_regular_file(); });
}

#ifdef WIN32
struct WindowsFileTimes {
  std::chrono::file_clock::time_point creation_time;
  std::chrono::file_clock::time_point last_access_time;
  std::chrono::file_clock::time_point last_write_time;
};

nonstd::expected<WindowsFileTimes, std::error_code> getWindowsFileTimes(const std::filesystem::path& path);
#endif

}  // namespace org::apache::nifi::minifi::utils::file
