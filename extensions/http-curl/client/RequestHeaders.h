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

#include <unordered_map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

struct curl_slist;

namespace org::apache::nifi::minifi::extensions::curl {
class RequestHeaders {
 public:
  RequestHeaders() = default;

  void appendHeader(std::string key, std::string value);

  void disableExpectHeader();

  struct CurlSListFreeAll { void operator()(struct curl_slist* curl) const; };

  [[nodiscard]] std::unique_ptr<struct curl_slist, CurlSListFreeAll> get() const;
  [[nodiscard]] auto size() const { return headers_.size(); }
  [[nodiscard]] bool empty() const;

  std::string& operator[](const std::string& key);
  std::string& operator[](std::string&& key);

  [[nodiscard]] bool contains(const std::string& key) const;
  void erase(const std::string& key);

 private:
  std::unordered_map<std::string, std::string> headers_;
};
}  // namespace org::apache::nifi::minifi::extensions::curl
