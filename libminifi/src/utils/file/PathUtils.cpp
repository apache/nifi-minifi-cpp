/**
 *
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

#include "utils/file/PathUtils.h"
#include "utils/file/FileUtils.h"

#include <iostream>
#ifdef WIN32
#include <Windows.h>
#else
#include <limits.h>
#include <stdlib.h>
#endif

namespace {
  std::string replaceOne(const std::string &input, const std::string &from, const std::string &to) {
    std::size_t found_at_position = input.find(from);
    if (found_at_position != std::string::npos) {
      std::string input_copy = input;
      return input_copy.replace(found_at_position, from.size(), to);
    } else {
      return input;
    }
  }

  void replaceAll(std::string &input, const std::string &from, const std::string &to) {
    std::size_t position = 0;
    while (position < input.size()) {
      position = input.find(from, position);
      if (position == std::string::npos) {
        return;
      }
      input.replace(position, from.size(), to);
      position += to.size();
    }
  }
}  // namespace

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {

bool PathUtils::getFileNameAndPath(const std::string &path, std::string &filePath, std::string &fileName) {
  const std::size_t found = path.find_last_of(FileUtils::get_separator());
  /**
   * Don't make an assumption about about the path, return false for this case.
   * Could make the assumption that the path is just the file name but with the function named
   * getFileNameAndPath we expect both to be there ( a fully qualified path ).
   *
   */
  if (found == std::string::npos || found == path.length() - 1) {
    return false;
  }
  if (found == 0) {
    filePath = "";  // don't assume that path is not empty
    filePath += FileUtils::get_separator();
    fileName = path.substr(found + 1);
    return true;
  }
  filePath = path.substr(0, found);
  fileName = path.substr(found + 1);
  return true;
}

std::string PathUtils::getFullPath(const std::string& path) {
#ifdef WIN32
  std::vector<char> buffer(MAX_PATH);
  uint32_t len = 0U;
  while (true) {
    len = GetFullPathNameA(path.c_str(), buffer.size(), buffer.data(), nullptr /*lpFilePart*/);
    if (len < buffer.size()) {
      break;
    }
    buffer.resize(len);
  }
  if (len > 0U) {
    return std::string(buffer.data(), len);
  } else {
    return "";
  }
#else
  std::vector<char> buffer(PATH_MAX);
  char* res = realpath(path.c_str(), buffer.data());
  if (res == nullptr) {
    return "";
  } else {
    return res;
  }
#endif
}

  std::string PathUtils::globToRegex(const std::string &glob) {
    std::string glob_copy = glob;
    replaceAll(glob_copy, ".", "\\.");
    replaceAll(glob_copy, "*", ".*");
    replaceAll(glob_copy, "?", ".");
    return glob_copy;
  }

  std::string PathUtils::replacePlaceholderWithBaseName(const std::string &pattern, const std::string& base_name) {
    static const std::string PLACEHOLDER = "${filename}";
    return replaceOne(pattern, PLACEHOLDER, base_name);
  }

} /* namespace file */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

