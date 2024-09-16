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

#include <optional>
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/**
 * Provides a single source of truth for the agent's class and identifier.
 */
class AgentIdentificationProvider {
 public:
  [[nodiscard]] virtual std::optional<std::string> getAgentClass() const = 0;

  [[nodiscard]] virtual std::string getAgentIdentifier() const = 0;

  virtual ~AgentIdentificationProvider() = default;
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
