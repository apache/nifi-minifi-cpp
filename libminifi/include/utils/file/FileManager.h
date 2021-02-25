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
#ifndef LIBMINIFI_INCLUDE_UTILS_FILE_FILEMANAGER_H_
#define LIBMINIFI_INCLUDE_UTILS_FILE_FILEMANAGER_H_

#include <string>
#include <vector>

#ifdef USE_BOOST
#include <boost/filesystem.hpp>

#else
#include <cstdlib>

#endif
#include <fcntl.h>

#include <cstdio>

#include "io/validation.h"
#include "utils/Id.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"

#ifndef FILE_SEPARATOR
  #ifdef WIN32
  #define FILE_SEPARATOR "\\"
  #else
  #define FILE_SEPARATOR "/"
  #endif
#endif


namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {

/**
 * Simple implementation of simple file manager utilities.
 *
 * unique_file is not a static implementation so that we can support scope driven temporary files.
 */
class FileManager {
 public:
  FileManager() = default;

  ~FileManager() {
    for (auto file : unique_files_) {
      std::remove(file.c_str());
    }
  }
  std::string unique_file(const std::string &location, bool keep = false) {
    const std::string& dir = !IsNullOrEmpty(location) ? location : utils::file::FileUtils::get_temp_directory();

    std::string file_name = utils::file::FileUtils::concat_path(dir, non_repeating_string_generator_.generate());
    while (!verify_not_exist(file_name)) {
      file_name = utils::file::FileUtils::concat_path(dir, non_repeating_string_generator_.generate());
    }
    if (!keep)
      unique_files_.push_back(file_name);
    return file_name;
  }

  std::string unique_file(bool keep = false) {
#ifdef USE_BOOST
    (void)keep;  // against unused variable warnings
    return boost::filesystem::unique_path().native();
#else  // USE_BOOST
    return unique_file(std::string{}, keep);
#endif  // USE_BOOST
  }


 protected:
  inline bool verify_not_exist(const std::string& name) {
#ifdef WIN32
    struct _stat buffer;
    return _stat(name.c_str(), &buffer) != 0;
#else
    struct stat buffer;
    return stat(name.c_str(), &buffer) != 0;
#endif
  }

  utils::NonRepeatingStringGenerator non_repeating_string_generator_;

  std::vector<std::string> unique_files_;
};

}  // namespace file
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_UTILS_FILE_FILEMANAGER_H_
