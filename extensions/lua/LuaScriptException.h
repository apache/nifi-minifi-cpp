// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <exception>
#include <utility>
#include <string>
#include <stdexcept>

namespace org::apache::nifi::minifi::extensions::lua {

class LuaScriptException : public std::runtime_error {
 public:
  explicit LuaScriptException(const std::string& error) : std::runtime_error(error) {}
};
}  // namespace org::apache::nifi::minifi::extensions::lua
