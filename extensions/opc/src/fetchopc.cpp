/**
 * FetchOPC class definition
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

#include <memory>
#include <string>
#include <list>
#include <map>
#include <mutex>
#include <thread>

#include "opc.h"
#include "fetchopc.h"
#include "utils/ByteArrayCallback.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/Resource.h"
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
  core::Property FetchOPCProcessor::NodeID(
      core::PropertyBuilder::createProperty("Node ID")
      ->withDescription("Specifies the ID of the root node to traverse")
      ->isRequired(true)->build());


  core::Property FetchOPCProcessor::NodeIDType(
      core::PropertyBuilder::createProperty("Node ID type")
      ->withDescription("Specifies the type of the provided node ID")
      ->isRequired(true)
      ->withAllowableValues<std::string>({"Path", "Int", "String"})->build());

  core::Property FetchOPCProcessor::NameSpaceIndex(
      core::PropertyBuilder::createProperty("Namespace index")
      ->withDescription("The index of the namespace. Used only if node ID type is not path.")
      ->withDefaultValue<int32_t>(0)->build());

  core::Property FetchOPCProcessor::MaxDepth(
      core::PropertyBuilder::createProperty("Max depth")
      ->withDescription("Specifiec the max depth of browsing. 0 means unlimited.")
      ->withDefaultValue<uint64_t>(0)->build());

  core::Property FetchOPCProcessor::Lazy(
      core::PropertyBuilder::createProperty("Lazy mode")
      ->withDescription("Only creates flowfiles from nodes with new timestamp from the server.")
      ->withDefaultValue<std::string>("Off")
      ->isRequired(true)
      ->withAllowableValues<std::string>({"On", "Off"})
      ->build());

  core::Relationship FetchOPCProcessor::Success("success", "Successfully retrieved OPC-UA nodes");
  core::Relationship FetchOPCProcessor::Failure("failure", "Retrieved OPC-UA nodes where value cannot be extracted (only if enabled)");


  void FetchOPCProcessor::initialize() {
    // Set the supported properties
    std::set<core::Property> fetchOPCProperties = {OPCServerEndPoint, NodeID, NodeIDType, NameSpaceIndex, MaxDepth, Lazy};
    std::set<core::Property> baseOPCProperties = BaseOPCProcessor::getSupportedProperties();
    fetchOPCProperties.insert(baseOPCProperties.begin(), baseOPCProperties.end());
    setSupportedProperties(fetchOPCProperties);

    // Set the supported relationships
    setSupportedRelationships({Success, Failure});
  }

  void FetchOPCProcessor::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) {
    logger_->log_trace("FetchOPCProcessor::onSchedule");

    translatedNodeIDs_.clear();  // Path might has changed during restart

    BaseOPCProcessor::onSchedule(context, factory);

    std::string value;
    context->getProperty(NodeID.getName(), nodeID_);
    context->getProperty(NodeIDType.getName(), value);

    maxDepth_ = 0;
    context->getProperty(MaxDepth.getName(), maxDepth_);

    if (value == "String") {
      idType_ = opc::OPCNodeIDType::String;
    } else if (value == "Int") {
      idType_ = opc::OPCNodeIDType::Int;
    } else if (value == "Path") {
      idType_ = opc::OPCNodeIDType::Path;
    } else {
      // Where have our validators gone?
      auto error_msg = utils::StringUtils::join_pack(value, " is not a valid node ID type!");
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }

    if (idType_ == opc::OPCNodeIDType::Int) {
      try {
        std::stoi(nodeID_);
      } catch(...) {
        auto error_msg = utils::StringUtils::join_pack(nodeID_, " cannot be used as an int type node ID");
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
      }
    }
    if (idType_ != opc::OPCNodeIDType::Path) {
      if (!context->getProperty(NameSpaceIndex.getName(), nameSpaceIdx_)) {
        auto error_msg = utils::StringUtils::join_pack(NameSpaceIndex.getName(), " is mandatory in case ", NodeIDType.getName(), " is not Path");
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
      }
    }

    context->getProperty(Lazy.getName(), value);
    lazy_mode_ = value == "On" ? true : false;
  }

  void FetchOPCProcessor::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
    logger_->log_trace("FetchOPCProcessor::onTrigger");

    std::unique_lock<std::mutex> lock(onTriggerMutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
      logger_->log_warn("processor was triggered before previous listing finished, configuration should be revised!");
      return;
    }

    if (!reconnect()) {
      yield();
      return;
    }

    nodesFound_ = 0;
    variablesFound_ = 0;

    std::function<opc::nodeFoundCallBackFunc> f = std::bind(&FetchOPCProcessor::nodeFoundCallBack, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, context, session);
    if (idType_ != opc::OPCNodeIDType::Path) {
      UA_NodeId myID;
      myID.namespaceIndex = nameSpaceIdx_;
      if (idType_ == opc::OPCNodeIDType::Int) {
        myID.identifierType = UA_NODEIDTYPE_NUMERIC;
        myID.identifier.numeric = std::stoi(nodeID_);
      } else if (idType_ == opc::OPCNodeIDType::String) {
        myID.identifierType = UA_NODEIDTYPE_STRING;
        myID.identifier.string = UA_STRING_ALLOC(nodeID_.c_str());
      }
      connection_->traverse(myID, f, "", maxDepth_);
    } else {
      if (translatedNodeIDs_.empty()) {
        auto sc = connection_->translateBrowsePathsToNodeIdsRequest(nodeID_, translatedNodeIDs_, logger_);
        if (sc != UA_STATUSCODE_GOOD) {
          logger_->log_error("Failed to translate %s to node id, no flow files will be generated (%s)", nodeID_.c_str(), UA_StatusCode_name(sc));
          yield();
          return;
        }
      }
      for (auto& nodeID : translatedNodeIDs_) {
        connection_->traverse(nodeID, f, nodeID_, maxDepth_);
      }
    }
    if (nodesFound_ == 0) {
      logger_->log_warn("Connected to OPC server, but no variable nodes were found. Configuration might be incorrect! Yielding...");
      yield();
    } else if (variablesFound_ == 0) {
      logger_->log_warn("Found no variables when traversing the specified node. No flowfiles are generated. Yielding...");
      yield();
    }
  }

  bool FetchOPCProcessor::nodeFoundCallBack(opc::Client& /*client*/, const UA_ReferenceDescription *ref, const std::string& path,
      const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
    nodesFound_++;
    if (ref->nodeClass == UA_NODECLASS_VARIABLE) {
      try {
        opc::NodeData nodedata = connection_->getNodeData(ref, path);
        bool write = true;
        if (lazy_mode_) {
          write = false;
          std::string nodeid = nodedata.attributes["Full path"];
          std::string cur_timestamp = node_timestamp_[nodeid];
          std::string new_timestamp = nodedata.attributes["Sourcetimestamp"];
          if (cur_timestamp != new_timestamp) {
            node_timestamp_[nodeid] = new_timestamp;
            logger_->log_debug("Node %s has new source timestamp %s", nodeid, new_timestamp);
            write = true;
          }
        }
        if (write) {
          OPCData2FlowFile(nodedata, context, session);
          variablesFound_++;
        }
      } catch (const std::exception& exception) {
        std::string browse_name(reinterpret_cast<char*>(ref->browseName.name.data), ref->browseName.name.length);
        logger_->log_warn("Caught Exception while trying to get data from node %s: %s", path + "/" + browse_name, exception.what());
      }
    }
    return true;
  }

  void FetchOPCProcessor::OPCData2FlowFile(const opc::NodeData& opcnode, const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession> &session) {
    auto flowFile = session->create();
    if (flowFile == nullptr) {
      logger_->log_error("Failed to create flowfile!");
      return;
    }
    for (const auto& attr : opcnode.attributes) {
      flowFile->setAttribute(attr.first, attr.second);
    }
    if (opcnode.data.size() > 0) {
      try {
        FetchOPCProcessor::WriteCallback callback(opc::nodeValue2String(opcnode));
        session->write(flowFile, &callback);
      } catch (const std::exception& e) {
        std::string browsename;
        flowFile->getAttribute("Browsename", browsename);
        logger_->log_info("Failed to extract data of OPC node %s: %s", browsename, e.what());
        session->transfer(flowFile, Failure);
        return;
      }
    }
    session->transfer(flowFile, Success);
  }

REGISTER_RESOURCE(FetchOPCProcessor, "Fetches OPC-UA node");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
