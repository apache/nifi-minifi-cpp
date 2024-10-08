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
#include <unordered_map>
#include <vector>
#include <memory>
#include <variant>
#include <chrono>
#include <concepts>

namespace org::apache::nifi::minifi::core {

struct RecordField;

struct BoxedRecordField {
  BoxedRecordField() = default;
  BoxedRecordField(const BoxedRecordField&) = delete;
  BoxedRecordField(BoxedRecordField&& rhs) noexcept : field(std::move(rhs.field)) {}
  BoxedRecordField& operator=(const BoxedRecordField&) = delete;
  BoxedRecordField& operator=(BoxedRecordField&& rhs)  noexcept {
    field = std::move(rhs.field);
    return *this;
  };
  ~BoxedRecordField() = default;

  explicit BoxedRecordField(std::unique_ptr<RecordField>&& _field) : field(std::move(_field)) {}
  bool operator==(const BoxedRecordField&) const;
  std::unique_ptr<RecordField> field = nullptr;
};


using RecordArray = std::vector<RecordField>;

using RecordObject = std::unordered_map<std::string, BoxedRecordField>;

template<typename T>
concept Float = std::is_floating_point_v<T>;

template<typename T>
concept Integer = std::integral<T>;

struct RecordField {
  explicit RecordField(RecordObject ro) : value_(std::move(ro)) {}
  explicit RecordField(RecordArray ra) : value_(std::move(ra)) {}
  explicit RecordField(std::string s) : value_(std::move(s)) {}
  explicit RecordField(std::chrono::system_clock::time_point tp) : value_(tp) {}
  explicit RecordField(bool b) : value_(b) {}
  explicit RecordField(const char c) : value_(std::string{c}) {}
  explicit RecordField(uint64_t u64) : value_(u64) {}
  explicit RecordField(Integer auto i64) : value_(int64_t{i64}) {}
  explicit RecordField(Float auto f) : value_(f) {}

  RecordField(const RecordField& field) = default;
  RecordField(RecordField&& field) noexcept : value_(std::move(field.value_)) {}

  RecordField& operator=(const RecordField&) = default;
  RecordField& operator=(RecordField&& field)  noexcept {
      value_ = std::move(field.value_);
      return *this;
  };

  ~RecordField() = default;


  bool operator==(const RecordField& rhs) const = default;

  std::variant<std::string, int64_t, uint64_t, double, bool, std::chrono::system_clock::time_point, RecordArray, RecordObject> value_;
};

inline bool BoxedRecordField::operator==(const BoxedRecordField& rhs) const {
  if (!field || !rhs.field)
    return field == rhs.field;
  return *field == *rhs.field;
}

}  // namespace org::apache::nifi::minifi::core
