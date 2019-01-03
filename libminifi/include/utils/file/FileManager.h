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
#ifndef LIBMINIFI_INCLUDE_UTILS_FILEMANAGER_H_
#define LIBMINIFI_INCLUDE_UTILS_FILEMANAGER_H_

#ifdef BOOST_VERSION
#include <boost/filesystem.hpp>
#else
#include <cstdlib>
#endif
#include <cstdio>
#include <fcntl.h>
#include "io/validation.h"
#include "utils/Id.h"
#include "utils/StringUtils.h"
#ifdef WIN32
#define stat _stat
#endif

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

  FileManager() {
  }

  ~FileManager() {
    for (auto file : unique_files_) {
      unlink(file.c_str());
    }
  }
  std::string unique_file(const std::string &location, bool keep = false) {

	 
    if (!IsNullOrEmpty(location)) {
      std::string file_name = location + FILE_SEPARATOR + non_repeating_string_generator_.generate();
      while (!verify_not_exist(file_name)) {
        file_name = location + FILE_SEPARATOR + non_repeating_string_generator_.generate();
      }
      if (!keep)
        unique_files_.push_back(file_name);
      return file_name;
    } else {
	  std::string tmpDir = "/tmp";
	  #ifdef WIN32
			TCHAR lpTempPathBuffer[MAX_PATH];
			GetTempPath(MAX_PATH, lpTempPathBuffer);
			tmpDir = lpTempPathBuffer;
	  #endif
      std::string file_name = tmpDir + FILE_SEPARATOR + non_repeating_string_generator_.generate();
      while (!verify_not_exist(file_name)) {
        file_name = tmpDir + FILE_SEPARATOR + non_repeating_string_generator_.generate();
      }
      if (!keep)
        unique_files_.push_back(file_name);
      return file_name;
    }
  }

  std::string unique_file(bool keep = false) {
#ifdef BOOST_VERSION
    return boost::filesystem::unique_path().native();
#else
	  std::string tmpDir = "/tmp";
	#ifdef WIN32
		  TCHAR lpTempPathBuffer[MAX_PATH];
		  GetTempPath(MAX_PATH, lpTempPathBuffer);
		  tmpDir = lpTempPathBuffer;
	#endif
    std::string file_name = tmpDir + FILE_SEPARATOR + non_repeating_string_generator_.generate();
    while (!verify_not_exist(file_name)) {
      file_name = tmpDir + FILE_SEPARATOR + non_repeating_string_generator_.generate();
    }
    if (!keep)
      unique_files_.push_back(file_name);
    return file_name;
#endif
  }


 protected:

  inline bool verify_not_exist(const std::string& name) {
    struct stat buffer;
    return (stat(name.c_str(), &buffer) != 0);
  }

  utils::NonRepeatingStringGenerator non_repeating_string_generator_;

  std::vector<std::string> unique_files_;
};

} /* namespace file */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_UTILS_FILEMANAGER_H_ */
