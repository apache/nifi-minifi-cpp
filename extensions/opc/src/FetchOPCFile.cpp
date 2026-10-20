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
#include "FetchOPCFile.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/Resource.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::processors {

namespace {
constexpr const char* NODE_ID_STATE_KEY = "node_id";
constexpr const char* SIZE_STATE_KEY = "size";
constexpr const char* LAST_MODIFIED_TIME_STATE_KEY = "last_modified_time";
}  // namespace

void FetchOPCFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchOPCFile::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) {
  logger_->log_trace("FetchOPCFile::onSchedule");

  BaseOPCProcessor::onSchedule(context, factory);

  node_id_ = utils::parseProperty(context, NodeID);
  id_type_ = utils::parseEnumProperty<opc::OPCNodeIDType>(context, NodeIDType);
  namespace_idx_ = gsl::narrow<UA_UInt16>(utils::parseU64Property(context, NameSpaceIndex));
  parseNode(context, PathReferenceTypes);
  lazy_mode_ = utils::parseEnumProperty<FetchOPCFileLazyModeOptions>(context, Lazy);
  cached_file_.reset();
}

bool FetchOPCFile::reconnect() {
  const bool was_connected = connection_ && connection_->isConnected();
  if (!BaseOPCProcessor::reconnect()) {
    return false;
  }
  if (!was_connected) {
    cached_file_.reset();
  }
  return true;
}

void FetchOPCFile::invalidateFile() {
  cached_file_.reset();
  path_node_id_resolved_ = false;
}

bool FetchOPCFile::resolveFile() {
  if (cached_file_) {
    return true;
  }
  auto remote_file = opc::RemoteFile::create(*connection_, opc::NodeId::copyOf(node_));
  if (!remote_file) {
    logger_->log_error("Failed to fetch file node '{}' or find its file methods: {}", node_id_, UA_StatusCode_name(remote_file.error()));
    invalidateFile();
    return false;
  }
  std::optional<opc::NodeId> size_node_id;
  std::optional<opc::NodeId> last_modified_time_node_id;
  if (lazy_mode_ == FetchOPCFileLazyModeOptions::On) {
    // Size is a mandatory, LastModifiedTime is an optional property of FileType nodes (OPC UA Part 20, 4.2.1)
    size_node_id = connection_->findChildByQualifiedName(node_, "Size", 0, UA_NS0ID_HASPROPERTY) | utils::toOptional();
    last_modified_time_node_id = connection_->findChildByQualifiedName(node_, "LastModifiedTime", 0, UA_NS0ID_HASPROPERTY) | utils::toOptional();
    if (!last_modified_time_node_id && size_node_id) {
      logger_->log_warn("File node '{}' has no LastModifiedTime property, lazy mode can only detect the changes of its Size", node_id_);
    } else if (!last_modified_time_node_id) {
      logger_->log_warn("File node '{}' has neither LastModifiedTime nor Size property, lazy mode cannot detect its changes, it is fetched on every trigger", node_id_);
    }
  }
  auto browse_name = connection_->readBrowseName(node_) | utils::toOptional();
  if (!browse_name) {
    logger_->log_warn("Failed to read browse name of file node '{}'", node_id_);
  }
  cached_file_.emplace(CachedFile{
      .remote_file = std::move(*remote_file),
      .browse_name = std::move(browse_name),
      .size_node_id = std::move(size_node_id),
      .last_modified_time_node_id = std::move(last_modified_time_node_id)});
  return true;
}

std::optional<core::StateManager::State> FetchOPCFile::readFileState() {
  core::StateManager::State state;
  if (cached_file_->size_node_id) {
    if (auto size = connection_->readUInt64Value(*cached_file_->size_node_id)) {
      state[SIZE_STATE_KEY] = std::to_string(*size);
    }
  }
  if (cached_file_->last_modified_time_node_id) {
    if (auto last_modified_time = connection_->readDateTimeValue(*cached_file_->last_modified_time_node_id)) {
      state[LAST_MODIFIED_TIME_STATE_KEY] = std::to_string(*last_modified_time);
    }
  }
  if (state.empty()) {
    return std::nullopt;
  }
  state[NODE_ID_STATE_KEY] = fmt::format("{};{};{}", magic_enum::enum_name(id_type_), namespace_idx_, node_id_);
  return state;
}

bool FetchOPCFile::writeOPCFileToFlowFile(opc::RemoteFile& remote_file, opc::RemoteFileHandle file_handle, core::ProcessSession& session,
    const std::shared_ptr<core::FlowFile>& flow_file) {
  UA_StatusCode read_status = UA_STATUSCODE_GOOD;
  session.write(flow_file, [&](const std::shared_ptr<io::OutputStream>& output_stream) -> io::IoResult {
    size_t total_size = 0;
    while (true) {
      auto chunk = remote_file.read(file_handle, remote_file.chunkSize());
      if (!chunk) {
        read_status = chunk.error();
        return io::IoResult::cancelled();
      }
      const auto data = chunk->data();
      if (data.empty()) {
        return io::IoResult::from(total_size);
      }
      const auto written = output_stream->write(data);
      if (io::isError(written)) {
        return io::IoResult::error();
      }
      total_size += data.size();
    }
  });

  if (read_status != UA_STATUSCODE_GOOD) {
    logger_->log_error("Failed to read file node '{}': {}", node_id_, UA_StatusCode_name(read_status));
    session.remove(flow_file);
    return false;
  }

  if (auto sc = file_handle.close(); !sc) {
    logger_->log_warn("Failed to close file node '{}' after reading it: {}", node_id_, UA_StatusCode_name(sc.error()));
  }
  return true;
}

void FetchOPCFile::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("FetchOPCFile::onTrigger");

  if (!reconnect()) {
    logger_->log_warn("Could not connect to OPC server, yielding");
    context.yield();
    return;
  }

  if (auto resolved = resolvePathNode(); !resolved) {
    logger_->log_error("Failed to resolve the file node: {}", resolved.error());
    context.yield();
    return;
  }

  if (!resolveFile()) {
    context.yield();
    return;
  }

  auto* state_manager = context.getStateManager();
  std::optional<core::StateManager::State> file_state;
  if (lazy_mode_ == FetchOPCFileLazyModeOptions::On) {
    if (state_manager == nullptr) {
      logger_->log_error("Failed to get StateManager, lazy mode cannot be used");
      context.yield();
      return;
    }
    file_state = readFileState();
    if (file_state && state_manager->get() == file_state) {
      logger_->log_debug("File node '{}' has not changed since the last fetch, skipping", node_id_);
      context.yield();
      return;
    }
  }

  auto handle = cached_file_->remote_file.open(opc::FILE_MODE_READ);
  if (!handle) {
    logger_->log_error("Failed to open file node '{}' for reading: {}", node_id_, UA_StatusCode_name(handle.error()));
    invalidateFile();
    context.yield();
    return;
  }

  auto flow_file = session.create();
  if (!writeOPCFileToFlowFile(cached_file_->remote_file, std::move(*handle), session, flow_file)) {
    invalidateFile();
    context.yield();
    return;
  }

  for (const auto& [key, value] : opc::nodeIdAttributes(node_)) {
    flow_file->setAttribute(key, value);
  }
  if (cached_file_->browse_name) {
    flow_file->setAttribute(BrowsenameAttr.name, *cached_file_->browse_name);
  }

  if (file_state && !state_manager->set(*file_state)) {
    logger_->log_warn("Failed to store the state of file node '{}', it may be fetched again even if it has not changed", node_id_);
  }
  session.transfer(flow_file, Success);
}

REGISTER_RESOURCE(FetchOPCFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
