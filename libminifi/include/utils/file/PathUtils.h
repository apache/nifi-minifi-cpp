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
#ifndef LIBMINIFI_INCLUDE_UTILS_PATHUTILS_H_
#define LIBMINIFI_INCLUDE_UTILS_PATHUTILS_H_

#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {
namespace PathUtils {

/**
 * Extracts the filename and path performing some validation of the path and output to ensure
 * we don't provide invalid results.
 * @param path input path
 * @param filePath output file path
 * @param fileName output file name
 * @return result of the operation.
 */
extern bool getFileNameAndPath(const std::string &path, std::string &filePath, std::string &fileName);

} /* namespace PathUtils */
} /* namespace file */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_UTILS_PATHUTILS_H_ */
