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
#include "PutOPCFile.h"

#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "OPCFileTransfer.h"
#include "core/Resource.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

void PutOPCFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PutOPCFile::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) {
  logger_->log_trace("PutOPCFile::onSchedule");

  BaseOPCProcessor::onSchedule(context, factory);

  target_node_id_defined_ = context.hasNonEmptyProperty(TargetNodeID.name);
  parent_node_defined_ = false;
  if (const auto parent_node_id = utils::parseOptionalProperty(context, ParentNodeID); parent_node_id && !parent_node_id->empty()) {
    node_id_ = *parent_node_id;
    id_type_ = utils::parseEnumProperty<opc::OPCNodeIDType>(context, ParentNodeIDType);
    namespace_idx_ = gsl::narrow<UA_UInt16>(utils::parseOptionalU64Property(context, ParentNameSpaceIndex).value_or(0));
    parseNode(context, PathReferenceTypes);
    parent_node_defined_ = true;
  }
  if (!target_node_id_defined_ && !parent_node_defined_) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Either Target node ID or Parent node ID must be set");
  }

  write_mode_ = utils::parseEnumProperty<OPCFileWriteMode>(context, WriteMode);
}

std::expected<opc::NodeId, std::string> PutOPCFile::findOrCreateFileInParent(core::ProcessContext& context, const core::FlowFile& flow_file) {
  if (!parent_node_defined_) {
    return std::unexpected{"target file node does not exist and no parent node is defined to create it in"};
  }

  const auto browse_name = context.getProperty(TargetNodeBrowseName, &flow_file).value_or("");
  if (browse_name.empty()) {
    return std::unexpected{"target node browse name is required to look up or create the file in the parent node"};
  }

  if (auto resolved = resolvePathNode(); !resolved) {
    return std::unexpected{fmt::format("failed to resolve the parent node: {}", resolved.error())};
  }

  // The node ID of a created file is assigned by the server, so a file created earlier is found by its browse name.
  // FileDirectoryType nodes reference their files with Organizes references (OPC UA Part 20, 4.3.1).
  if (auto file_node = connection_->findObjectChildByName(node_, browse_name, UA_NS0ID_ORGANIZES)) {
    return std::move(*file_node);
  } else if (file_node.error() != UA_STATUSCODE_BADNOMATCH) {
    return std::unexpected{fmt::format("failed to browse parent node '{}': {}", node_id_, UA_StatusCode_name(file_node.error()))};
  }

  logger_->log_debug("File '{}' not found in parent node '{}', creating it", browse_name, node_id_);
  auto created_file_node = opc::createFileInDirectory(*connection_, node_, browse_name);
  if (!created_file_node) {
    return std::unexpected{
        fmt::format("failed to create file '{}' in parent node '{}': {}", browse_name, node_id_, UA_StatusCode_name(created_file_node.error()))};
  }
  return std::move(*created_file_node);
}

std::expected<opc::NodeId, std::string> PutOPCFile::getTargetFileNode(core::ProcessContext& context, const core::FlowFile& flow_file) {
  if (target_node_id_defined_) {
    auto target_node = resolveTargetNode(context, flow_file, {TargetNodeIDType, TargetNodeID, TargetNodeNameSpaceIndex, TargetPathReferenceTypes});
    if (!target_node) {
      return std::unexpected{target_node.error()};
    }
    if (target_node->exists) {
      return std::move(target_node->node_id);
    }
    logger_->log_debug("Target file node does not exist, looking it up in the parent node");
  }
  return findOrCreateFileInParent(context, flow_file);
}

void PutOPCFile::addFileNodeAttributes(const UA_NodeId& file_node_id, core::FlowFile& flow_file) const {
  for (const auto& [key, value] : opc::nodeIdAttributes(file_node_id)) {
    flow_file.setAttribute(key, value);
  }
  if (auto browse_name = connection_->readBrowseName(file_node_id)) {
    flow_file.setAttribute(BrowsenameAttr.name, *browse_name);
  }
}

std::expected<void, std::string> PutOPCFile::writeFile(const UA_NodeId& file_node_id, core::ProcessSession& session,
    const std::shared_ptr<core::FlowFile>& flow_file) const {
  auto remote_file = opc::RemoteFile::create(*connection_, opc::NodeId::copyOf(file_node_id));
  if (!remote_file) {
    return std::unexpected{
        fmt::format("target node is not a file node, failed to find its file methods: {}", UA_StatusCode_name(remote_file.error()))};
  }

  const UA_Byte mode = opc::FILE_MODE_WRITE | (write_mode_ == OPCFileWriteMode::Append ? opc::FILE_MODE_APPEND : opc::FILE_MODE_ERASE_EXISTING);
  auto file_handle = remote_file->open(mode);
  if (!file_handle) {
    return std::unexpected{fmt::format("failed to open file for writing: {}", UA_StatusCode_name(file_handle.error()))};
  }

  std::expected<void, UA_StatusCode> write_status;
  session.read(flow_file, [&](const std::shared_ptr<io::InputStream>& input_stream) -> io::IoResult {
    std::vector<std::byte> buffer(std::min<uint64_t>(gsl::narrow<size_t>(remote_file->chunkSize()), flow_file->getSize()));
    size_t total_size = 0;
    while (true) {
      const auto read = input_stream->read(buffer);
      if (io::isError(read)) {
        return io::IoResult::error();
      }
      if (read == 0) {
        return io::IoResult::from(total_size);
      }
      auto write_result = remote_file->write(*file_handle, std::span<const std::byte>(buffer.data(), read));
      if (!write_result) {
        write_status = std::unexpected{write_result.error()};
        return io::IoResult::from(total_size);
      }
      total_size += read;
    }
  });

  if (!write_status) {
    return std::unexpected{fmt::format("failed to write file: {}", UA_StatusCode_name(write_status.error()))};
  }
  // Servers may only persist the written content on close, so its result has to be checked
  if (auto close_result = file_handle->close(); !close_result) {
    return std::unexpected{fmt::format("failed to close file: {}", UA_StatusCode_name(close_result.error()))};
  }
  return {};
}

void PutOPCFile::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("PutOPCFile::onTrigger");

  if (!reconnect()) {
    logger_->log_warn("Could not connect to OPC server, yielding");
    context.yield();
    return;
  }

  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  auto file_node = getTargetFileNode(context, *flow_file);
  if (!file_node) {
    logger_->log_error("Flowfile {}: {}, routing to failure", flow_file->getUUIDStr(), file_node.error());
    session.transfer(flow_file, Failure);
    return;
  }

  if (auto result = writeFile(*file_node, session, flow_file); !result) {
    logger_->log_error("Flowfile {}: {}, routing to failure", flow_file->getUUIDStr(), result.error());
    session.transfer(flow_file, Failure);
    return;
  }

  addFileNodeAttributes(*file_node, *flow_file);
  session.transfer(flow_file, Success);
}

REGISTER_RESOURCE(PutOPCFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
