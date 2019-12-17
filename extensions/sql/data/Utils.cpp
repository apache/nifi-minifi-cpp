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

#include "Utils.h"

#include <algorithm>
#include  <cctype>
#include  <regex>
#include  <sstream>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

std::string toLower(const std::string& str) {
  std::string ret;

  // (int(*)(int))std::tolower - to avoid compilation error.
  std::transform(str.begin(), str.end(), std::back_inserter(ret), (int(*)(int))std::tolower);

  return ret;
}

std::vector<std::string> inputStringToList(const std::string& str) {
  std::vector<std::string> ret;

  std::string token;
  // Convert to lower and remove white characters.
  std::istringstream tokenStream(std::regex_replace(toLower(str), std::regex("\\s"), std::string("")));

  while (std::getline(tokenStream, token, ','))
  {
    ret.push_back(token);
  }

  return ret;
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
