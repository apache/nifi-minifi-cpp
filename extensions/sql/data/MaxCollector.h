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

#pragma once

#include <string>
#include <unordered_map>
#include <tuple>
#include <vector>
#include <sstream>

#include "SQLRowSubscriber.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {

class MaxCollector: public SQLRowSubscriber {
  void beginProcessBatch() override {}
  void endProcessBatch() override {}
  void beginProcessRow() override {}
  void endProcessRow() override {}
  void finishProcessing() override {
    updateMapState();
  }

  void processColumnNames(const std::vector<std::string>& names) override {
    for (const auto& expected : state_) {
      if (std::find(names.begin(), names.end(), expected.first) == names.end()) {
        throw minifi::Exception(PROCESSOR_EXCEPTION,
          "Column '" + expected.first + "' is not found in the columns of '" + query_ + "' result.");
      }
    }
  }

  void processColumn(const std::string& name, const std::string& value)  override {
    updateMaxValue(name, '\'' + value + '\'');
  }

  void processColumn(const std::string& name, double value) override {
    updateMaxValue(name, value);
  }

  void processColumn(const std::string& name, int value) override {
    updateMaxValue(name, value);
  }

  void processColumn(const std::string& name, long long value) override {
    updateMaxValue(name, value);
  }

  void processColumn(const std::string& name, unsigned long long value) override {
    updateMaxValue(name, value);
  }

  void processColumn(const std::string& /*name*/, const char* /*value*/) override {}

  template <typename T>
  class MaxValue {
   public:
    void updateMaxValue(const std::string& column, const T& value) {
      const auto it = column_maxima.find(column);
      if (it == column_maxima.end()) {
        column_maxima.emplace(column, value);
      } else {
        if (value > it->second) {
          it->second = value;
        }
      }
    }

   protected:
    void updateStateImpl(std::unordered_map<std::string, std::string>& state) const {
      for (auto& curr_column_max : state) {
        const auto it = column_maxima.find(curr_column_max.first);
        if (it != column_maxima.end()) {
          std::stringstream ss;
          ss << it->second;
          curr_column_max.second = ss.str();
        }
      }
    }

   private:
    std::unordered_map<std::string, T> column_maxima;
  };

  template <typename ...Ts>
  struct MaxValues : public MaxValue<Ts>... {
    void updateState(std::unordered_map<std::string, std::string>& state) const {
      (void)(std::initializer_list<int>{(MaxValue<Ts>::updateStateImpl(state), 0)...});
    }
  };

 public:
  MaxCollector(std::string query, std::unordered_map<std::string, std::string>& state)
    :query_(std::move(query)), state_(state) {
  }

  template <typename T>
  void updateMaxValue(const std::string& column_name, const T& value) {
    if (state_.count(column_name)) {
      max_values_.MaxValue<T>::updateMaxValue(column_name, value);
    }
  }

  void updateMapState() {
    max_values_.updateState(state_);
  }

 private:
  const std::string query_;
  std::unordered_map<std::string, std::string>& state_;
  MaxValues<std::string, double, int, long long, unsigned long long> max_values_;
};
  
}  // namespace sql
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
