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
#include "minifi-cpp/core/RecordField.h"

#include "utils/GeneralUtils.h"
#include "utils/TimeUtil.h"

namespace org::apache::nifi::minifi::core {

rapidjson::Value RecordField::toJson(rapidjson::Document::AllocatorType& allocator) const {
    rapidjson::Value value;
    std::visit(utils::overloaded {
      [&value, &allocator](const std::string& str) {
        value.SetString(str.c_str(), allocator);
      },
      [&value](int64_t i64) {
        value.SetInt64(i64);
      },
      [&value](uint64_t u64) {
        value.SetUint64(u64);
      },
      [&value](double d) {
        value.SetDouble(d);
      },
      [&value](bool b) {
        value.SetBool(b);
      },
      [&value, &allocator](const std::chrono::system_clock::time_point& time_point) {
        value.SetString(utils::timeutils::getDateTimeStr(std::chrono::time_point_cast<std::chrono::seconds>(time_point)).c_str(), allocator);
      },
      [&value, &allocator](const RecordArray& arr) {
        value.SetArray();
        for (const auto& elem : arr) {
          rapidjson::Value elem_value = elem.toJson(allocator);
          value.PushBack(elem_value, allocator);
        }
      },
      [&value, &allocator](const RecordObject& obj) {
        value.SetObject();
        for (const auto& [key, field] : obj) {
          rapidjson::Value keyValue;
          keyValue.SetString(key.c_str(), allocator);

          rapidjson::Value fieldValue = field.toJson(allocator);
          value.AddMember(keyValue, fieldValue, allocator);
        }
      }
    }, value_);

    return value;
}

RecordField RecordField::fromJson(const rapidjson::Value& value) {
  if (value.IsString()) {
    std::string str_value = value.GetString();
    if (auto test_time = utils::timeutils::parseDateTimeStr(str_value)) {
      return RecordField{std::chrono::time_point{*test_time}};
    }
    return RecordField{str_value};
  } else if (value.IsInt64()) {
    return RecordField{value.GetInt64()};
  } else if (value.IsUint64()) {
    return RecordField{value.GetUint64()};
  } else if (value.IsDouble()) {
    return RecordField{value.GetDouble()};
  } else if (value.IsBool()) {
    return RecordField{value.GetBool()};
  } else if (value.IsArray()) {
    RecordArray arr;
    for (const auto& elem : value.GetArray()) {
      arr.push_back(RecordField::fromJson(elem));
    }
    return RecordField{std::move(arr)};
  } else if (value.IsObject()) {
    RecordObject obj;
    for (const auto& member : value.GetObject()) {
      obj.emplace(member.name.GetString(), RecordField::fromJson(member.value));
    }
    return RecordField{std::move(obj)};
  } else {
    throw std::runtime_error("Invalid JSON value type");
  }
}

}  // namespace org::apache::nifi::minifi::core
