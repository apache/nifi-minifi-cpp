// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <vector>
#include <unordered_map>
#include <string>

#include "RecordField.h"

#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson

namespace org::apache::nifi::minifi::core {

class Record final {
 public:
  Record() = default;
  Record(core::RecordObject&& record_object) : fields_(std::move(record_object)) {}
  Record(Record&& rhs) noexcept = default;
  Record& operator=(Record&& rhs) noexcept = default;

  Record(const Record&) = delete;
  Record& operator=(const Record&) = delete;

  ~Record() = default;

  auto emplace(std::string key, RecordField&& field) {
    return fields_.emplace(std::move(key), std::move(field));
  }

  [[nodiscard]] const RecordField& at(const std::string& key) const {
    return fields_.at(key);
  }

  [[nodiscard]] std::unordered_map<std::string, RecordField>::const_iterator begin() const {
    return fields_.begin();
  }

  [[nodiscard]] std::unordered_map<std::string, RecordField>::const_iterator end() const {
    return fields_.end();
  }

  bool operator==(const Record& rhs) const = default;

  rapidjson::Document toJson() const {
    rapidjson::Document doc;
    auto& alloc = doc.GetAllocator();
    rapidjson::Value obj(rapidjson::kObjectType);
    for (const auto& [key, field] : fields_) {
      obj.AddMember(rapidjson::Value(key.c_str(), alloc), field.toJson(alloc), alloc);
    }
    doc.Swap(obj);
    return doc;
  }

  static Record fromJson(const rapidjson::Document& document) {
    Record record;
    for (const auto& member : document.GetObject()) {
      record.emplace(member.name.GetString(), RecordField::fromJson(member.value));
    }
    return record;
  }

 private:
  std::unordered_map<std::string, RecordField> fields_;
};

using RecordSet = std::vector<Record>;

}  // namespace org::apache::nifi::minifi::core
