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

#include "core/flow/StructuredConnectionParser.h"
#include "core/flow/CheckRequiredField.h"
#include "Funnel.h"

namespace org::apache::nifi::minifi::core::flow {

void StructuredConnectionParser::addNewRelationshipToConnection(std::string_view relationship_name, minifi::Connection& connection) const {
  core::Relationship relationship(std::string(relationship_name), "");
  logger_->log_debug("parseConnection: relationship => [{}]", relationship_name);
  connection.addRelationship(relationship);
}

void StructuredConnectionParser::addFunnelRelationshipToConnection(minifi::Connection& connection) const {
  utils::Identifier srcUUID;
  try {
    srcUUID = getSourceUUID();
  } catch(const std::exception&) {
    return;
  }
  auto processor = parent_->findProcessorById(srcUUID);
  if (!processor) {
    logger_->log_error("Could not find processor with id {}", srcUUID.to_string());
    return;
  }

  auto& processor_ref = *processor;
  if (typeid(minifi::Funnel) == typeid(processor_ref)) {
    addNewRelationshipToConnection(minifi::Funnel::Success.name, connection);
  }
}

void StructuredConnectionParser::configureConnectionSourceRelationships(minifi::Connection& connection) const {
  // Configure connection source
  if (connectionNode_[schema_.source_relationship] && !connectionNode_[schema_.source_relationship].getString().value().empty()) {
    addNewRelationshipToConnection(connectionNode_[schema_.source_relationship].getString().value(), connection);
  } else if (auto relList = connectionNode_[schema_.source_relationship_list]) {
    if (relList.isSequence() && !relList.empty()) {
      for (const auto &rel : relList) {
        addNewRelationshipToConnection(rel.getString().value(), connection);
      }
    } else if (!relList.isSequence() && !relList.getString().value().empty()) {
      addNewRelationshipToConnection(relList.getString().value(), connection);
    } else {
      addFunnelRelationshipToConnection(connection);
    }
  } else {
    addFunnelRelationshipToConnection(connection);
  }
}

uint64_t StructuredConnectionParser::getWorkQueueSize() const {
  if (const auto max_work_queue_data_size_node = connectionNode_[schema_.max_queue_size]) {
    std::string max_work_queue_str = max_work_queue_data_size_node.getIntegerAsString().value();
    if (const auto max_work_queue_size = parsing::parseIntegral<uint64_t>(max_work_queue_str)) {
      logger_->log_debug("Setting {} as the max queue size.", *max_work_queue_size);
      return *max_work_queue_size;
    }
    logger_->log_error("Invalid max queue size value: {}.", max_work_queue_str);
  }
  return Connection::DEFAULT_BACKPRESSURE_THRESHOLD_COUNT;
}

uint64_t StructuredConnectionParser::getWorkQueueDataSize() const {
  const flow::Node max_work_queue_data_size_node = connectionNode_[schema_.max_queue_data_size];
  if (max_work_queue_data_size_node) {
    const std::string max_work_queue_data_size_str = max_work_queue_data_size_node.getIntegerAsString().value();
    if (const auto max_work_queue_data_size = parsing::parseDataSize(max_work_queue_data_size_str)) {
      logger_->log_debug("Setting {} as the max as the max queue data size.", *max_work_queue_data_size);
      return *max_work_queue_data_size;
    }
    logger_->log_error("Invalid max queue data size value: {}.", max_work_queue_data_size_str);
  }
  return Connection::DEFAULT_BACKPRESSURE_THRESHOLD_DATA_SIZE;
}

uint64_t StructuredConnectionParser::getSwapThreshold() const {
  const flow::Node swap_threshold_node = connectionNode_[schema_.swap_threshold];
  if (swap_threshold_node) {
    auto swap_threshold_str = swap_threshold_node.getString().value();
    if (const auto swap_threshold = parsing::parseDataSize(swap_threshold_str)) {
      logger_->log_debug("Setting {} as the swap threshold.", *swap_threshold);
      return *swap_threshold;
    }
    logger_->log_error("Invalid swap threshold value: {}.", swap_threshold_str);
  }
  return 0;
}

utils::Identifier StructuredConnectionParser::getSourceUUID() const {
  const flow::Node source_id_node = connectionNode_[schema_.source_id];
  if (source_id_node) {
    const auto srcUUID = utils::Identifier::parse(source_id_node.getString().value());
    if (srcUUID) {
      logger_->log_debug("Using 'source id' to match source with same id for connection '{}': source id => [{}]", name_, srcUUID.value().to_string());
      return srcUUID.value();
    }
    logger_->log_error("Invalid source id value: {}.", source_id_node.getString().value());
    throw std::invalid_argument("Invalid source id");
  }
  // if we don't have a source id, try to resolve using source name. config schema v2 will make this unnecessary
  checkRequiredField(connectionNode_, schema_.source_name);
  const auto connectionSrcProcName = connectionNode_[schema_.source_name].getString().value();
  const auto srcUUID = utils::Identifier::parse(connectionSrcProcName);
  if (srcUUID && parent_->findProcessorById(srcUUID.value(), ProcessGroup::Traverse::ExcludeChildren)) {
    // the source name is a remote port id, so use that as the source id
    logger_->log_debug("Using 'source name' containing a remote port id to match the source for connection '{}': source name => [{}]", name_, connectionSrcProcName);
    return srcUUID.value();
  }
  // lastly, look the processor up by name
  auto srcProcessor = parent_->findProcessorByName(connectionSrcProcName, ProcessGroup::Traverse::ExcludeChildren);
  if (srcProcessor) {
    logger_->log_debug("Using 'source name' to match source with same name for connection '{}': source name => [{}]", name_, connectionSrcProcName);
    return srcProcessor->getUUID();
  }
  // we ran out of ways to discover the source processor
  const std::string error_msg = "Could not locate a source with name " + connectionSrcProcName + " to create a connection ";
  logger_->log_error("{}", error_msg);
  throw std::invalid_argument(error_msg);
}

utils::Identifier StructuredConnectionParser::getDestinationUUID() const {
  const flow::Node destination_id_node = connectionNode_[schema_.destination_id];
  if (destination_id_node) {
    const auto destUUID = utils::Identifier::parse(destination_id_node.getString().value());
    if (destUUID) {
      logger_->log_debug("Using 'destination id' to match destination with same id for connection '{}': destination id => [{}]", name_, destUUID.value().to_string());
      return destUUID.value();
    }
    logger_->log_error("Invalid destination id value: {}.", destination_id_node.getString().value());
    throw std::invalid_argument("Invalid destination id");
  }
  // we use the same logic as above for resolving the source processor
  // for looking up the destination processor in absence of a processor id
  checkRequiredField(connectionNode_, schema_.destination_name);
  auto connectionDestProcName = connectionNode_[schema_.destination_name].getString().value();
  const auto destUUID = utils::Identifier::parse(connectionDestProcName);
  if (destUUID && parent_->findProcessorById(destUUID.value(), ProcessGroup::Traverse::ExcludeChildren)) {
    // the destination name is a remote port id, so use that as the dest id
    logger_->log_debug("Using 'destination name' containing a remote port id to match the destination for connection '{}': destination name => [{}]", name_, connectionDestProcName);
    return destUUID.value();
  }
  // look the processor up by name
  auto destProcessor = parent_->findProcessorByName(connectionDestProcName, ProcessGroup::Traverse::ExcludeChildren);
  if (destProcessor) {
    logger_->log_debug("Using 'destination name' to match destination with same name for connection '{}': destination name => [{}]", name_, connectionDestProcName);
    return destProcessor->getUUID();
  }
  // we ran out of ways to discover the destination processor
  const std::string error_msg = "Could not locate a destination with name " + connectionDestProcName + " to create a connection";
  logger_->log_error("{}", error_msg.c_str());
  throw std::invalid_argument(error_msg);
}

std::chrono::milliseconds StructuredConnectionParser::getFlowFileExpiration() const {
  using namespace std::literals::chrono_literals;
  const flow::Node expiration_node = connectionNode_[schema_.flowfile_expiration];
  if (!expiration_node) {
    logger_->log_debug("parseConnection: flowfile expiration is not set, assuming 0 (never expire)");
    return 0ms;
  }
  auto expiration_duration = utils::timeutils::StringToDuration<std::chrono::milliseconds>(expiration_node.getString().value());
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
  logger_->log_debug("parseConnection: flowfile expiration => [{}]", expiration_duration);
  return *expiration_duration;
}

bool StructuredConnectionParser::getDropEmpty() const {
  const flow::Node drop_empty_node = connectionNode_[schema_.drop_empty];
  if (drop_empty_node) {
    return utils::string::toBool(drop_empty_node.getString().value()).value_or(false);
  }
  return false;
}

}  // namespace org::apache::nifi::minifi::core::flow
