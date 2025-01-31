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

#ifdef WIN32
// ignore the warning about inheriting via dominance from CoreComponent
#pragma warning(disable : 4250)
#endif

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#include "utils/ArrayUtils.h"
#include "minifi-cpp/utils/Id.h"
#include "minifi-cpp/properties/Configure.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::core {

/**
 * Base component within MiNiFi
 * Purpose: Many objects store a name and UUID, therefore
 * the functionality is localized here to avoid duplication
 */
class CoreComponent {
 public:
  virtual ~CoreComponent() = default;

  [[nodiscard]] virtual std::string getName() const = 0;
  virtual void setName(std::string name) = 0;
  virtual void setUUID(const utils::Identifier& uuid) = 0;
  [[nodiscard]] virtual utils::Identifier getUUID() const = 0;
  [[nodiscard]] virtual utils::SmallString<36> getUUIDStr() const = 0;
  virtual void configure(const std::shared_ptr<Configure>& /*configuration*/) = 0;
};

}  // namespace org::apache::nifi::minifi::core
