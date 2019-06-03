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

} /* namespace file */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

