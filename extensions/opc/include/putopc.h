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
#include <string>
#include <utility>
#include <vector>

#include "opc.h"
#include "opcbase.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/ArrayUtils.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::processors {

class PutOPCProcessor : public BaseOPCProcessor {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Creates/updates  OPC nodes";

  EXTENSIONAPI static const core::Property ParentNodeIDType;
  EXTENSIONAPI static const core::Property ParentNodeID;
  EXTENSIONAPI static const core::Property ParentNameSpaceIndex;
  EXTENSIONAPI static const core::Property ValueType;
  EXTENSIONAPI static const core::Property TargetNodeIDType;
  EXTENSIONAPI static const core::Property TargetNodeID;
  EXTENSIONAPI static const core::Property TargetNodeBrowseName;
  EXTENSIONAPI static const core::Property TargetNodeNameSpaceIndex;
  static auto properties() {
    return utils::array_cat(BaseOPCProcessor::properties(), std::array{
      ParentNodeIDType,
      ParentNodeID,
      ParentNameSpaceIndex,
      ValueType,
      TargetNodeIDType,
      TargetNodeID,
      TargetNodeBrowseName,
      TargetNodeNameSpaceIndex
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit PutOPCProcessor(std::string name, const utils::Identifier& uuid = {})
      : BaseOPCProcessor(std::move(name), uuid), nameSpaceIdx_(0), parentExists_(false) {
    logger_ = core::logging::LoggerFactory<PutOPCProcessor>::getLogger();
  }

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;

 private:
  std::string nodeID_;
  int32_t nameSpaceIdx_{};
  opc::OPCNodeIDType idType_{};
  UA_NodeId parentNodeID_{};
  bool parentExists_{};
  opc::OPCNodeDataType nodeDataType_{};
};

}  // namespace org::apache::nifi::minifi::processors
