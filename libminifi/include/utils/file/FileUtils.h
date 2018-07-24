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
#include <cstring>
#ifdef BOOST_VERSION
#include <boost/filesystem.hpp>
#else
#include <cstdlib>
#include <sys/stat.h>
#include <dirent.h>
#endif
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#ifdef WIN32
#define stat _stat
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
 public:

  FileUtils() = delete;

  /**
   * Deletes a directory, deleting recursively if delete_files_recursively is true
   * @param path current path to delete
   * @param delete_files_recursively deletes recursively
   */
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
    struct stat result;
    if (stat(path.c_str(), &result) == 0) {
      return result.st_mtime;
    }
#endif
    return 0;
  }

  static int create_dir(const std::string &path, bool create = true) {
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
    struct stat dir_stat;
    if (stat(path.c_str(), &dir_stat)) {
      mkdir(path.c_str(), 0700);
    }
    return 0;
#endif
    return -1;
  }

  static int copy_file(const std::string &path_from, const std::string dest_path) {
    std::ifstream src(path_from, std::ios::binary);
    if (!src.is_open())
      return -1;
    std::ofstream dest(dest_path, std::ios::binary);
    dest << src.rdbuf();
    return 0;
  }

};

} /* namespace file */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_UTILS_FILEUTILS_H_ */
