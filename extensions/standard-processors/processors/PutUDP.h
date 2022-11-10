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
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Processor.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::core::logging { class Logger; }

namespace org::apache::nifi::minifi::processors {
class PutUDP final : public core::Processor {
 public:
  EXTENSIONAPI static constexpr const char* Description = "The PutUDP processor receives a FlowFile and packages the FlowFile content into a single UDP datagram packet "
      "which is then transmitted to the configured UDP server. "
      "The processor doesn't guarantee a successful transfer, even if the flow file is routed to the success relationship.";

  EXTENSIONAPI static const core::Property Hostname;
  EXTENSIONAPI static const core::Property Port;
  static auto properties() { return std::array{Hostname, Port}; }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit PutUDP(std::string name, const utils::Identifier& uuid = {});
  PutUDP(const PutUDP&) = delete;
  PutUDP& operator=(const PutUDP&) = delete;
  ~PutUDP() final;

  void initialize() final;
  void notifyStop() final;
  void onSchedule(core::ProcessContext*, core::ProcessSessionFactory *) final;
  void onTrigger(core::ProcessContext*, core::ProcessSession*) final;

 private:
  std::shared_ptr<core::logging::Logger> logger_;
};
}  // namespace org::apache::nifi::minifi::processors
