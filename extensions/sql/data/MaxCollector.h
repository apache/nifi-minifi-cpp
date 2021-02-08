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
  void endProcessBatch(State state) override {
    if (state == State::DONE) {
      updateMapState();
    }
  }
  void beginProcessRow() override {}
  void endProcessRow() override {}

  void processColumnNames(const std::vector<std::string>& names) override {
    for (auto& expected : mapState_) {
      if (std::find(names.begin(), names.end(), expected.first) == names.end()) {
        throw minifi::Exception(PROCESSOR_EXCEPTION,
          "Column '" + expected.first + "' is not found in the columns of '" + selectQuery_ + "' result.");
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
  struct MaxValue {
    void updateMaxValue(const std::string& column, const T& value) {
      const auto it = column_maxima.find(column);
      if (it == column_maxima.end()) {
        column_maxima.insert({ column, value });
      } else {
        if (value > it->second) {
          it->second = value;
        }
      }
    }

    std::unordered_map<std::string, T> column_maxima;
  };

  template <typename ...Ts>
  struct MaxValues : public MaxValue<Ts>... {
    void updateState(std::unordered_map<std::string, std::string>& state) const {
      (void)(std::initializer_list<int>{([&] {
        for (auto& curr_column_max : state) {
          const auto it = MaxValue<Ts>::column_maxima.find(curr_column_max.first);
          if (it != MaxValue<Ts>::column_maxima.end()) {
            std::stringstream ss;
            ss << it->second;
            curr_column_max.second = ss.str();
          }
        }
      }(), 0)...});
    }
  };

 public:
  MaxCollector(std::string selectQuery, std::unordered_map<std::string, std::string>& mapState)
    :selectQuery_(std::move(selectQuery)), mapState_(mapState) {
  }

  template <typename T>
  void updateMaxValue(const std::string& columnName, const T& value) {
    if (mapState_.count(columnName)) {
      maxValues_.MaxValue<T>::updateMaxValue(columnName, value);
    }
  }

  void updateMapState() {
    maxValues_.updateState(mapState_);
  }

 private:
  const std::string selectQuery_;
  std::unordered_map<std::string, std::string>& mapState_;
  MaxValues<std::string, double, int, long long, unsigned long long> maxValues_;
};
  
}  // namespace sql
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
