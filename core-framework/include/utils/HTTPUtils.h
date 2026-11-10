// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
