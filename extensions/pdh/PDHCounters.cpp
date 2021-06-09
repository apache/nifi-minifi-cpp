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
#include <unordered_map>

#include "PDHCounters.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

DWORD PDHCounter::getDWFormat() const {
  return is_double_format_ ? PDH_FMT_DOUBLE : PDH_FMT_LARGE;
}

std::unique_ptr<PDHCounter> PDHCounter::createPDHCounter(const std::string& query_name, bool is_double) {
  auto groups = utils::StringUtils::splitRemovingEmpty(query_name, "\\");
  if (groups.size() != 2 || query_name.substr(0, 1) != "\\")
    return nullptr;
  if (query_name.find("(*)") != std::string::npos) {
    return std::unique_ptr<PDHCounter> { new PDHCounterArray(query_name, is_double) };
  } else {
    return std::unique_ptr<SinglePDHCounter> { new SinglePDHCounter(query_name, is_double) };
  }
}

const std::string& PDHCounter::getName() const {
  return pdh_english_counter_name_;
}

std::string PDHCounter::getObjectName() const {
  auto groups = utils::StringUtils::splitRemovingEmpty(pdh_english_counter_name_, "\\");
  return groups[0];
}

std::string PDHCounter::getCounterName() const {
  auto groups = utils::StringUtils::splitRemovingEmpty(pdh_english_counter_name_, "\\");
  return groups[1];
}

void SinglePDHCounter::addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const {
  rapidjson::Value key(getCounterName().c_str(), getCounterName().length(), alloc);
  rapidjson::Value& group_node = acquireNode(getObjectName(), body, alloc);
  group_node.AddMember(key, getValue(), alloc);
}

PDH_STATUS SinglePDHCounter::addToQuery(PDH_HQUERY& pdh_query)  {
  return PdhAddEnglishCounterA(pdh_query, pdh_english_counter_name_.c_str(), 0, &counter_);
}

bool SinglePDHCounter::collectData() {
  return PdhGetFormattedCounterValue(counter_, getDWFormat(), nullptr, &current_value_) == ERROR_SUCCESS;
}

rapidjson::Value SinglePDHCounter::getValue() const {
  rapidjson::Value value;
  if (is_double_format_)
    value.SetDouble(current_value_.doubleValue);
  else
    value.SetInt64(current_value_.largeValue);
  return value;
}

std::string PDHCounterArray::getObjectName() const {
  std::string group_name_with_wildcard = PDHCounter::getObjectName();
  return group_name_with_wildcard.substr(0, group_name_with_wildcard.find("(*)"));
}

void PDHCounterArray::addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const {
  rapidjson::Value& group_node = acquireNode(getObjectName(), body, alloc);
  std::unordered_map<std::string, uint32_t> instance_name_counter;
  for (DWORD i = 0; i < item_count_; ++i) {
    uint32_t instance_name_count = instance_name_counter[std::string(values_[i].szName)]++;
    std::string node_name = instance_name_count > 0 ? std::string(values_[i].szName) + "#" + std::to_string(instance_name_count) : values_[i].szName;
    rapidjson::Value& counter_node = acquireNode(node_name, group_node, alloc);
    rapidjson::Value value = getValue(i);
    rapidjson::Value key;
    key.SetString(getCounterName().c_str(), getCounterName().length(), alloc);
    counter_node.AddMember(key, value, alloc);
  }
}

PDH_STATUS PDHCounterArray::addToQuery(PDH_HQUERY& pdh_query) {
  return PdhAddEnglishCounterA(pdh_query, pdh_english_counter_name_.c_str(), 0, &counter_);
}

bool PDHCounterArray::collectData() {
  clearCurrentData();
  PDH_STATUS status = PdhGetFormattedCounterArrayA(counter_, getDWFormat(), &buffer_size_, &item_count_, values_);
  if (PDH_MORE_DATA == status) {
    values_ = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(malloc(buffer_size_));
    status = PdhGetFormattedCounterArrayA(counter_, getDWFormat(), &buffer_size_, &item_count_, values_);
  }
  return status == ERROR_SUCCESS;
}

void PDHCounterArray::clearCurrentData() {
  free(values_);
  values_ = nullptr;
  buffer_size_ = item_count_ = 0;
}

rapidjson::Value PDHCounterArray::getValue(const DWORD i) const {
  rapidjson::Value value;
  if (is_double_format_)
    value.SetDouble(values_[i].FmtValue.doubleValue);
  else
    value.SetInt64(values_[i].FmtValue.largeValue);
  return value;
}

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
