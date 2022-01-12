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
#include <functional>

namespace org::apache::nifi::minifi::sql {

class SQLIdentifier {
 public:
  explicit SQLIdentifier(const std::string& str);

  std::string value() const { return value_; }

  std::string str() const { return original_value_; }

  bool operator==(const SQLIdentifier &other) const {
    return value_ == other.value_;
  }

  bool operator==(const std::string& other) const {
    return value_ == other;
  }

  friend struct ::std::hash<SQLIdentifier>;

 private:
  std::string original_value_;
  std::string value_;
};

}  // namespace org::apache::nifi::minifi::sql

namespace std {
template<>
struct hash<org::apache::nifi::minifi::sql::SQLIdentifier> {
  size_t operator()(const org::apache::nifi::minifi::sql::SQLIdentifier &id) const {
    return std::hash<std::string>{}(id.value_);
  }
};
}  // namespace std
