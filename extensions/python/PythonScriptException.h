// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <exception>
#include <utility>
#include <string>
#include <stdexcept>

namespace org::apache::nifi::minifi::extensions::python {

class PythonScriptWarning : public std::runtime_error {
 public:
  explicit PythonScriptWarning(const std::string& error) : std::runtime_error(error) {}
};

class PythonScriptException : public std::runtime_error {
 public:
  explicit PythonScriptException(const std::string& error) : std::runtime_error(error) {}
};

}  // namespace org::apache::nifi::minifi::extensions::python
