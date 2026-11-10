// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <functional>

namespace org::apache::nifi::minifi::sql {

class SQLColumnIdentifier {
 public:
  explicit SQLColumnIdentifier(std::string str);

  std::string value() const { return value_; }

  std::string str() const { return original_value_; }

  bool operator==(const SQLColumnIdentifier &other) const {
    return value_ == other.value_;
  }

  bool operator==(const std::string& other) const {
    return value_ == other;
  }

  friend struct ::std::hash<SQLColumnIdentifier>;

 private:
  std::string value_;
  std::string original_value_;
};

}  // namespace org::apache::nifi::minifi::sql

namespace std {
template<>
struct hash<org::apache::nifi::minifi::sql::SQLColumnIdentifier> {
  size_t operator()(const org::apache::nifi::minifi::sql::SQLColumnIdentifier &id) const {
    return std::hash<std::string>{}(id.value_);
  }
};
}  // namespace std
