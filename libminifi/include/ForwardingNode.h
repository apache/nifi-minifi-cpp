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
#include <memory>
#include <utility>

#include "core/logging/LoggerFactory.h"
#include "core/Processor.h"

namespace org::apache::nifi::minifi {

class ForwardingNode : public core::Processor {
 public:
  ForwardingNode(std::string name, const utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger) : Processor(std::move(name), uuid), logger_(std::move(logger)) {
    strategy_ = core::SchedulingStrategy::EVENT_DRIVEN;
  }
  ForwardingNode(std::string name, std::shared_ptr<core::logging::Logger> logger) : Processor(std::move(name)), logger_(std::move(logger)) {}

  static auto properties() { return std::array<core::Property, 0>{}; }
  MINIFIAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }
  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;
  MINIFIAPI static constexpr bool SupportsDynamicRelationships = false;
  MINIFIAPI static constexpr bool IsSingleThreaded = false;

  void initialize() override;

  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;

 private:
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi
