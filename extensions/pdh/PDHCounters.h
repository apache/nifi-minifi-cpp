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

#include <TCHAR.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <string>

#include "PerformanceDataCounter.h"
#include <memory>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class PDHCounter : public PerformanceDataCounter {
 public:
  virtual ~PDHCounter() {}
  static std::unique_ptr<PDHCounter> createPDHCounter(const std::string& query_name, bool is_double = true);

  const std::string& getName() const;
  virtual std::string getObjectName() const;
  virtual std::string getCounterName() const;
  virtual PDH_STATUS addToQuery(PDH_HQUERY& pdh_query) = 0;
 protected:
  PDHCounter(const std::string& query_name, bool is_double)
      : PerformanceDataCounter(), is_double_format_(is_double), pdh_english_counter_name_(query_name) {}

  DWORD getDWFormat() const;

  const bool is_double_format_;
  std::string pdh_english_counter_name_;
};

class SinglePDHCounter : public PDHCounter {
 public:
  void addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const override;

  PDH_STATUS addToQuery(PDH_HQUERY& pdh_query) override;
  bool collectData() override;

 protected:
  friend std::unique_ptr<PDHCounter> PDHCounter::createPDHCounter(const std::string& query_name, bool is_double);
  explicit SinglePDHCounter(const std::string& query_name, bool is_double)
      : PDHCounter(query_name, is_double), counter_(nullptr), current_value_() {}

  rapidjson::Value getValue() const;

  PDH_HCOUNTER counter_;
  PDH_FMT_COUNTERVALUE current_value_;
};

class PDHCounterArray : public PDHCounter {
 public:
  virtual ~PDHCounterArray() { clearCurrentData(); }
  void addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const override;

  std::string getObjectName() const override;
  PDH_STATUS addToQuery(PDH_HQUERY& pdh_query) override;
  bool collectData() override;

 protected:
  friend std::unique_ptr<PDHCounter> PDHCounter::createPDHCounter(const std::string& query_name, bool is_double);
  explicit PDHCounterArray(const std::string& query_name, bool is_double)
      : PDHCounter(query_name, is_double), counter_(nullptr), buffer_size_(0), item_count_(0), values_(nullptr) {}

  void clearCurrentData();
  rapidjson::Value getValue(const DWORD i) const;

  PDH_HCOUNTER counter_;
  DWORD buffer_size_;         // Size of the values_ array in bytes
  DWORD item_count_;          // Number of items in values_ array
  PDH_FMT_COUNTERVALUE_ITEM* values_;  // Array of PDH_FMT_COUNTERVALUE_ITEM structures
};


}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
