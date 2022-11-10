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

#include <memory>
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "core/FlowFile.h"
#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "core/Resource.h"
#include "io/BufferStream.h"
#include "io/StreamFactory.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::processors {

void FetchSFTP::initialize() {
  logger_->log_trace("Initializing FetchSFTP");

  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

FetchSFTP::FetchSFTP(std::string name, const utils::Identifier& uuid /*= utils::Identifier()*/)
    : SFTPProcessorBase(std::move(name), uuid),
      create_directory_(false),
      disable_directory_listing_(false) {
  logger_ = core::logging::LoggerFactory<FetchSFTP>::getLogger();
}

FetchSFTP::~FetchSFTP() = default;

void FetchSFTP::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  parseCommonPropertiesOnSchedule(context);

  std::string value;
  context->getProperty(CompletionStrategy.getName(), completion_strategy_);
  if (!context->getProperty(CreateDirectory.getName(), value)) {
    logger_->log_error("Create Directory attribute is missing or invalid");
  } else {
    create_directory_ = utils::StringUtils::toBool(value).value_or(false);
  }
  if (!context->getProperty(DisableDirectoryListing.getName(), value)) {
    logger_->log_error("Disable Directory Listing attribute is missing or invalid");
  } else {
    disable_directory_listing_ = utils::StringUtils::toBool(value).value_or(false);
  }
  if (!context->getProperty(UseCompression.getName(), value)) {
    logger_->log_error("Use Compression attribute is missing or invalid");
  } else {
    use_compression_ = utils::StringUtils::toBool(value).value_or(false);
  }

  startKeepaliveThreadIfNeeded();
}

void FetchSFTP::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  auto flow_file = session->get();
  if (flow_file == nullptr) {
    return;
  }

  /* Parse common properties */
  SFTPProcessorBase::CommonProperties common_properties;
  if (!parseCommonPropertiesOnTrigger(context, flow_file, common_properties)) {
    context->yield();
    return;
  }

  /* Parse processor-specific properties */
  std::string remote_file;
  std::string move_destination_directory;

  context->getProperty(RemoteFile, remote_file, flow_file);
  context->getProperty(MoveDestinationDirectory, move_destination_directory, flow_file);

  /* Get SFTPClient from cache or create it */
  const SFTPProcessorBase::ConnectionCacheKey connection_cache_key = {common_properties.hostname,
                                                                      common_properties.port,
                                                                      common_properties.username,
                                                                      proxy_type_,
                                                                      common_properties.proxy_host,
                                                                      common_properties.proxy_port,
                                                                      common_properties.proxy_username};
  auto client = getOrCreateConnection(connection_cache_key,
                                      common_properties.password,
                                      common_properties.private_key_path,
                                      common_properties.private_key_passphrase,
                                      common_properties.proxy_password);
  if (client == nullptr) {
    context->yield();
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
    session->write(flow_file, [&remote_file, &client](const std::shared_ptr<io::OutputStream>& stream) -> int64_t {
      auto bytes_read = client->getFile(remote_file, *stream);
      if (!bytes_read) {
        throw utils::SFTPException{client->getLastError()};
      }
      return gsl::narrow<int64_t>(*bytes_read);
    });
  } catch (const utils::SFTPException& ex) {
    logger_->log_debug(ex.what());
    switch (ex.error().value()) {
      case utils::SFTPError::PermissionDenied:
        session->transfer(flow_file, PermissionDenied);
        put_connection_back_to_cache();
        return;
      case utils::SFTPError::FileDoesNotExist:
        session->transfer(flow_file, NotFound);
        put_connection_back_to_cache();
        return;
      case utils::SFTPError::CommunicationFailure:
      case utils::SFTPError::IoError:
        session->transfer(flow_file, CommsFailure);
        return;
      default:
        session->transfer(flow_file, PermissionDenied);
        return;
    }
  }

  /* Set attributes */
  std::string parent_path;
  std::string child_path;
  std::tie(parent_path, child_path) = utils::file::split_path(remote_file, true /*force_posix*/);

  session->putAttribute(flow_file, ATTRIBUTE_SFTP_REMOTE_HOST, common_properties.hostname);
  session->putAttribute(flow_file, ATTRIBUTE_SFTP_REMOTE_PORT, std::to_string(common_properties.port));
  session->putAttribute(flow_file, ATTRIBUTE_SFTP_REMOTE_FILENAME, remote_file);
  flow_file->setAttribute(core::SpecialFlowAttribute::FILENAME, child_path);
  if (!parent_path.empty()) {
    flow_file->setAttribute(core::SpecialFlowAttribute::PATH, parent_path);
  }

  /* Execute completion strategy */
  if (completion_strategy_ == COMPLETION_STRATEGY_DELETE_FILE) {
    if (!client->removeFile(remote_file)) {
      logger_->log_warn("Completion Strategy is Delete File, but failed to delete remote file \"%s\"", remote_file);
    }
  } else if (completion_strategy_ == COMPLETION_STRATEGY_MOVE_FILE) {
    bool should_move = true;
    if (create_directory_) {
      auto res = createDirectoryHierarchy(*client, move_destination_directory, disable_directory_listing_);
      if (res != SFTPProcessorBase::CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_OK) {
        should_move = false;
      }
    }
    if (!should_move) {
      logger_->log_warn("Completion Strategy is Move File, but failed to create Move Destination Directory \"%s\"", move_destination_directory);
    } else {
      auto target_path = utils::file::concat_path(move_destination_directory, child_path);
      if (!client->rename(remote_file, target_path, false /*overwrite*/)) {
        logger_->log_warn(R"(Completion Strategy is Move File, but failed to move file "%s" to "%s")", remote_file, target_path);
      }
    }
  }

  session->transfer(flow_file, Success);
  put_connection_back_to_cache();
}

}  // namespace org::apache::nifi::minifi::processors
