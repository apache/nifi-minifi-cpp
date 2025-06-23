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

#pragma once

#include <string>
#include <string_view>
#include <optional>

#include "utils/RegexUtils.h"
#include "utils/net/DNS.h"

namespace org::apache::nifi::minifi::utils {

/**
This function, unfortunately, assumes that we're parsing http components of a local host. On windows this is problematic
so we convert localhost to our local hostname.
  */
inline bool parse_http_components(const std::string &url, std::string &port, std::string &scheme, std::string &path) {
#ifdef WIN32
  auto hostname = (url.find(org::apache::nifi::minifi::utils::net::getMyHostName()) != std::string::npos ? org::apache::nifi::minifi::utils::net::getMyHostName() : "localhost");
  std::string regex_str = "(http|https)://(" + hostname + ":)([0-9]+)?(/.*)";
#else
  std::string regex_str = "(http|https)://(localhost:)([0-9]+)?(/.*)";
#endif

  auto rgx = org::apache::nifi::minifi::utils::Regex(regex_str, {org::apache::nifi::minifi::utils::Regex::Mode::ICASE});
  org::apache::nifi::minifi::utils::SMatch matches;
  if (org::apache::nifi::minifi::utils::regexMatch(url, matches, rgx)) {
    if (matches.size() >= 5) {
      scheme = matches[1];
      port = matches[3];
      path = matches[4];
      return true;
    }
  }
  return false;
}

std::optional<std::string_view> getDefaultCAFile();

}  // namespace org::apache::nifi::minifi::utils
