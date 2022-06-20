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

namespace org::apache::nifi::minifi::core::yaml {

void YamlConnectionParser::addNewRelationshipToConnection(const std::string& relationship_name, minifi::Connection& connection) const {
  core::Relationship relationship(relationship_name, "");
  logger_->log_debug("parseConnection: relationship => [%s]", relationship_name);
  connection.addRelationship(std::move(relationship));
}

void YamlConnectionParser::addFunnelRelationshipToConnection(minifi::Connection& connection) const {
  utils::Identifier srcUUID;
  try {
    srcUUID = getSourceUUIDFromYaml();
  } catch(const std::exception&) {
    return;
  }
  auto processor = parent_->findProcessorById(srcUUID);
  if (!processor) {
    logger_->log_error("Could not find processor with id %s", srcUUID.to_string());
    return;
  }

  auto& processor_ref = *processor;
  if (typeid(minifi::core::Funnel) == typeid(processor_ref)) {
    addNewRelationshipToConnection(minifi::core::Funnel::Success.getName(), connection);
  }
}

void YamlConnectionParser::configureConnectionSourceRelationshipsFromYaml(minifi::Connection& connection) const {
  // Configure connection source
  if (connectionNode_.as<YAML::Node>()["source relationship name"] && !connectionNode_["source relationship name"].as<std::string>().empty()) {
    addNewRelationshipToConnection(connectionNode_["source relationship name"].as<std::string>(), connection);
  } else if (connectionNode_.as<YAML::Node>()["source relationship names"]) {
    auto relList = connectionNode_["source relationship names"];
    if (relList.IsSequence() && relList.begin() != relList.end()) {
      for (const auto &rel : relList) {
        addNewRelationshipToConnection(rel.as<std::string>(), connection);
      }
    } else if (!relList.IsSequence() && !relList.as<std::string>().empty()) {
      addNewRelationshipToConnection(relList.as<std::string>(), connection);
    } else {
      addFunnelRelationshipToConnection(connection);
    }
  } else {
    addFunnelRelationshipToConnection(connection);
  }
}

uint64_t YamlConnectionParser::getWorkQueueSizeFromYaml() const {
  const YAML::Node max_work_queue_data_size_node = connectionNode_["max work queue size"];
  if (max_work_queue_data_size_node) {
    auto max_work_queue_str = max_work_queue_data_size_node.as<std::string>();
    uint64_t max_work_queue_size;
    if (core::Property::StringToInt(max_work_queue_str, max_work_queue_size)) {
      logger_->log_debug("Setting %" PRIu64 " as the max queue size.", max_work_queue_size);
      return max_work_queue_size;
    }
    logger_->log_info("Invalid max queue size value: %s.", max_work_queue_str);
  }
  return 0;
}

uint64_t YamlConnectionParser::getWorkQueueDataSizeFromYaml() const {
  const YAML::Node max_work_queue_data_size_node = connectionNode_["max work queue data size"];
  if (max_work_queue_data_size_node) {
    auto max_work_queue_str = max_work_queue_data_size_node.as<std::string>();
    uint64_t max_work_queue_data_size = 0;
    if (core::Property::StringToInt(max_work_queue_str, max_work_queue_data_size)) {
      logger_->log_debug("Setting %" PRIu64 "as the max as the max queue data size.", max_work_queue_data_size);
      return max_work_queue_data_size;
    }
    logger_->log_info("Invalid max queue data size value: %s.", max_work_queue_str);
  }
  return 0;
}

utils::Identifier YamlConnectionParser::getSourceUUIDFromYaml() const {
  const YAML::Node source_id_node = connectionNode_["source id"];
  if (source_id_node) {
    const auto srcUUID = utils::Identifier::parse(source_id_node.as<std::string>());
    if (srcUUID) {
      logger_->log_debug("Using 'source id' to match source with same id for connection '%s': source id => [%s]", name_, srcUUID.value().to_string());
      return srcUUID.value();
    }
    logger_->log_error("Invalid source id value: %s.", source_id_node.as<std::string>());
    throw std::invalid_argument("Invalid source id");
  }
  // if we don't have a source id, try to resolve using source name. config schema v2 will make this unnecessary
  checkRequiredField(connectionNode_, "source name", CONFIG_YAML_CONNECTIONS_KEY);
  const auto connectionSrcProcName = connectionNode_["source name"].as<std::string>();
  const auto srcUUID = utils::Identifier::parse(connectionSrcProcName);
  if (srcUUID && parent_->findProcessorById(srcUUID.value(), ProcessGroup::Traverse::ExcludeChildren)) {
    // the source name is a remote port id, so use that as the source id
    logger_->log_debug("Using 'source name' containing a remote port id to match the source for connection '%s': source name => [%s]", name_, connectionSrcProcName);
    return srcUUID.value();
  }
  // lastly, look the processor up by name
  auto srcProcessor = parent_->findProcessorByName(connectionSrcProcName, ProcessGroup::Traverse::ExcludeChildren);
  if (srcProcessor) {
    logger_->log_debug("Using 'source name' to match source with same name for connection '%s': source name => [%s]", name_, connectionSrcProcName);
    return srcProcessor->getUUID();
  }
  // we ran out of ways to discover the source processor
  const std::string error_msg = "Could not locate a source with name " + connectionSrcProcName + " to create a connection ";
  logger_->log_error(error_msg.c_str());
  throw std::invalid_argument(error_msg);
}

utils::Identifier YamlConnectionParser::getDestinationUUIDFromYaml() const {
  const YAML::Node destination_id_node = connectionNode_["destination id"];
  if (destination_id_node) {
    const auto destUUID = utils::Identifier::parse(destination_id_node.as<std::string>());
    if (destUUID) {
      logger_->log_debug("Using 'destination id' to match destination with same id for connection '%s': destination id => [%s]", name_, destUUID.value().to_string());
      return destUUID.value();
    }
    logger_->log_error("Invalid destination id value: %s.", destination_id_node.as<std::string>());
    throw std::invalid_argument("Invalid destination id");
  }
  // we use the same logic as above for resolving the source processor
  // for looking up the destination processor in absence of a processor id
  checkRequiredField(connectionNode_, "destination name", CONFIG_YAML_CONNECTIONS_KEY);
  auto connectionDestProcName = connectionNode_["destination name"].as<std::string>();
  const auto destUUID = utils::Identifier::parse(connectionDestProcName);
  if (destUUID && parent_->findProcessorById(destUUID.value(), ProcessGroup::Traverse::ExcludeChildren)) {
    // the destination name is a remote port id, so use that as the dest id
    logger_->log_debug("Using 'destination name' containing a remote port id to match the destination for connection '%s': destination name => [%s]", name_, connectionDestProcName);
    return destUUID.value();
  }
  // look the processor up by name
  auto destProcessor = parent_->findProcessorByName(connectionDestProcName, ProcessGroup::Traverse::ExcludeChildren);
  if (destProcessor) {
    logger_->log_debug("Using 'destination name' to match destination with same name for connection '%s': destination name => [%s]", name_, connectionDestProcName);
    return destProcessor->getUUID();
  }
  // we ran out of ways to discover the destination processor
  const std::string error_msg = "Could not locate a destination with name " + connectionDestProcName + " to create a connection";
  logger_->log_error(error_msg.c_str());
  throw std::invalid_argument(error_msg);
}

std::chrono::milliseconds YamlConnectionParser::getFlowFileExpirationFromYaml() const {
  using namespace std::literals::chrono_literals;
  const YAML::Node expiration_node = connectionNode_["flowfile expiration"];
  if (!expiration_node) {
    logger_->log_debug("parseConnection: flowfile expiration is not set, assuming 0 (never expire)");
    return 0ms;
  }
  auto expiration_duration = utils::timeutils::StringToDuration<std::chrono::milliseconds>(expiration_node.as<std::string>());
  if (!expiration_duration.has_value()) {
    // We should throw here, but we do not.
    // The reason is that our parser only accepts time formats that consists of a number and
    // a unit, but users might use this field populated with a "0" (and no units).
    // We cannot correct this, because there is no API contract for the config, we need to support
    // all already-supported configuration files.
    // This has the side-effect of allowing values like "20 minuites" and silently defaulting to 0.
    logger_->log_debug("Parsing failure for flowfile expiration duration");
    expiration_duration = 0ms;
  }
  logger_->log_debug("parseConnection: flowfile expiration => [%d]", expiration_duration->count());
  return *expiration_duration;
}

bool YamlConnectionParser::getDropEmptyFromYaml() const {
  const YAML::Node drop_empty_node = connectionNode_["drop empty"];
  if (drop_empty_node) {
    return utils::StringUtils::toBool(drop_empty_node.as<std::string>()).value_or(false);
  }
  return false;
}

}  // namespace org::apache::nifi::minifi::core::yaml
