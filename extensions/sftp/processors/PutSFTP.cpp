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

#include "PutSFTP.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>

#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/Resource.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "io/BufferStream.h"
#include "utils/ConfigurationUtils.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/ProcessorConfigUtils.h"
#include "io/validation.h"

namespace org::apache::nifi::minifi::processors {

void PutSFTP::initialize() {
  logger_->log_trace("Initializing PutSFTP");

  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

PutSFTP::PutSFTP(core::ProcessorMetadata metadata)
  : SFTPProcessorBase(std::move(metadata)),
    create_directory_(false),
    batch_size_(0),
    reject_zero_byte_(false),
    dot_rename_(false) {
}

PutSFTP::~PutSFTP() = default;

void PutSFTP::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  parseCommonPropertiesOnSchedule(context);

  create_directory_ = utils::parseBoolProperty(context, CreateDirectory);
  batch_size_ = utils::parseU64Property(context, BatchSize);
  conflict_resolution_ = utils::parseProperty(context, ConflictResolution);
  reject_zero_byte_ = utils::parseBoolProperty(context, RejectZeroByte);
  dot_rename_ = utils::parseBoolProperty(context, DotRename);
  use_compression_ = utils::parseBoolProperty(context, UseCompression);

  startKeepaliveThreadIfNeeded();
}

bool PutSFTP::processOne(core::ProcessContext& context, core::ProcessSession& session) {
  auto flow_file = session.get();
  if (flow_file == nullptr) {
    return false;
  }

  /* Parse common properties */
  SFTPProcessorBase::CommonProperties common_properties;
  if (!parseCommonPropertiesOnTrigger(context, flow_file.get(), common_properties)) {
    context.yield();
    return false;
  }

  /* Parse processor-specific properties */
  std::filesystem::path filename;
  std::filesystem::path remote_path;

  if (auto file_name_str = flow_file->getAttribute(core::SpecialFlowAttribute::FILENAME))
    filename = *file_name_str;

  if (auto remote_path_str = context.getProperty(RemotePath, flow_file.get())) {
    remote_path = std::filesystem::path(*remote_path_str, std::filesystem::path::format::generic_format).lexically_normal();
    while (remote_path.filename().empty() && !remote_path.empty())
      remote_path = remote_path.parent_path();
    if (remote_path.empty())
      remote_path = ".";
  }

  bool disable_directory_listing = context.getDynamicProperty(std::string{DisableDirectoryListing.name})
      | utils::andThen(parsing::parseBool)
      | utils::valueOrElse([&]() -> bool { return utils::parseBoolProperty(context, DisableDirectoryListing);});

  std::string temp_file_name = context.getProperty(TempFilename, flow_file.get()).value_or("");
  auto last_modified_ = utils::parseOptionalProperty(context, LastModifiedTime, flow_file.get())
      | utils::andThen(utils::timeutils::parseDateTimeStr);

  std::optional<uint32_t> permissions = context.getProperty(Permissions, flow_file.get()) | utils::andThen(parsing::parseUnixOctalPermissions) | utils::toOptional();
  std::optional<uint64_t> remote_owner = utils::parseOptionalU64Property(context, RemoteOwner, flow_file.get());
  std::optional<uint64_t> remote_group = utils::parseOptionalU64Property(context, RemoteGroup, flow_file.get());

  /* Reject zero-byte files if needed */
  if (reject_zero_byte_ && flow_file->getSize() == 0U) {
    logger_->log_debug("Rejecting {} because it is zero bytes", filename.generic_string());
    session.transfer(flow_file, Reject);
    return true;
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
    return false;
  }

  /*
   * Unless we're sure that the connection is good, we don't want to put it back to the cache.
   * So we will only call this when we're sure that the connection is OK.
   */
  auto put_connection_back_to_cache = [this, &connection_cache_key, &client]() {
    addConnectionToCache(connection_cache_key, std::move(client));
  };

  /* Try to detect conflicts if needed */
  std::string resolved_filename = filename.generic_string();
  if (conflict_resolution_ != CONFLICT_RESOLUTION_NONE) {
    auto target_path = (remote_path / filename).generic_string();
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    if (!client->stat(target_path, true /*follow_symlinks*/, attrs)) {
      if (client->getLastError() != utils::SFTPError::FileDoesNotExist) {
        logger_->log_error("Failed to stat {}", target_path.c_str());
        session.transfer(flow_file, Failure);
        return true;
      }
    } else {
      if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) && LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
        logger_->log_error("Rejecting {} because a directory with the same name already exists", filename.string());
        session.transfer(flow_file, Reject);
        put_connection_back_to_cache();
        return true;
      }
      logger_->log_debug("Found file with the same name as the target file: {}", filename.string());
      if (conflict_resolution_ == CONFLICT_RESOLUTION_IGNORE) {
        logger_->log_debug("Routing {} to SUCCESS despite a file with the same name already existing", filename.string());
        session.transfer(flow_file, Success);
        put_connection_back_to_cache();
        return true;
      } else if (conflict_resolution_ == CONFLICT_RESOLUTION_REJECT) {
        logger_->log_debug("Routing {} to REJECT because a file with the same name already exists", filename.string());
        session.transfer(flow_file, Reject);
        put_connection_back_to_cache();
        return true;
      } else if (conflict_resolution_ == CONFLICT_RESOLUTION_FAIL) {
        logger_->log_debug("Routing {} to FAILURE because a file with the same name already exists", filename.string());
        session.transfer(flow_file, Failure);
        put_connection_back_to_cache();
        return true;
      } else if (conflict_resolution_ == CONFLICT_RESOLUTION_RENAME) {
        std::string possible_resolved_filename;
        bool unique_name_generated = false;
        for (int i = 1; i < 100; i++) {
          possible_resolved_filename = std::to_string(i) + "." + filename.generic_string();
          auto possible_resolved_path = (remote_path / possible_resolved_filename).generic_string();
          if (!client->stat(possible_resolved_path, true /*follow_symlinks*/, attrs)) {
            if (client->getLastError() == utils::SFTPError::FileDoesNotExist) {
              unique_name_generated = true;
              break;
            } else {
              logger_->log_error("Failed to stat {}", possible_resolved_path.c_str());
              session.transfer(flow_file, Failure);
              return true;
            }
          }
        }
        if (unique_name_generated) {
          logger_->log_debug("Resolved {} to {}", filename.generic_string(), possible_resolved_filename);
          resolved_filename = std::move(possible_resolved_filename);
        } else {
          logger_->log_error("Rejecting {} because a unique name could not be determined after 99 attempts", filename.string());
          session.transfer(flow_file, Reject);
          put_connection_back_to_cache();
          return true;
        }
      }
    }
  }

  /* Create remote directory if needed */
  if (create_directory_) {
    auto res = createDirectoryHierarchy(*client, remote_path.generic_string(), disable_directory_listing);
    switch (res) {
      case SFTPProcessorBase::CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_OK:
        break;
      case SFTPProcessorBase::CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_STAT_FAILED:
        context.yield();
        return false;
      case SFTPProcessorBase::CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_NOT_A_DIRECTORY:
      case SFTPProcessorBase::CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_NOT_FOUND:
      case SFTPProcessorBase::CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_PERMISSION_DENIED:
        session.transfer(flow_file, Failure);
        put_connection_back_to_cache();
        return true;
      default:
        logger_->log_error("Unknown createDirectoryHierarchy result: {}", magic_enum::enum_underlying(res));
        context.yield();
        return false;
    }
  }

  /* Upload file */
  auto target_path = remote_path;
  if (!IsNullOrEmpty(temp_file_name)) {
    target_path /= temp_file_name;
  } else if (dot_rename_) {
    target_path /= "." + resolved_filename;
  } else {
    target_path /= resolved_filename;
  }

  std::string final_target_path = (remote_path / resolved_filename).generic_string();
  logger_->log_debug("The target path is {}, final target path is {}", target_path.string(), final_target_path);

  try {
    session.read(flow_file, [&client, &target_path, this](const std::shared_ptr<io::InputStream>& stream) {
      if (!client->putFile(target_path.generic_string(),
          *stream,
          conflict_resolution_ == CONFLICT_RESOLUTION_REPLACE /*overwrite*/,
          gsl::narrow<int64_t>(stream->size()) /*expected_size*/)) {
        throw utils::SFTPException{client->getLastError()};
      }
      return gsl::narrow<int64_t>(stream->size());
    });
  } catch (const utils::SFTPException& ex) {
    logger_->log_debug("{}", ex.what());
    session.transfer(flow_file, Failure);
    return true;
  }

  /* Move file to its final place */
  if (target_path != final_target_path) {
    if (!client->rename(target_path.generic_string(), final_target_path, conflict_resolution_ == CONFLICT_RESOLUTION_REPLACE /*overwrite*/)) {
      logger_->log_error("Failed to move temporary file {} to final path {}", target_path.generic_string(), final_target_path);
      if (!client->removeFile(target_path.generic_string())) {
        logger_->log_error("Failed to remove temporary file {}", target_path.generic_string());
      }
      session.transfer(flow_file, Failure);
      return true;
    }
  }

  /* Set file attributes if needed */
  if (last_modified_ ||
      permissions ||
      remote_owner ||
      remote_group) {
    utils::SFTPClient::SFTPAttributes attrs{};
    attrs.flags = 0U;
    if (last_modified_) {
      /*
       * NiFi doesn't set atime, only mtime, but because they can only be set together,
       * if we don't want to modify atime, we first have to get it.
       * Therefore setting them both saves an extra protocol round.
       */
      attrs.flags |= utils::SFTPClient::SFTP_ATTRIBUTE_MTIME | utils::SFTPClient::SFTP_ATTRIBUTE_ATIME;
      attrs.mtime = std::chrono::duration_cast<std::chrono::seconds>(last_modified_->time_since_epoch()).count();
      attrs.atime = std::chrono::duration_cast<std::chrono::seconds>(last_modified_->time_since_epoch()).count();
    }
    if (permissions) {
      attrs.flags |= utils::SFTPClient::SFTP_ATTRIBUTE_PERMISSIONS;
      attrs.permissions = *permissions;
    }
    if (remote_owner) {
      attrs.flags |= utils::SFTPClient::SFTP_ATTRIBUTE_UID;
      attrs.uid = *remote_owner;
    }
    if (remote_group) {
      attrs.flags |= utils::SFTPClient::SFTP_ATTRIBUTE_GID;
      attrs.gid = *remote_group;
    }
    if (!client->setAttributes(final_target_path, attrs)) {
      /* This is not fatal, just log a warning */
      logger_->log_warn("Failed to set file attributes for {}", target_path.generic_string());
    }
  }

  session.transfer(flow_file, Success);
  put_connection_back_to_cache();
  return true;
}

void PutSFTP::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  const uint64_t limit = batch_size_ > 0 ? batch_size_ : std::numeric_limits<uint64_t>::max();
  for (uint64_t i = 0; i < limit; i++) {
    if (!this->processOne(context, session)) {
      return;
    }
  }
}

REGISTER_RESOURCE(PutSFTP, Processor);

}  // namespace org::apache::nifi::minifi::processors
