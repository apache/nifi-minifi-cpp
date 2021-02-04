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
#include "rapidjson/document.h"

#define FIELD_ACCESSOR(field) \
  template<typename T> \
  static auto get_##field(T&& instance) -> decltype((std::forward<T>(instance).field)) { \
    return std::forward<T>(instance).field; \
  }

#define METHOD_ACCESSOR(method) \
  template<typename T, typename ...Args> \
  static auto call_##method(T&& instance, Args&& ...args) -> decltype((std::forward<T>(instance).method(std::forward<Args>(args)...))) { \
    return std::forward<T>(instance).method(std::forward<Args>(args)...); \
  }

// carries out a loose match on objects, i.e. it doesn't matter if the
// actual object has extra fields than expected
void matchJSON(const rapidjson::Value& json, const rapidjson::Value& expected) {
  if (expected.IsObject()) {
    REQUIRE(json.IsObject());
    for (const auto& expected_member : expected.GetObject()) {
      REQUIRE(json.HasMember(expected_member.name));
      matchJSON(json[expected_member.name], expected_member.value);
    }
  } else if (expected.IsArray()) {
    REQUIRE(json.IsArray());
    REQUIRE(json.Size() == expected.Size());
    for (size_t idx{0}; idx < expected.Size(); ++idx) {
      matchJSON(json[idx], expected[idx]);
    }
  } else {
    REQUIRE(json == expected);
  }
}

void verifyJSON(const std::string& json_str, const std::string& expected_str) {
  rapidjson::Document json, expected;
  REQUIRE_FALSE(json.Parse(json_str.c_str()).HasParseError());
  REQUIRE_FALSE(expected.Parse(expected_str.c_str()).HasParseError());

  matchJSON(json, expected);
}
