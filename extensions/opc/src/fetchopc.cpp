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

#include "fetchopc.h"

#include <list>
#include <memory>
#include <string>

#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "opc.h"
#include "utils/Enum.h"
#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

void FetchOPCProcessor::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchOPCProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) {
  logger_->log_trace("FetchOPCProcessor::onSchedule");

  state_manager_ = context.getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  translated_node_ids_.clear();  // Path might has changed during restart

  BaseOPCProcessor::onSchedule(context, factory);

  node_id_ = utils::parseProperty(context, NodeID);
  max_depth_ = utils::parseU64Property(context, MaxDepth);

  parseIdType(context, NodeIDType);

  namespace_idx_ = gsl::narrow<int32_t>(utils::parseI64Property(context, NameSpaceIndex));

  lazy_mode_ = utils::parseEnumProperty<LazyModeOptions>(context, Lazy);

  if (id_type_ == opc::OPCNodeIDType::Path) {
    readPathReferenceTypes(context, node_id_);
  }
}

void FetchOPCProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("FetchOPCProcessor::onTrigger");

  if (!reconnect()) {
    context.yield();
    return;
  }

  size_t nodes_found = 0;
  size_t variables_found = 0;

  std::unordered_map<std::string, std::string> state_map;
  state_manager_->get(state_map);

  auto found_cb = [this, &context, &session, &nodes_found, &variables_found, &state_map](const UA_ReferenceDescription* ref, const std::string& path) {
    return nodeFoundCallBack(ref, path, context, session, nodes_found, variables_found, state_map);
  };

  if (id_type_ != opc::OPCNodeIDType::Path) {
    UA_NodeId my_id;
    my_id.namespaceIndex = namespace_idx_;
    if (id_type_ == opc::OPCNodeIDType::Int) {
      my_id.identifierType = UA_NODEIDTYPE_NUMERIC;
      my_id.identifier.numeric = std::stoi(node_id_);  // NOLINT(cppcoreguidelines-pro-type-union-access)
    } else if (id_type_ == opc::OPCNodeIDType::String) {
      my_id.identifierType = UA_NODEIDTYPE_STRING;
      my_id.identifier.string = UA_STRING_ALLOC(node_id_.c_str());  // NOLINT(cppcoreguidelines-pro-type-union-access)
    } else {
      logger_->log_error("Unhandled id type: '{}'. No flowfiles are generated.", magic_enum::enum_underlying(id_type_));
      context.yield();
      return;
    }
    connection_->traverse(my_id, found_cb, "", max_depth_);
  } else {
    if (translated_node_ids_.empty()) {
      auto sc = connection_->translateBrowsePathsToNodeIdsRequest(node_id_, translated_node_ids_, namespace_idx_, path_reference_types_, logger_);
      if (sc != UA_STATUSCODE_GOOD) {
        logger_->log_error("Failed to translate {} to node id, no flow files will be generated ({})", node_id_.c_str(), UA_StatusCode_name(sc));
        context.yield();
        return;
      }
    }
    for (auto& node_id : translated_node_ids_) {
      connection_->traverse(node_id, found_cb, node_id_, max_depth_);
    }
  }
  if (nodes_found == 0) {
    logger_->log_warn("Connected to OPC server, but no variable nodes were found. Configuration might be incorrect! Yielding...");
    context.yield();
  } else if (variables_found == 0) {
    logger_->log_warn("Found no variables when traversing the specified node. No flowfiles are generated. Yielding...");
    context.yield();
  }

  state_manager_->set(state_map);
}

