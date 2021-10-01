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

#include <string>
#include "Export.h"

namespace org::apache::nifi::minifi::utils {
class TextFragmentUtils {
 public:
  MINIFIAPI static const char *BASE_NAME_ATTRIBUTE;
  MINIFIAPI static const char *POST_NAME_ATTRIBUTE;
  MINIFIAPI static const char *OFFSET_ATTRIBUTE;

  static std::string createFileName(const std::string& base_name, const std::string& post_name, const size_t offset, const size_t size) {
    return base_name + "." + std::to_string(offset) + "-" + std::to_string(offset + size - 1) + "." + post_name;
  }
};
}  // namespace org::apache::nifi::minifi::utils
