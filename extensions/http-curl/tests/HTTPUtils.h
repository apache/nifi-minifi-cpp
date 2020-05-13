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

#ifndef NIFI_MINIFI_CPP_HTTPUTILS_H
#define NIFI_MINIFI_CPP_HTTPUTILS_H

#if defined(_WIN32) || __cplusplus > 201103L
#include <regex>
#else
#include <regex.h>
#endif

/**
This function, unfortunately, assumes that we're parsing http components of a local host. On windows this is problematic
so we convert localhost to our local hostname.
  */
inline bool parse_http_components(const std::string &url, std::string &port, std::string &scheme, std::string &path) {

#if (__cplusplus > 201103L) || defined(_WIN32)
  #ifdef WIN32
	auto hostname = (url.find(org::apache::nifi::minifi::io::Socket::getMyHostName()) != std::string::npos ? org::apache::nifi::minifi::io::Socket::getMyHostName() : "localhost");
	std::string regexstr = "^(http|https)://(" + hostname + ":)([0-9]+)?(/.*)$";
#else
	std::string regexstr = "^(http|https)://(localhost:)([0-9]+)?(/.*)$";
#endif
  std::regex rgx;
  std::regex_constants::syntax_option_type regex_mode = std::regex_constants::icase;

  rgx = std::regex(regexstr, regex_mode);

  std::smatch matches;
  std::string scratch = url;
  if (std::regex_search(scratch, matches, rgx)) {
	  for (int i = 1; i < matches.size(); i++) {
		  auto str = matches[i].str();
		  switch (i) {
		  case 1:
			  scheme = str;
			  break;
		  case 3:
			  port = str;
			  break;
		  case 4:
			  path = str;
			  break;
		  default:
			  break;
		  }
	  }
  }
#else
  const char *regexstr = "^(http|https)://(localhost:)([0-9]+)?(/.*)$";
  regex_t regex;

  int ret = regcomp(&regex, regexstr, REG_EXTENDED);
  if (ret) {
    return false;
  }

  size_t potentialGroups = regex.re_nsub + 1;
  regmatch_t groups[potentialGroups];
  if (regexec(&regex, url.c_str(), potentialGroups, groups, 0) == 0) {
    for (size_t i = 0; i < potentialGroups; i++) {
      if (groups[i].rm_so == -1)
        break;

      std::string str(url.data() + groups[i].rm_so, groups[i].rm_eo - groups[i].rm_so);
      switch (i) {
        case 1:
          scheme = str;
          break;
        case 3:
          port = str;
          break;
        case 4:
          path = str;
          break;
        default:
          break;
      }
    }
  }
  if (path.empty() || scheme.empty() || port.empty())
    return false;

  regfree(&regex);
#endif
  return true;

}

#endif //NIFI_MINIFI_CPP_HTTPUTILS_H
