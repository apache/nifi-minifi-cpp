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

#include <filesystem>

namespace org::apache::nifi::minifi {
class Locations {
public:
  virtual ~Locations() = default;
  [[nodiscard]] virtual std::filesystem::path getWorkingDir() const = 0;
  [[nodiscard]] virtual std::filesystem::path getLockPath() const = 0;
  [[nodiscard]] virtual std::filesystem::path getLogPropertiesPath() const = 0;
  [[nodiscard]] virtual std::filesystem::path getUidPropertiesPath() const = 0;
  [[nodiscard]] virtual std::filesystem::path getPropertiesPath() const = 0;
  [[nodiscard]] virtual std::filesystem::path getFipsPath() const = 0;
  [[nodiscard]] virtual std::filesystem::path getLogsDirs() const = 0;
  [[nodiscard]] virtual std::string_view getDefaultExtensionsPattern() const = 0;

  [[nodiscard]] virtual std::string toString() const = 0;
};
}  // namespace org::apache::nifi::minifi
