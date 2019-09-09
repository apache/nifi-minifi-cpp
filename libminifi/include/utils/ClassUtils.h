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
#ifndef LIBMINIFI_INCLUDE_UTILS_CLASSUTILS_H_
#define LIBMINIFI_INCLUDE_UTILS_CLASSUTILS_H_

#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace ClassUtils {

/**
 * Shortens class names via the canonical representation ( package with name )
 * @param class_name input class name
 * @param out output class name that is shortened.
 * @return true if out has been updated, false otherwise
 */
bool shortenClassName(const std::string &class_name, std::string &out);

} /* namespace ClassUtils */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_UTILS_CLASSUTILS_H_ */
