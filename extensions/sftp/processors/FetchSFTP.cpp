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

#include "FetchSFTP.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>

#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/Relationship.h"
#include "core/Resource.h"
#include "io/BufferStream.h"
#include "utils/ConfigurationUtils.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

void FetchSFTP::initialize() {
  logger_->log_trace("Initializing FetchSFTP");

  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

FetchSFTP::~FetchSFTP() = default;

void FetchSFTP::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  parseCommonPropertiesOnSchedule(context);

  completion_strategy_ = utils::parseProperty(context, CompletionStrategy);
  create_directory_ = utils::parseBoolProperty(context, CreateDirectory);
  disable_directory_listing_ = utils::parseBoolProperty(context, DisableDirectoryListing);
  use_compression_ =  utils::parseBoolProperty(context, UseCompression);

  startKeepaliveThreadIfNeeded();
}

void FetchSFTP::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto flow_file = session.get();
  if (flow_file == nullptr) {
    return;
  }

  /* Parse common properties */
  SFTPProcessorBase::CommonProperties common_properties;
  if (!parseCommonPropertiesOnTrigger(context, flow_file.get(), common_properties)) {
    context.yield();
    return;
  }

  std::filesystem::path remote_file;
  if (auto remote_file_str = context.getProperty(RemoteFile, flow_file.get())) {
    remote_file = std::filesystem::path(*remote_file_str, std::filesystem::path::format::generic_format);
  }

  std::filesystem::path move_destination_directory;
  if (auto move_destination_directory_str = context.getProperty(MoveDestinationDirectory, flow_file.get())) {
    move_destination_directory = std::filesystem::path(*move_destination_directory_str, std::filesystem::path::format::generic_format);
  }

  /* Get SFTPClient from cache or create it */
  const SFTPProcessorBase::ConnectionCacheKey connection_cache_key = {common_properties.hostname,
                                                                      common_properties.port,
                                                                      common_properties.username,
                                                                      proxy_type_,
                                                                      common_properties.proxy_host,
                                                                      common_properties.proxy_port,
                                                                      common_properties.proxy_username};
  const auto buffer_size = utils::configuration::getBufferSize(*context.getConfiguration());
  auto client = getOrCreateConnection(connection_cache_key,
                                      common_properties.password,
                                      common_properties.private_key_path,
                                      common_properties.private_key_passphrase,
                                      common_properties.proxy_password,
                                      buffer_size);
  if (client == nullptr) {
    context.yield();
    return;
  }

  /*
   * Unless we're sure that the connection is good, we don't want to put it back to the cache.
   * So we will only call this when we're sure that the connection is OK.
   */
  auto put_connection_back_to_cache = [this, &connection_cache_key, &client]() {
    addConnectionToCache(connection_cache_key, std::move(client));
  };

  /* Download file */
  try {
    session.write(flow_file, [&remote_file, &client](const std::shared_ptr<io::OutputStream>& stream) -> int64_t {
      auto bytes_read = client->getFile(remote_file.generic_string(), *stream);
      if (!bytes_read) {
        throw utils::SFTPException{client->getLastError()};
      }
      return gsl::narrow<int64_t>(*bytes_read);
    });
  } catch (const utils::SFTPException& ex) {
    logger_->log_debug("{}", ex.what());
    switch (ex.error()) {
      case utils::SFTPError::PermissionDenied:
        session.transfer(flow_file, PermissionDenied);
        put_connection_back_to_cache();
        return;
      case utils::SFTPError::FileDoesNotExist:
        session.transfer(flow_file, NotFound);
        put_connection_back_to_cache();
        return;
      case utils::SFTPError::CommunicationFailure:
      case utils::SFTPError::IoError:
        session.transfer(flow_file, CommsFailure);
        return;
      default:
        session.transfer(flow_file, PermissionDenied);
        return;
    }
  }

  /* Set attributes */
  std::string child_path = remote_file.filename().generic_string();

  session.putAttribute(*flow_file, ATTRIBUTE_SFTP_REMOTE_HOST, common_properties.hostname);
  session.putAttribute(*flow_file, ATTRIBUTE_SFTP_REMOTE_PORT, std::to_string(common_properties.port));
  session.putAttribute(*flow_file, ATTRIBUTE_SFTP_REMOTE_FILENAME, remote_file.generic_string());
  flow_file->setAttribute(core::SpecialFlowAttribute::FILENAME, child_path);
  if (!remote_file.parent_path().empty()) {
    flow_file->setAttribute(core::SpecialFlowAttribute::PATH, (remote_file.parent_path() / "").generic_string());
  }

  /* Execute completion strategy */
  if (completion_strategy_ == COMPLETION_STRATEGY_DELETE_FILE) {
    if (!client->removeFile(remote_file.generic_string())) {
      logger_->log_warn("Completion Strategy is Delete File, but failed to delete remote file \"{}\"", remote_file.generic_string());
    }
  } else if (completion_strategy_ == COMPLETION_STRATEGY_MOVE_FILE) {
    bool should_move = true;
    if (create_directory_) {
      auto res = createDirectoryHierarchy(*client, move_destination_directory.generic_string(), disable_directory_listing_);
      if (res != SFTPProcessorBase::CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_OK) {
        should_move = false;
      }
    }
    if (!should_move) {
      logger_->log_warn("Completion Strategy is Move File, but failed to create Move Destination Directory \"{}\"", move_destination_directory.generic_string());
    } else {
      auto target_path = move_destination_directory / child_path;
      if (!client->rename(remote_file.generic_string(), target_path.generic_string(), false /*overwrite*/)) {
        logger_->log_warn(R"(Completion Strategy is Move File, but failed to move file "{}" to "{}")", remote_file.generic_string(), target_path.generic_string());
      }
    }
  }

  session.transfer(flow_file, Success);
  put_connection_back_to_cache();
}

REGISTER_RESOURCE(FetchSFTP, Processor);

}  // namespace org::apache::nifi::minifi::processors
