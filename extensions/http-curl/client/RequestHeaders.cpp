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

#include "RequestHeaders.h"
#include <string>
#include <utility>
#include "utils/StringUtils.h"

#ifdef WIN32
#pragma comment(lib, "wldap32.lib" )
#pragma comment(lib, "crypt32.lib" )
#pragma comment(lib, "Ws2_32.lib")

#define CURL_STATICLIB
#include <curl/curl.h>
#else
#include <curl/curl.h>
#endif

namespace org::apache::nifi::minifi::extensions::curl {

void RequestHeaders::appendHeader(std::string key, std::string value) {
  headers_.emplace(std::move(key), std::move(value));
}

void RequestHeaders::disableExpectHeader() {
  headers_["Expect"] = "";
}

std::unique_ptr<struct curl_slist, RequestHeaders::CurlSListFreeAll> RequestHeaders::get() const {
  curl_slist* new_list = nullptr;
  for (const auto& [header_key, header_value] : headers_)
    new_list = curl_slist_append(new_list, utils::StringUtils::join_pack(header_key, ": ", header_value).c_str());

  return {new_list, {}};
}

bool RequestHeaders::empty() const {
  return headers_.empty();
}

[[nodiscard]] bool RequestHeaders::contains(const std::string& key) const {
  return headers_.contains(key);
}

void RequestHeaders::erase(const std::string& key) {
  headers_.erase(key);
}

std::string& RequestHeaders::operator[](const std::string& key) {
  return headers_[key];
}

std::string& RequestHeaders::operator[](std::string&& key) {
  return headers_[std::move(key)];
}

void RequestHeaders::CurlSListFreeAll::operator()(struct curl_slist* s_list) const { curl_slist_free_all(s_list); }

}  // namespace org::apache::nifi::minifi::extensions::curl
