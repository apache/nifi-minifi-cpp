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
#include <unordered_map>
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
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

class FetchOPCProcessor : public BaseOPCProcessor {
 public:
  explicit FetchOPCProcessor(std::string name, const utils::Identifier& uuid = {})
      : BaseOPCProcessor(std::move(name), uuid), nameSpaceIdx_(0), nodesFound_(0), variablesFound_(0), maxDepth_(0) {
    logger_ = core::logging::LoggerFactory<FetchOPCProcessor>::getLogger();
  }

  EXTENSIONAPI static constexpr const char* Description = "Fetches OPC-UA node";

  EXTENSIONAPI static const core::Property NodeIDType;
  EXTENSIONAPI static const core::Property NodeID;
  EXTENSIONAPI static const core::Property NameSpaceIndex;
  EXTENSIONAPI static const core::Property MaxDepth;
  EXTENSIONAPI static const core::Property Lazy;
  static auto properties() {
    return utils::array_cat(BaseOPCProcessor::properties(), std::array{
      NodeIDType,
      NodeID,
      NameSpaceIndex,
      MaxDepth,
      Lazy
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;

 protected:
  bool nodeFoundCallBack(opc::Client& client, const UA_ReferenceDescription *ref, const std::string& path,
                         const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);

  void OPCData2FlowFile(const opc::NodeData& opcnode, const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);

  std::string nodeID_;
  int32_t nameSpaceIdx_;
  opc::OPCNodeIDType idType_;
  uint32_t nodesFound_;
  uint32_t variablesFound_;
  uint64_t maxDepth_;
  bool lazy_mode_;

 private:
  std::vector<UA_NodeId> translatedNodeIDs_;  // Only used when user provides path, path->nodeid translation is only done once
  std::unordered_map<std::string, std::string> node_timestamp_;  // Key = Full path, Value = Timestamp
};

}  // namespace org::apache::nifi::minifi::processors
