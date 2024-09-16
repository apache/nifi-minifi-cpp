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

#include <memory>
#include <vector>
#include <string>

#include "minifi-cpp/properties/Configure.h"

namespace org::apache::nifi::minifi::core::extension {

using ExtensionConfig = std::shared_ptr<org::apache::nifi::minifi::Configure>;

class Extension {
 public:
  virtual ~Extension() = default;

  /**
   * Ensures that the extension is initialized at most once, and schedules
   * an automatic deinitialization on extension unloading. This init/deinit
   * is backed by a local static object and sequenced relative to other static
   * variable init/deinit (specifically the registration of classes into ClassLoader)
   * according to the usual rules.
   * @param config
   * @return True if the initialization succeeded
   */
  virtual bool initialize(const ExtensionConfig& config) = 0;
  virtual const std::string& getName() const = 0;
};

}  // namespace org::apache::nifi::minifi::core::extension
