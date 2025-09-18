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

  void setDescription(const std::string& desc);

  void setVersion(const std::string& version);

  void addProperty(const std::string& name, const std::string& description, const std::optional<std::string>& defaultvalue, bool required, bool el, bool sensitive,
      const std::optional<int64_t>& property_type_code, gsl::span<const std::string_view> allowable_values, const std::optional<std::string>& controller_service_type_name);

 private:
  python::processors::ExecutePythonProcessor* processor_;
};

}  // namespace org::apache::nifi::minifi::extensions::python
