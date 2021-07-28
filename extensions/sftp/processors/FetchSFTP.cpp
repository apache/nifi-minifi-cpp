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

#include "FetchSFTP.h"

#include <memory>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <utility>

#include "utils/ByteArrayCallback.h"
#include "core/FlowFile.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "core/Resource.h"
#include "io/BufferStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property FetchSFTP::RemoteFile(core::PropertyBuilder::createProperty("Remote File")
    ->withDescription("The fully qualified filename on the remote system")
    ->isRequired(true)->supportsExpressionLanguage(true)->build());

core::Property FetchSFTP::CompletionStrategy(
    core::PropertyBuilder::createProperty("Completion Strategy")
    ->withDescription("Specifies what to do with the original file on the server once it has been pulled into NiFi. "
                      "If the Completion Strategy fails, a warning will be logged but the data will still be transferred.")
    ->isRequired(true)
    ->withAllowableValues<std::string>({COMPLETION_STRATEGY_NONE, COMPLETION_STRATEGY_MOVE_FILE, COMPLETION_STRATEGY_DELETE_FILE})
    ->withDefaultValue(COMPLETION_STRATEGY_NONE)->build());

core::Property FetchSFTP::MoveDestinationDirectory(core::PropertyBuilder::createProperty("Move Destination Directory")
    ->withDescription("The directory on the remote server to move the original file to once it has been ingested into NiFi. "
                      "This property is ignored unless the Completion Strategy is set to 'Move File'. "
                      "The specified directory must already exist on the remote system if 'Create Directory' is disabled, or the rename will fail.")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

core::Property FetchSFTP::CreateDirectory(core::PropertyBuilder::createProperty("Create Directory")
    ->withDescription("Specifies whether or not the remote directory should be created if it does not exist.")
    ->isRequired(true)->withDefaultValue<bool>(false)->build());

core::Property FetchSFTP::DisableDirectoryListing(core::PropertyBuilder::createProperty("Disable Directory Listing")
    ->withDescription("Control how 'Move Destination Directory' is created when 'Completion Strategy' is 'Move File' and 'Create Directory' is enabled. "
                      "If set to 'true', directory listing is not performed prior to create missing directories. "
                      "By default, this processor executes a directory listing command to see target directory existence before creating missing directories. "
                      "However, there are situations that you might need to disable the directory listing such as the following. "
                      "Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. "
                      "Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, "
                      "then an error is returned because the directory already exists.")
    ->isRequired(false)->withDefaultValue<bool>(false)->build());

core::Property FetchSFTP::UseCompression(core::PropertyBuilder::createProperty("Use Compression")
    ->withDescription("Indicates whether or not ZLIB compression should be used when transferring files")
    ->isRequired(true)->withDefaultValue<bool>(false)->build());

core::Relationship FetchSFTP::Success("success",
                                      "All FlowFiles that are received are routed to success");
core::Relationship FetchSFTP::CommsFailure("comms.failure",
                                           "Any FlowFile that could not be fetched from the remote server due to a communications failure will be transferred to this Relationship.");
core::Relationship FetchSFTP::NotFound("not.found",
                                       "Any FlowFile for which we receive a 'Not Found' message from the remote server will be transferred to this Relationship.");
core::Relationship FetchSFTP::PermissionDenied("permission.denied",
                                               "Any FlowFile that could not be fetched from the remote server due to insufficient permissions will be transferred to this Relationship.");

constexpr char const* FetchSFTP::COMPLETION_STRATEGY_NONE;
constexpr char const* FetchSFTP::COMPLETION_STRATEGY_MOVE_FILE;
constexpr char const* FetchSFTP::COMPLETION_STRATEGY_DELETE_FILE;

constexpr char const* FetchSFTP::ATTRIBUTE_SFTP_REMOTE_HOST;
constexpr char const* FetchSFTP::ATTRIBUTE_SFTP_REMOTE_PORT;
constexpr char const* FetchSFTP::ATTRIBUTE_SFTP_REMOTE_FILENAME;

constexpr char const* FetchSFTP::ProcessorName;

void FetchSFTP::initialize() {
  logger_->log_trace("Initializing FetchSFTP");

  // Set the supported properties
  std::set<core::Property> properties;
  addSupportedCommonProperties(properties);
  properties.insert(RemoteFile);
  properties.insert(CompletionStrategy);
  properties.insert(MoveDestinationDirectory);
  properties.insert(CreateDirectory);
  properties.insert(DisableDirectoryListing);
  properties.insert(UseCompression);
  setSupportedProperties(properties);

  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(CommsFailure);
  relationships.insert(NotFound);
  relationships.insert(PermissionDenied);
  setSupportedRelationships(relationships);
}

FetchSFTP::FetchSFTP(const std::string& name, const utils::Identifier& uuid /*= utils::Identifier()*/)
    : SFTPProcessorBase(name, uuid),
      create_directory_(false),
      disable_directory_listing_(false) {
  logger_ = logging::LoggerFactory<FetchSFTP>::getLogger();
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

FetchSFTP::WriteCallback::WriteCallback(const std::string& remote_file,
                                    utils::SFTPClient& client)
    : logger_(logging::LoggerFactory<FetchSFTP::WriteCallback>::getLogger())
    , remote_file_(remote_file)
    , client_(client) {
}

FetchSFTP::WriteCallback::~WriteCallback() = default;

int64_t FetchSFTP::WriteCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  if (!client_.getFile(remote_file_, *stream)) {
    throw utils::SFTPException{client_.getLastError()};
  }
  return stream->size();
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
  WriteCallback write_callback(remote_file, *client);
  try {
    session->write(flow_file, &write_callback);
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
  std::tie(parent_path, child_path) = utils::file::FileUtils::split_path(remote_file, true /*force_posix*/);

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
      auto target_path = utils::file::FileUtils::concat_path(move_destination_directory, child_path);
      if (!client->rename(remote_file, target_path, false /*overwrite*/)) {
        logger_->log_warn("Completion Strategy is Move File, but failed to move file \"%s\" to \"%s\"", remote_file, target_path);
      }
    }
  }

  session->transfer(flow_file, Success);
  put_connection_back_to_cache();
}

REGISTER_RESOURCE(FetchSFTP, "Fetches the content of a file from a remote SFTP server and overwrites the contents of an incoming FlowFile with the content of the remote file.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
