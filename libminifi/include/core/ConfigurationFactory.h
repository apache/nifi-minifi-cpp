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

#ifndef LIBMINIFI_INCLUDE_CORE_CONFIGURATIONFACTORY_H_
#define LIBMINIFI_INCLUDE_CORE_CONFIGURATIONFACTORY_H_

#include <memory>
#include <optional>
#include <string>
#include <type_traits>

#include "FlowConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

template<typename T>
T* instantiate(ConfigurationContext ctx) {
  return new T(std::move(ctx));
}

/**
 * Configuration factory is used to create a new FlowConfiguration
 * object.
 */
std::unique_ptr<core::FlowConfiguration> createFlowConfiguration(const ConfigurationContext& ctx, const std::string& configuration_class_name, bool fail_safe = false);

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_CONFIGURATIONFACTORY_H_
