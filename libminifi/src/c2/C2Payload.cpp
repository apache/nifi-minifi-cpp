// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "c2/C2Payload.h"
#include <utility>
#include <vector>
#include <string>
#include "utils/StringUtils.h"
#include "rapidjson/writer.h"

namespace org::apache::nifi::minifi::c2 {

C2Payload::C2Payload(Operation op, std::string identifier, bool isRaw)
    : C2Payload(op, state::UpdateState::FULLY_APPLIED, std::move(identifier), isRaw) {
}

C2Payload::C2Payload(Operation op, state::UpdateState state, std::string identifier, bool isRaw)
    : state::Update(state::UpdateStatus(state, 0)),
      ident_(std::move(identifier)),
      op_(op),
      raw_(isRaw) {
}


C2Payload::C2Payload(Operation op, bool isRaw)
    : state::Update(state::UpdateStatus(state::UpdateState::INITIATE, 0)),
      op_(op),
      raw_(isRaw) {
}

C2Payload::C2Payload(Operation op, state::UpdateState state, bool isRaw)
    : state::Update(state::UpdateStatus(state, 0)),
      op_(op),
      raw_(isRaw) {
}

void C2Payload::addContent(C2ContentResponse &&content, bool collapsible) {
  if (collapsible) {
    for (auto &existing_content : content_) {
      if (existing_content.name == content.name) {
        existing_content.operation_arguments.insert(content.operation_arguments.begin(), content.operation_arguments.end());
        return;
      }
    }
  }
  content_.push_back(std::move(content));
}

void C2Payload::setRawData(const std::string &data) {
  setRawData(std::as_bytes(std::span(data)));
}

void C2Payload::setRawData(const std::vector<char> &data) {
  setRawData(std::as_bytes(std::span(data)));
}

void C2Payload::setRawData(std::span<const std::byte> data) {
  raw_data_.reserve(raw_data_.size() + data.size());
  raw_data_.insert(std::end(raw_data_), std::begin(data), std::end(data));
}

void C2Payload::addPayload(C2Payload &&payload) {
  payloads_.push_back(std::move(payload));
}

template<typename T>
static std::ostream& operator<<(std::ostream& out, const std::vector<T>& items) {
  out << "[";
  bool first = true;
  for (auto& item : items) {
    if (!first) out << ", ";
    first = false;
    out << item;
  }
  out << "]";
  return out;
}

template<typename K, typename V>
static std::ostream& operator<<(std::ostream& out, const std::map<K, V>& items) {
  out << "{";
  bool first = true;
  for (auto& item : items) {
    if (!first) out << ", ";
    first = false;
    out << item.first << ": " << item.second;
  }
  out << "}";
  return out;
}

std::ostream& operator<<(std::ostream& out, const C2Payload& payload) {
  out << std::boolalpha;
  return out << "{"
    << "ident: \"" << payload.ident_ << "\", "
    << "label: \"" << payload.label_ << "\", "
    << "payloads: " << payload.payloads_ << ", "
    << "contents: " << payload.content_ << ", "
    << "op: " << magic_enum::enum_name(payload.op_) << ", "
    << "raw: " << payload.raw_ << ", "
    << "data: \"" << utils::string::escapeUnprintableBytes(payload.raw_data_) << "\", "
    << "is_container: " << payload.is_container_ << ", "
    << "is_collapsible: " << payload.is_collapsible_
    << "}";
}

std::ostream& operator<<(std::ostream& out, const C2ContentResponse& response) {
  out << std::boolalpha;
  return out << "{"
    << "op: " << magic_enum::enum_name(response.op) << ", "
    << "required: " << response.required << ", "
    << "ident: \"" << response.ident << "\", "
    << "delay: " << response.delay << ", "
    << "ttl: " << response.ttl << ", "
    << "name: \"" << response.name << "\", "
    << "args: " << response.operation_arguments
    << "}";
}

std::ostream& operator<<(std::ostream& out, const C2Value& val) {
  if (auto* val_ptr = val.valueNode()) {
    out << '"' << val_ptr->to_string() << '"';
  } else {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    gsl::not_null(val.json())->Accept(writer);
    out << std::string_view{buffer.GetString(), buffer.GetLength()};
  }
  return out;
}

}  // namespace org::apache::nifi::minifi::c2
