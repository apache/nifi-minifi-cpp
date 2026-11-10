// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <variant>
#include <chrono>
#include <concepts>
#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::core {

struct RecordField;

using RecordArray = std::vector<RecordField>;
using RecordObject = std::unordered_map<std::string, RecordField>;

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

  rapidjson::Value toJson(rapidjson::Document::AllocatorType& alloc) const;
  static RecordField fromJson(const rapidjson::Value& value);

  bool operator==(const RecordField& rhs) const = default;

  std::variant<std::string, int64_t, uint64_t, double, bool, std::chrono::system_clock::time_point, RecordArray, RecordObject> value_;
};

}  // namespace org::apache::nifi::minifi::core
