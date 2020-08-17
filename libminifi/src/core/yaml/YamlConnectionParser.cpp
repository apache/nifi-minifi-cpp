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

#include "core/yaml/YamlConnectionParser.h"
#include "core/yaml/CheckRequiredField.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace yaml {

// This is no longer needed in c++17
constexpr const char* YamlConnectionParser::CONFIG_YAML_CONNECTIONS_KEY;

void YamlConnectionParser::configureConnectionSourceRelationshipsFromYaml(const std::shared_ptr<minifi::Connection>& connection) const {
  auto addNewRelationshipToConnection = [&] (const std::string& relationship_name) {
    core::Relationship relationship(relationship_name, "");
    logger_->log_debug("parseConnection: relationship => [%s]", relationship_name);
    connection->addRelationship(std::move(relationship));
  };
  // Configure connection source
  if (connectionNode_.as<YAML::Node>()["source relationship name"]) {
    addNewRelationshipToConnection(connectionNode_["source relationship name"].as<std::string>());
  } else if (connectionNode_.as<YAML::Node>()["source relationship names"]) {
    auto relList = connectionNode_["source relationship names"];
      if (relList.IsSequence()) {
        for (const auto &rel : relList) {
          addNewRelationshipToConnection(rel.as<std::string>());
        }
      } else {
        addNewRelationshipToConnection(relList.as<std::string>());
      }
  }
}

uint64_t YamlConnectionParser::getWorkQueueSizeFromYaml() const {
  const YAML::Node max_work_queue_data_size_node = connectionNode_["max work queue size"];
  if (max_work_queue_data_size_node) {
    auto max_work_queue_str = max_work_queue_data_size_node.as<std::string>();
    uint64_t max_work_queue_size;
    if (core::Property::StringToInt(max_work_queue_str, max_work_queue_size)) {
      logger_->log_debug("Setting %u as the max queue size.", max_work_queue_size);
      return max_work_queue_size;
    }
  }
  return 0;
}

uint64_t YamlConnectionParser::getWorkQueueDataSizeFromYaml() const {
  const YAML::Node max_work_queue_data_size_node = connectionNode_["max work queue data size"];
  if (max_work_queue_data_size_node) {
    auto max_work_queue_str = max_work_queue_data_size_node.as<std::string>();
    uint64_t max_work_queue_data_size = 0;
    if (core::Property::StringToInt(max_work_queue_str, max_work_queue_data_size)) {
      return max_work_queue_data_size;
      logger_->log_debug("Setting %u as the max as the max queue data size.", max_work_queue_data_size);
    }
  }
  return 0;
}

utils::Identifier YamlConnectionParser::getSourceUUIDFromYaml() const {
  const YAML::Node source_id_node = connectionNode_["source id"];
  if (source_id_node) {
    const utils::Identifier srcUUID{ source_id_node.as<std::string>() };
    logger_->log_debug("Using 'source id' to match source with same id for connection '%s': source id => [%s]", name_, srcUUID.to_string());
    return srcUUID;
  }
  // if we don't have a source id, try to resolve using source name. config schema v2 will make this unnecessary
  checkRequiredField(&connectionNode_, "source name", logger_, CONFIG_YAML_CONNECTIONS_KEY);
  const std::string connectionSrcProcName = connectionNode_["source name"].as<std::string>();
  if (nullptr != parent_->findProcessorById(utils::Identifier{connectionSrcProcName})) {
    // the source name is a remote port id, so use that as the source id
    logger_->log_debug("Using 'source name' containing a remote port id to match the source for connection '%s': source name => [%s]", name_, connectionSrcProcName);
    return utils::Identifier{ connectionSrcProcName };
  }
  // lastly, look the processor up by name
  auto srcProcessor = parent_->findProcessorByName(connectionSrcProcName);
  if (nullptr != srcProcessor) {
    logger_->log_debug("Using 'source name' to match source with same name for connection '%s': source name => [%s]", name_, connectionSrcProcName);
    utils::Identifier srcUUID;
    srcProcessor->getUUID(srcUUID);
    return srcUUID;
  }
  // we ran out of ways to discover the source processor
  const std::string error_msg = "Could not locate a source with name " + connectionSrcProcName + " to create a connection ";
  logger_->log_error(error_msg.c_str());
  throw std::invalid_argument(error_msg);
}

utils::Identifier YamlConnectionParser::getDestinationUUIDFromYaml() const {
  const YAML::Node destination_id_node = connectionNode_["destination id"];
  if (destination_id_node) {
    const std::string dest_uuid_str{ destination_id_node.as<std::string>() };
    logger_->log_debug("Using 'destination id' to match destination with same id for connection '%s': destination id => [%s]", name_, dest_uuid_str.c_str());
    return utils::Identifier{dest_uuid_str};
  }
  // we use the same logic as above for resolving the source processor
  // for looking up the destination processor in absence of a processor id
  checkRequiredField(&connectionNode_, "destination name", logger_, CONFIG_YAML_CONNECTIONS_KEY);
  std::string connectionDestProcName = connectionNode_["destination name"].as<std::string>();
  if (parent_->findProcessorById(utils::Identifier{connectionDestProcName})) {
    // the destination name is a remote port id, so use that as the dest id
    logger_->log_debug("Using 'destination name' containing a remote port id to match the destination for connection '%s': destination name => [%s]", name_, connectionDestProcName);
    return utils::Identifier{ connectionDestProcName };
  }
  // look the processor up by name
  auto destProcessor = parent_->findProcessorByName(connectionDestProcName);
  if (NULL != destProcessor) {
    logger_->log_debug("Using 'destination name' to match destination with same name for connection '%s': destination name => [%s]", name_, connectionDestProcName);
    utils::Identifier destUUID;
    destProcessor->getUUID(destUUID);
    return destUUID;
  }
  // we ran out of ways to discover the destination processor
  const std::string error_msg = "Could not locate a destination with name " + connectionDestProcName + " to create a connection";
  logger_->log_error(error_msg.c_str());
  throw std::invalid_argument(error_msg);
}

uint64_t YamlConnectionParser::getFlowFileExpirationFromYaml() const {
  const YAML::Node expiration_node = connectionNode_["flowfile expiration"];
  if (!expiration_node) {
    return 0;
  }
  uint64_t expirationDuration = 0;
  TimeUnit unit;
  const std::string flowfile_expiration_str = expiration_node.as<std::string>();
  if (!core::Property::StringToTime(flowfile_expiration_str, expirationDuration, unit) || !core::Property::ConvertTimeUnitToMS(expirationDuration, unit, expirationDuration)) {
    // We should throw here, but we do not.
    // The reason is that our parser only accepts time formats that consists of a number and
    // a unit, but users might use this field populated with a "0" (and no units).
    // We cannot correct this, because there is no API contract for the config, we need to support
    // all already-supported configuration files.
    // This has the side-effect of allowing values like "20 minuites" and silently defaulting to 0.
    logger_->log_debug("parseConnection: flowfile expiration => [%d]", expirationDuration);
  }
  return expirationDuration;
}

bool YamlConnectionParser::getDropEmptyFromYaml() const {
  const YAML::Node drop_empty_node = connectionNode_["drop empty"];
  if (drop_empty_node) {
    bool dropEmpty = false;
    return utils::StringUtils::StringToBool(drop_empty_node.as<std::string>(), dropEmpty) && dropEmpty;
  }
  return false;
}

}  // namespace yaml
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
