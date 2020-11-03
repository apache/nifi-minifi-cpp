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

#include "StringView.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

struct StringViewUtils {

  static inline bool equalsIgnoreCase(StringView left, StringView right) {
    if (left.length() != right.length()) {
      return false;
    }
    return std::equal(right.begin(), right.end(), left.begin(),
        [](unsigned char lc, unsigned char rc) {return tolower(lc) == tolower(rc);});
  }

  static inline StringView trimRight(StringView view) {
    return {view.begin(), std::find_if(view.rbegin(), view.rend(), [](char c) -> bool { return !isspace(c); }).base()};
  }

  static inline StringView trimLeft(StringView view) {
    return {std::find_if(view.begin(), view.end(), [] (char c) {return !isspace(c);}), view.end()};
  }

  static StringView trim(StringView view) {
    return trimRight(trimLeft(view));
  }

  static utils::optional<bool> toBool(StringView input) {
    StringView view = trim(input);
    if (equalsIgnoreCase(view, StringView("true"))) {
      return true;
    }
    if (equalsIgnoreCase(view, StringView("false"))) {
      return false;
    }
    return {};
  }

};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