void FetchOPCProcessor::writeFlowFileUsingLazyModeWithTimestamp(const opc::NodeData& nodedata, core::ProcessContext& context, core::ProcessSession& session, size_t& variables_found,
    std::unordered_map<std::string, std::string>& state_map) const {
  auto writeDataToFlowFile = [this, &nodedata, &context, &session, &variables_found]() {
    OPCData2FlowFile(nodedata, context, session);
    ++variables_found;
  };

  auto full_path_it = nodedata.attributes.find("Full path");
  if (full_path_it == nodedata.attributes.end()) {
    logger_->log_error("Node data does not contain 'Full path' attribute, cannot read state for node");
    writeDataToFlowFile();
    return;
  }

  auto source_timestamp_it = nodedata.attributes.find("Sourcetimestamp");
  if (source_timestamp_it == nodedata.attributes.end()) {
    logger_->log_error("Node data does not contain 'Sourcetimestamp' attribute, cannot read state for node");
    writeDataToFlowFile();
    return;
  }

  auto new_state_value = source_timestamp_it->second;
  auto nodeid = full_path_it->second + "_timestamp";
  auto cur_state_value = state_map[nodeid];
  if (cur_state_value.empty() || cur_state_value != new_state_value) {
    logger_->log_debug("Node {} has new source timestamp", full_path_it->second);
    state_map[nodeid] = new_state_value;
    writeDataToFlowFile();
    return;
  }

  logger_->log_debug("Node {} has no new source timestamp, skipping", full_path_it->second);
}

void FetchOPCProcessor::writeFlowFileUsingLazyModeWithNewValue(const opc::NodeData& nodedata, core::ProcessContext& context, core::ProcessSession& session, size_t& variables_found,
    std::unordered_map<std::string, std::string>& state_map) const {
  auto writeDataToFlowFile = [this, &nodedata, &context, &session, &variables_found]() {
    OPCData2FlowFile(nodedata, context, session);
    ++variables_found;
  };

  auto full_path_it = nodedata.attributes.find("Full path");
  if (full_path_it == nodedata.attributes.end()) {
    logger_->log_error("Node data does not contain 'Full path' attribute, cannot read state for node");
    writeDataToFlowFile();
    return;
  }

  auto new_state_value = opc::nodeValue2String(nodedata);
  auto nodeid = full_path_it->second + "_new_value";
  auto cur_state_value = state_map[nodeid];
  if (cur_state_value.empty() || cur_state_value != new_state_value) {
    logger_->log_debug("Node {} has new value", full_path_it->second);
    state_map[nodeid] = new_state_value;
    writeDataToFlowFile();
    return;
  }

  logger_->log_debug("Node {} has no new value, skipping", full_path_it->second);
}

bool FetchOPCProcessor::nodeFoundCallBack(const UA_ReferenceDescription *ref, const std::string& path,
    core::ProcessContext& context, core::ProcessSession& session, size_t& nodes_found, size_t& variables_found,
    std::unordered_map<std::string, std::string>& state_map) {
  ++nodes_found;
  if (ref->nodeClass != UA_NODECLASS_VARIABLE) {
    return true;
  }
  try {
    opc::NodeData nodedata = connection_->getNodeData(ref, path);
    if (lazy_mode_ == LazyModeOptions::On) {
      writeFlowFileUsingLazyModeWithTimestamp(nodedata, context, session, variables_found, state_map);
    } else if (lazy_mode_ == LazyModeOptions::NewValue) {
      writeFlowFileUsingLazyModeWithNewValue(nodedata, context, session, variables_found, state_map);
    } else {
      OPCData2FlowFile(nodedata, context, session);
      ++variables_found;
    }
  } catch (const std::exception& exception) {
    std::string browse_name(reinterpret_cast<char*>(ref->browseName.name.data), ref->browseName.name.length);
    logger_->log_warn("Caught Exception while trying to get data from node {}: {}", path + "/" + browse_name, exception.what());
  }
  return true;
}

void FetchOPCProcessor::OPCData2FlowFile(const opc::NodeData& opc_node, core::ProcessContext&, core::ProcessSession& session) const {
  auto flow_file = session.create();
  if (flow_file == nullptr) {
    logger_->log_error("Failed to create flowfile!");
    return;
  }
  for (const auto& attr : opc_node.attributes) {
    flow_file->setAttribute(attr.first, attr.second);
  }
  if (!opc_node.data.empty()) {
    try {
      session.writeBuffer(flow_file, opc::nodeValue2String(opc_node));
    } catch (const std::exception& e) {
      std::string browsename;
      flow_file->getAttribute("Browsename", browsename);
      logger_->log_info("Failed to extract data of OPC node {}: {}", browsename, e.what());
      session.transfer(flow_file, Failure);
      return;
    }
  }
  session.transfer(flow_file, Success);
}

REGISTER_RESOURCE(FetchOPCProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
