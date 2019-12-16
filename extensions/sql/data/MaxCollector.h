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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {

struct Date : public std::string {
  Date(const std::string& str)
    : std::string(str) {
  }
};

class MaxCollector {
  template <typename T>
  static bool valueGreaterMaxValue(const T& value, const T& maxValue) {
    return value > maxValue;
  }

  /* Date is returned as 'Y-m-d H:M:S.999', which works for string comparison 'value > maxValue', but can be customized if needed via template specialization.
  static bool valueGreaterMaxValue(const Date& value, const Date& maxValue) {
    return value > maxValue;
  }
  */

  template <typename T>
  struct MaxValue {
    void updateMaxValue(const std::string& columnName, const T& value) {
      const auto it = mapColumnNameValue_.find(columnName);
      if (it == mapColumnNameValue_.end()) {
        mapColumnNameValue_.insert({ columnName, value });
      } else {
        if (valueGreaterMaxValue(value, it->second)) {
          it->second = value;
        }
      }
    }

    std::unordered_map<std::string, T> mapColumnNameValue_;
  };

  template <typename T, typename Tuple, int Index, bool>
  struct TupleIndexByType {
    constexpr static int index() {
      using tupleElType = typename std::decay<decltype(std::get<Index + 1>(Tuple()))>::type;

      return TupleIndexByType<T, Tuple, Index + 1, std::is_same<tupleElType, MaxValue<T>>::value>::index();
    }
  };

  template <typename T, typename Tuple, int Index>
  struct TupleIndexByType<T, Tuple, Index, true> {
    constexpr static int index() {
      return Index;
    }
  };

  template <typename Tuple, int Index>
  struct UpdateMapState {
    UpdateMapState(const Tuple& tpl, std::unordered_map<std::string, std::string>& mapState) {
      for (auto& el : mapState) {
        const auto& maxVal = std::get<Index>(tpl);

        const auto it = maxVal.mapColumnNameValue_.find(el.first);
        if (it != maxVal.mapColumnNameValue_.end()) {
          el.second = (std::stringstream() << it->second).str();
        }
      }

      UpdateMapState<Tuple, Index - 1>(tpl, mapState);
    }
  };

  template <typename Tuple>
  struct UpdateMapState<Tuple, -1> {
    UpdateMapState(const Tuple&, std::unordered_map<std::string, std::string>&) {}
  };

  template <typename ...Ts>
  struct MaxValues : public std::tuple<MaxValue<Ts>...> {
    constexpr static size_t size = sizeof...(Ts);
  };

 public:
  MaxCollector(const std::string& selectQuery, const std::string& maxValueColumnNames, std::unordered_map<std::string, std::string>& mapState)
    :selectQuery_(selectQuery), maxValueColumnNames_(maxValueColumnNames), mapState_(mapState) {
  }

  bool hasColumn(const std::string& columName) {
    return !!mapState_.count(columName);
  }

  template <typename T>
  void updateMaxValue(const std::string& columnName, const T& value) {
    if (mapState_.count(columnName)) {
      constexpr auto index = TupleIndexByType<T, decltype(maxValues_), -1, false>::index();
      std::get<index>(maxValues_).updateMaxValue(columnName, value);
    }
  }

  bool updateMapState() {
    auto mapState = mapState_;
    UpdateMapState<decltype(maxValues_), decltype(maxValues_)::size - 1>(maxValues_, mapState_);

    return mapState != mapState_;
  }

  void checkNumberProcessedColumns(int columns) {
    if (columns != mapState_.size())
      throw std::runtime_error("QueryDatabaseTable: Column(s) '" + maxValueColumnNames_ + "' are not found in the columns of '" + selectQuery_ + "' result.");
  }

 private:
  const std::string selectQuery_;
  const std::string maxValueColumnNames_;
  std::unordered_map<std::string, std::string>& mapState_;
  MaxValues<std::string, double, int, long long, unsigned long long, Date> maxValues_;
};
	
} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
