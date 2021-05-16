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

#include <vector>
#include <string>
#include <set>

#include "core/ProcessContext.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

std::string getRequiredPropertyOrThrow(const core::ProcessContext* context, const std::string& property_name);
std::vector<std::string> listFromCommaSeparatedProperty(const core::ProcessContext* context, const std::string& property_name);
std::vector<std::string> listFromRequiredCommaSeparatedProperty(const core::ProcessContext* context, const std::string& property_name);
bool parseBooleanPropertyOrThrow(core::ProcessContext* context, const std::string& property_name);
std::chrono::milliseconds parseTimePropertyMSOrThrow(core::ProcessContext* context, const std::string& property_name);
utils::optional<uint64_t> getOptionalUintProperty(const core::ProcessContext& context, const std::string& property_name);
std::string parsePropertyWithAllowableValuesOrThrow(const core::ProcessContext& context, const std::string& property_name, const std::set<std::string>& allowable_values);

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
