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
#ifndef LIBMINIFI_INCLUDE_UTILS_FILEUTILS_H_
#define LIBMINIFI_INCLUDE_UTILS_FILEUTILS_H_

#include <sstream>
#include <fstream>
#include <vector>
#ifdef BOOST_VERSION
#include <boost/filesystem.hpp>
#else
#include <cstring>
#include <cstdlib>
#include <errno.h>
#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <dirent.h>
#endif
#endif
#include <cstdio>
#ifndef WIN32
#include <unistd.h>
#endif
#include <fcntl.h>
#ifdef WIN32
#include <direct.h>
#include "utils/Id.h"
#include "properties/Properties.h"
#include <windows.h> // winapi
#include <sys/stat.h> // stat
#include <tchar.h> // _tcscpy,_tcscat,_tcscmp
#include <string> // string
#include <algorithm> // replace
#include <sys/types.h>
#include <sys/utime.h> // _utime64
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "core/logging/LoggerConfiguration.h"
#include "utils/StringUtils.h"

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif


namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {

/**
 * Simple implementation of some file system utilities.
 *
 */
class FileUtils {
 private:
  static inline int platform_create_dir(const std::string &path) {
#ifdef WIN32
    return _mkdir(path.c_str());
#else
    return mkdir(path.c_str(), 0700);
#endif
  }
 public:

  FileUtils() = delete;

  /*
   * Get the platform-specific path separator.
   * @param force_posix returns the posix path separator ('/'), even when not on posix. Useful when dealing with remote posix paths.
   * @return the path separator character
   */
  static char get_separator(bool force_posix = false)
  {
#ifdef WIN32
    if (force_posix) {
      return '/';
    } else {
      return '\\';
    }
#else
    return '/';
#endif
  }

  static std::string create_temp_directory(const char * const format) {
#ifdef WIN32
    std::string tempDirectory;
    char tempBuffer[MAX_PATH];
    auto ret = GetTempPath(MAX_PATH, tempBuffer);
    if (ret <= MAX_PATH && ret != 0)
    {
      static std::shared_ptr<minifi::utils::IdGenerator> generator;
      if (!generator) {
        generator = minifi::utils::IdGenerator::getIdGenerator();
        generator->initialize(std::make_shared<minifi::Properties>());
      }
      tempDirectory = tempBuffer;
      minifi::utils::Identifier id;
      generator->generate(id);
      tempDirectory += id.to_string();
      create_dir(tempDirectory);
    }
    return tempDirectory;
#else
    std::string mutable_format{ format };
    if (mutable_format.empty()) { return ""; }
    auto dir = mkdtemp(&mutable_format[0]);
    if (nullptr == dir) {
      return "";
    }
    return mutable_format;
#endif
  }

  static int64_t delete_dir(const std::string &path, bool delete_files_recursively = true) {
#ifdef BOOST_VERSION
    try {
      if (boost::filesystem::exists(path)) {
        if (delete_files_recursively) {
          boost::filesystem::remove_all(path);
        }
        else {
          boost::filesystem::remove(path);
        }
      }
    } catch(boost::filesystem::filesystem_error const & e)
    {
      return -1;
      //display error message
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
    //List files
    hFind = FindFirstFile(str.c_str(), &FindFileData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
      do {
        if (strcmp(FindFileData.cFileName, ".") != 0 && strcmp(FindFileData.cFileName, "..") != 0)
        {
          std::stringstream strs;
          strs << path << "\\" << FindFileData.cFileName;
          str = strs.str();

          Attributes = GetFileAttributes(str.c_str());
          if (Attributes & FILE_ATTRIBUTE_DIRECTORY)
          {
            //is directory
            delete_dir(str, delete_files_recursively);
          }
          else
          {
            remove(str.c_str());
            //not directory
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

  static uint64_t last_write_time(const std::string &path) {
#ifdef BOOST_VERSION
    return boost::filesystem::last_write_time(movedFile.str());
#else
#ifdef WIN32
    struct _stat result;
    if (_stat(path.c_str(), &result) == 0) {
      return result.st_mtime;
    }
#else
    struct stat result;
    if (stat(path.c_str(), &result) == 0) {
      return result.st_mtime;
    }
#endif
#endif
    return 0;
  }

  static bool set_last_write_time(const std::string &path, uint64_t write_time) {
#ifdef WIN32
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
  static bool get_permissions(const std::string &path, uint32_t &permissions) {
    struct stat result;
    if (stat(path.c_str(), &result) == 0) {
      permissions = result.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
      return true;
    }
    return false;
  }
#endif

#ifndef WIN32
  static bool get_uid_gid(const std::string &path, uint64_t &uid, uint64_t &gid) {
    struct stat result;
    if (stat(path.c_str(), &result) == 0) {
      uid = result.st_uid;
      gid = result.st_gid;
      return true;
    }
    return false;
  }
#endif

  static int is_directory(const char * path) {
      struct stat dir_stat;
      if (stat(path, &dir_stat) < 0) {
          return 0;
      }
      return S_ISDIR(dir_stat.st_mode);
  }

  static int create_dir(const std::string& path, bool recursive = true) {
#ifdef BOOST_VERSION
    boost::filesystem::path dir(path);
    if(boost::filesystem::create_directory(dir))
    {
      return 0;
    }
    else
    {
      return -1;
    }
#else
    if (!recursive) {
        if (platform_create_dir(path) != 0 && errno != EEXIST) {
            return -1;
        }
        return 0;
    }
    if (platform_create_dir(path) == 0) {
        return 0;
    }

    switch (errno) {
    case ENOENT: {
        size_t found = path.find_last_of(get_separator());

        if (found == std::string::npos) {
            return -1;
        }

        const std::string dir = path.substr(0, found);
        int res = create_dir(dir);
        if (res < 0) {
            return -1;
        }
        return platform_create_dir(path);
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

  static int copy_file(const std::string &path_from, const std::string dest_path) {
    std::ifstream src(path_from, std::ios::binary);
    if (!src.is_open())
      return -1;
    std::ofstream dest(dest_path, std::ios::binary);
    dest << src.rdbuf();
    return 0;
  }

  static void addFilesMatchingExtension(const std::shared_ptr<logging::Logger> &logger, const std::string &originalPath, const std::string &extension, std::vector<std::string> &accruedFiles) {
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
        struct _stat statbuf {};

        std::string path = originalPath + "\\" + FindFileData.cFileName;
        logger->log_info("Adding %s to paths", path);
        if (_stat(path.c_str(), &statbuf) != 0) {
          logger->log_warn("Failed to stat %s", path);
          break;
        }
        logger->log_info("Adding %s to paths", path);
        if (S_ISDIR(statbuf.st_mode)) {
          addFilesMatchingExtension(logger, path, extension, accruedFiles);
        }
        else {
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

  static std::vector<std::pair<std::string, std::string>> list_dir_all(const std::string& dir, const std::shared_ptr<logging::Logger> &logger,
                                                                       bool recursive = true)  {

    std::vector<std::pair<std::string, std::string>> fileList;
    auto lambda = [&fileList] (const std::string &path, const std::string &filename) {
      fileList.push_back(make_pair(path, filename));
      return true;
    };

    list_dir(dir, lambda, logger, recursive);

    return fileList;
  }

  /*
   * Provides a platform-independent function to list a directory
   * Callback is called for every file found: first argument is the path of the directory, second is the filename
   * Return value of the callback is used to continue (true) or stop (false) listing
   */
  static void list_dir(const std::string& dir, std::function<bool (const std::string&, const std::string&)> callback,
                       const std::shared_ptr<logging::Logger> &logger, bool recursive = true) {

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
      std::string path = dir + get_separator() + d_name;

      struct stat statbuf;
      if (stat(path.c_str(), &statbuf) != 0) {
        logger->log_warn("Failed to stat %s", path);
        continue;
      }

      if (S_ISDIR(statbuf.st_mode)) {
        // if this is a directory
        if (recursive && strcmp(d_name.c_str(), "..") != 0 && strcmp(d_name.c_str(), ".") != 0) {
          list_dir(path, callback, logger, recursive);
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

    if(hFind == INVALID_HANDLE_VALUE) {
      logger->log_warn("Failed to open directory: %s", dir.c_str());
      return;
    }

    do {
      struct _stat statbuf {};
      if (strcmp(FindFileData.cFileName, ".") != 0 && strcmp(FindFileData.cFileName, "..") != 0) {
        std::string path = dir + get_separator() + FindFileData.cFileName;
        if (_stat(path.c_str(), &statbuf) != 0) {
          logger->log_warn("Failed to stat %s", path);
          continue;
        }
        if (S_ISDIR(statbuf.st_mode)) {
          if (recursive) {
            list_dir(path, callback, logger, recursive);
          }
        }
        else {
          if (!callback(dir, FindFileData.cFileName)) {
            break;
          }
        }
      }
    } while (FindNextFileA(hFind, &FindFileData));
    FindClose(hFind);
#endif
  }

  static std::string concat_path(const std::string& root, const std::string& child, bool force_posix = false) {
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

  static std::tuple<std::string /*parent_path*/, std::string /*child_path*/> split_path(const std::string& path, bool force_posix = false) {
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

  static std::string get_parent_path(const std::string& path, bool force_posix = false) {
    std::string parent_path;
    std::tie(parent_path, std::ignore) = split_path(path, force_posix);
    return parent_path;
  }

  static std::string get_child_path(const std::string& path, bool force_posix = false) {
    std::string child_path;
    std::tie(std::ignore, child_path) = split_path(path, force_posix);
    return child_path;
  }

  static bool is_hidden(const std::string& path){
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
  static std::string get_executable_path() {
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
      size_t ret = GetModuleFileNameA(hModule, buf.data(), buf.size());
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

  /*
   * Returns the absolute path to the directory containing the current executable
   */
  static std::string get_executable_dir() {
    auto executable_path = get_executable_path();
    if (executable_path.empty()) {
      return "";
    }
    return get_parent_path(executable_path);
  }
};

} /* namespace file */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_UTILS_FILEUTILS_H_ */
