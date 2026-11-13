// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <optional>

#include "core/ProcessorImpl.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::extensions::python {

namespace processors {
class ExecutePythonProcessor;
}

class PythonProcessor {
 public:
  explicit PythonProcessor(python::processors::ExecutePythonProcessor* proc);

  void setSupportsDynamicProperties();

  void setSingleThreaded();

  void setDescription(const std::string& desc);

  void setVersion(const std::string& version);

  void addProperty(const std::string& name, const std::string& description, const std::optional<std::string>& defaultvalue, bool required, bool el, bool sensitive,
      const std::optional<int64_t>& property_type_code, std::span<const std::string_view> allowable_values, const std::optional<std::string>& controller_service_type_name);

 private:
  python::processors::ExecutePythonProcessor* processor_;
};

}  // namespace org::apache::nifi::minifi::extensions::python
