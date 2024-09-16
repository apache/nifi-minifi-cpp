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

#include "utils/ClassUtils.h"

#include <iostream>
#include <string>

#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils {

bool ClassUtils::shortenClassName(std::string_view class_name, std::string &out) {
  std::string class_delim = "::";
  auto class_split = utils::string::split(class_name, class_delim);
  // support . and ::
  if (class_split.size() <= 1) {
    if (class_name.find('.') != std::string::npos) {
      class_delim = ".";
      class_split = utils::string::split(class_name, class_delim);
    } else {
      // if no update can be performed, return false to let the developer know
      // this. Out will have no updates
      return false;
    }
  }
  for (auto &elem : class_split) {
    if (&elem != &class_split.back() && elem.size() > 1) {
      elem = elem.substr(0, 1);
    }
  }

  out = utils::string::join(class_delim, class_split);
  return true;
}

}  // namespace org::apache::nifi::minifi::utils
