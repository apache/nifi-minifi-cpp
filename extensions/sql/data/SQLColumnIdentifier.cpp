// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "SQLColumnIdentifier.h"

namespace org::apache::nifi::minifi::sql {

SQLColumnIdentifier::SQLColumnIdentifier(std::string str)
    : value_([&] {
          if (str.length() < 2) {
            return str;
          }
          if ((str.front() == '"' && str.back() == '"')
              || (str.front() == '[' && str.back() == ']')
              || (str.front() == '`' && str.back() == '`')) {
            return str.substr(1, str.length() - 2);
          }
          return str;
        }()),
      original_value_(std::move(str)) {
}

}  // namespace org::apache::nifi::minifi::sql

