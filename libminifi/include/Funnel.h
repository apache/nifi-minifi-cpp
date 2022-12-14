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
#include <utility>

#include "ForwardingNode.h"

namespace org::apache::nifi::minifi {

class Funnel final : public ForwardingNode {
 public:
  Funnel(std::string name, const utils::Identifier& uuid) : ForwardingNode(std::move(name), uuid, core::logging::LoggerFactory<Funnel>::getLogger(uuid)) {}
  explicit Funnel(std::string name) : ForwardingNode(std::move(name), core::logging::LoggerFactory<Funnel>::getLogger()) {}

  MINIFIAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS
};

}  // namespace org::apache::nifi::minifi
