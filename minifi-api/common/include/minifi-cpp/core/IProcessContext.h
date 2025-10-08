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
#pragma once

#include <string_view>

#include "IFlowFile.h"
#include "PropertyDefinition.h"

namespace org::apache::nifi::minifi::core {

class IProcessContext {
 public:
  virtual nonstd::expected<std::string, std::error_code> getProperty(std::string_view name, const IFlowFile* flow_file) const = 0;
  virtual bool hasNonEmptyProperty(std::string_view name) const = 0;
  virtual void yield() = 0;
  virtual std::string getProcessorName() const = 0;

  virtual ~IProcessContext() = default;

  nonstd::expected<std::string, std::error_code> getProperty(const PropertyReference& property_reference,
      const IFlowFile* flow_file = nullptr) const {
    return getProperty(property_reference.name, flow_file);
  }
};

}  // namespace org::apache::nifi::minifi::core
