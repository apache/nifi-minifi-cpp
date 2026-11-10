// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <unordered_set>
#include <optional>

#include "core/Core.h"
#include "minifi-cpp/Exception.h"

namespace org::apache::nifi::minifi::core {

class ParameterException : public Exception {
 public:
  explicit ParameterException(const std::string& message) : Exception(ExceptionType::PARAMETER_EXCEPTION, message) {}
  explicit ParameterException(const char* message) : Exception(ExceptionType::PARAMETER_EXCEPTION, message) {}
};

struct Parameter {
  std::string name;
  std::string description;
  bool sensitive = false;
  bool provided = false;
  std::string value;
};

class ParameterContext : public CoreComponentImpl {
 public:
  using CoreComponentImpl::CoreComponentImpl;

  void setDescription(const std::string &description) {
    description_ = description;
  }

  std::string getDescription() const {
    return description_;
  }

  void addParameter(const Parameter &parameter);
  std::optional<Parameter> getParameter(const std::string &name) const;

  const std::unordered_map<std::string, Parameter>& getParameters() const {
    return parameters_;
  }

  void addInheritedParameterContext(gsl::not_null<ParameterContext*> parameter_context) {
    inherited_parameter_contexts_.push_back(parameter_context);
  }

  const std::vector<gsl::not_null<ParameterContext*>>& getInheritedParameterContexts() const {
    return inherited_parameter_contexts_;
  }

  void setParameterProvider(const std::string &parameter_provider) {
    parameter_provider_ = parameter_provider;
  }

  std::string getParameterProvider() const {
    return parameter_provider_;
  }

 private:
  std::string description_;
  std::unordered_map<std::string, Parameter> parameters_;
  std::vector<gsl::not_null<ParameterContext*>> inherited_parameter_contexts_;
  std::string parameter_provider_;
};

}  // namespace org::apache::nifi::minifi::core

