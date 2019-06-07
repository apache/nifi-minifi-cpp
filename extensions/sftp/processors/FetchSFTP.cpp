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
#include "io/DataStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property FetchSFTP::Hostname(
    core::PropertyBuilder::createProperty("Hostname")->withDescription("The fully qualified hostname or IP address of the remote system")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());
core::Property FetchSFTP::Port(
    core::PropertyBuilder::createProperty("Port")->withDescription("The port that the remote system is listening on for file transfers")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());
core::Property FetchSFTP::Username(
    core::PropertyBuilder::createProperty("Username")->withDescription("Username")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());
core::Property FetchSFTP::Password(
    core::PropertyBuilder::createProperty("Password")->withDescription("Password for the user account")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property FetchSFTP::PrivateKeyPath(
    core::PropertyBuilder::createProperty("Private Key Path")->withDescription("The fully qualified path to the Private Key file")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property FetchSFTP::PrivateKeyPassphrase(
    core::PropertyBuilder::createProperty("Private Key Passphrase")->withDescription("Password for the private key")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property FetchSFTP::RemoteFile(
    core::PropertyBuilder::createProperty("Remote File")->withDescription("The fully qualified filename on the remote system")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());
core::Property FetchSFTP::CompletionStrategy(
    core::PropertyBuilder::createProperty("Completion Strategy")->withDescription("Specifies what to do with the original file on the server once it has been pulled into NiFi. If the Completion Strategy fails, a warning will be logged but the data will still be transferred.")
        ->isRequired(true)
        ->withAllowableValues<std::string>({COMPLETION_STRATEGY_NONE,
                                            COMPLETION_STRATEGY_MOVE_FILE,
                                            COMPLETION_STRATEGY_DELETE_FILE})
        ->withDefaultValue(COMPLETION_STRATEGY_NONE)->build());
core::Property FetchSFTP::MoveDestinationDirectory(
    core::PropertyBuilder::createProperty("Move Destination Directory")->withDescription("The directory on the remote server to move the original file to once it has been ingested into NiFi. "
                                                                                         "This property is ignored unless the Completion Strategy is set to 'Move File'. "
                                                                                         "The specified directory must already exist on the remote system if 'Create Directory' is disabled, or the rename will fail.")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property FetchSFTP::CreateDirectory(
    core::PropertyBuilder::createProperty("Create Directory")->withDescription("Specifies whether or not the remote directory should be created if it does not exist.")
        ->isRequired(true)->withDefaultValue<bool>(false)->build());
core::Property FetchSFTP::DisableDirectoryListing(
    core::PropertyBuilder::createProperty("Disable Directory Listing")->withDescription("Control how 'Move Destination Directory' is created when 'Completion Strategy' is 'Move File' and 'Create Directory' is enabled. "
                                                                                        "If set to 'true', directory listing is not performed prior to create missing directories. "
                                                                                        "By default, this processor executes a directory listing command to see target directory existence before creating missing directories. "
                                                                                        "However, there are situations that you might need to disable the directory listing such as the following. "
                                                                                        "Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. "
                                                                                        "Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, "
                                                                                        "then an error is returned because the directory already exists.")
        ->isRequired(false)->withDefaultValue<bool>(false)->build());
core::Property FetchSFTP::ConnectionTimeout(
    core::PropertyBuilder::createProperty("Connection Timeout")->withDescription("Amount of time to wait before timing out while creating a connection")
        ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("30 sec")->build());
core::Property FetchSFTP::DataTimeout(
    core::PropertyBuilder::createProperty("Data Timeout")->withDescription("When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems")
        ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("30 sec")->build());
core::Property FetchSFTP::SendKeepaliveOnTimeout(
    core::PropertyBuilder::createProperty("Send Keep Alive On Timeout")->withDescription("Indicates whether or not to send a single Keep Alive message when SSH socket times out")
        ->isRequired(true)->withDefaultValue<bool>(true)->build());
core::Property FetchSFTP::HostKeyFile(
    core::PropertyBuilder::createProperty("Host Key File")->withDescription("If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used")
        ->isRequired(false)->build());
core::Property FetchSFTP::StrictHostKeyChecking(
    core::PropertyBuilder::createProperty("Strict Host Key Checking")->withDescription("Indicates whether or not strict enforcement of hosts keys should be applied")
        ->isRequired(true)->withDefaultValue<bool>(false)->build());
core::Property FetchSFTP::UseCompression(
    core::PropertyBuilder::createProperty("Use Compression")->withDescription("Indicates whether or not ZLIB compression should be used when transferring files")
        ->isRequired(true)->withDefaultValue<bool>(false)->build());
core::Property FetchSFTP::ProxyType(
    core::PropertyBuilder::createProperty("Proxy Type")->withDescription("Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. "
                                                                         "Supported proxies: HTTP + AuthN, SOCKS + AuthN")
        ->isRequired(false)
        ->withAllowableValues<std::string>({PROXY_TYPE_DIRECT,
                                            PROXY_TYPE_HTTP,
                                            PROXY_TYPE_SOCKS})
        ->withDefaultValue(PROXY_TYPE_DIRECT)->build());
core::Property FetchSFTP::ProxyHost(
    core::PropertyBuilder::createProperty("Proxy Host")->withDescription("The fully qualified hostname or IP address of the proxy server")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property FetchSFTP::ProxyPort(
    core::PropertyBuilder::createProperty("Proxy Port")->withDescription("The port of the proxy server")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property FetchSFTP::HttpProxyUsername(
    core::PropertyBuilder::createProperty("Http Proxy Username")->withDescription("Http Proxy Username")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property FetchSFTP::HttpProxyPassword(
    core::PropertyBuilder::createProperty("Http Proxy Password")->withDescription("Http Proxy Password")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());

core::Relationship FetchSFTP::Success("success", "All FlowFiles that are received are routed to success");
core::Relationship FetchSFTP::CommsFailure("comms.failure", "Any FlowFile that could not be fetched from the remote server due to a communications failure will be transferred to this Relationship.");
core::Relationship FetchSFTP::NotFound("not.found", "Any FlowFile for which we receive a 'Not Found' message from the remote server will be transferred to this Relationship.");
core::Relationship FetchSFTP::PermissionDenied("permission.denied", "Any FlowFile that could not be fetched from the remote server due to insufficient permissions will be transferred to this Relationship.");

void FetchSFTP::initialize() {
  logger_->log_trace("Initializing FetchSFTP");

  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(Hostname);
  properties.insert(Port);
  properties.insert(Username);
  properties.insert(Password);
  properties.insert(PrivateKeyPath);
  properties.insert(PrivateKeyPassphrase);
  properties.insert(RemoteFile);
  properties.insert(CompletionStrategy);
  properties.insert(MoveDestinationDirectory);
  properties.insert(CreateDirectory);
  properties.insert(DisableDirectoryListing);
  properties.insert(ConnectionTimeout);
  properties.insert(DataTimeout);
  properties.insert(SendKeepaliveOnTimeout);
  properties.insert(HostKeyFile);
  properties.insert(StrictHostKeyChecking);
  properties.insert(UseCompression);
  properties.insert(ProxyType);
  properties.insert(ProxyHost);
  properties.insert(ProxyPort);
  properties.insert(HttpProxyUsername);
  properties.insert(HttpProxyPassword);
  setSupportedProperties(properties);

  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(CommsFailure);
  relationships.insert(NotFound);
  relationships.insert(PermissionDenied);
  setSupportedRelationships(relationships);
}

FetchSFTP::FetchSFTP(std::string name, utils::Identifier uuid /*= utils::Identifier()*/)
    : SFTPProcessorBase(name, uuid),
      create_directory_(false),
      disable_directory_listing_(false) {
  logger_ = logging::LoggerFactory<FetchSFTP>::getLogger();
}

FetchSFTP::~FetchSFTP() {
}

void FetchSFTP::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  std::string value;
  context->getProperty(CompletionStrategy.getName(), completion_strategy_);
  if (!context->getProperty(CreateDirectory.getName(), value)) {
    logger_->log_error("Create Directory attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, create_directory_);
  }
  if (!context->getProperty(DisableDirectoryListing.getName(), value)) {
    logger_->log_error("Disable Directory Listing attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, disable_directory_listing_);
  }
  if (!context->getProperty(ConnectionTimeout.getName(), value)) {
    logger_->log_error("Connection Timeout attribute is missing or invalid");
  } else {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, connection_timeout_, unit) || !core::Property::ConvertTimeUnitToMS(connection_timeout_, unit, connection_timeout_)) {
      logger_->log_error("Connection Timeout attribute is invalid");
    }
  }
  if (!context->getProperty(DataTimeout.getName(), value)) {
    logger_->log_error("Data Timeout attribute is missing or invalid");
  } else {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, data_timeout_, unit) || !core::Property::ConvertTimeUnitToMS(data_timeout_, unit, data_timeout_)) {
      logger_->log_error("Data Timeout attribute is invalid");
    }
  }
  if (!context->getProperty(SendKeepaliveOnTimeout.getName(), value)) {
    logger_->log_error("Send Keep Alive On Timeout attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, use_keepalive_on_timeout_);
  }
  context->getProperty(HostKeyFile.getName(), host_key_file_);
  if (!context->getProperty(StrictHostKeyChecking.getName(), value)) {
    logger_->log_error("Strict Host Key Checking attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, strict_host_checking_);
  }
  if (!context->getProperty(UseCompression.getName(), value)) {
    logger_->log_error("Use Compression attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, use_compression_);
  }
  context->getProperty(ProxyType.getName(), proxy_type_);

  startKeepaliveThreadIfNeeded();
}

void FetchSFTP::notifyStop() {
  logger_->log_debug("Got notifyStop, stopping keepalive thread and clearing connections");
  cleanupConnectionCache();
}

FetchSFTP::WriteCallback::WriteCallback(const std::string& remote_file,
                                    utils::SFTPClient& client)
    : logger_(logging::LoggerFactory<FetchSFTP::WriteCallback>::getLogger())
    , remote_file_(remote_file)
    , client_(client) {
}

FetchSFTP::WriteCallback::~WriteCallback() {
}

int64_t FetchSFTP::WriteCallback::process(std::shared_ptr<io::BaseStream> stream) {
  if (!client_.getFile(remote_file_, *stream)) {
    throw client_.getLastError();
  }
  return stream->getSize();
}

void FetchSFTP::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<FlowFileRecord> flow_file = std::static_pointer_cast<FlowFileRecord>(session->get());
  if (flow_file == nullptr) {
    return;
  }

  /* Parse possibly FlowFile-dependent properties */
  std::string hostname;
  uint16_t port = 0U;
  std::string username;
  std::string password;
  std::string private_key_path;
  std::string private_key_passphrase;
  std::string remote_file;
  std::string move_destination_directory;
  std::string proxy_host;
  uint16_t proxy_port = 0U;
  std::string proxy_username;
  std::string proxy_password;

  std::string value;
  if (!context->getProperty(Hostname, hostname, flow_file)) {
    logger_->log_error("Hostname attribute is missing");
    context->yield();
    return;
  }
  if (!context->getProperty(Port, value, flow_file)) {
    logger_->log_error("Port attribute is missing or invalid");
    context->yield();
    return;
  } else {
    int port_tmp;
    if (!core::Property::StringToInt(value, port_tmp) ||
        port_tmp < std::numeric_limits<uint16_t>::min() ||
        port_tmp > std::numeric_limits<uint16_t>::max()) {
      logger_->log_error("Port attribute \"%s\" is invalid", value);
      context->yield();
      return;
    } else {
      port = static_cast<uint16_t>(port_tmp);
    }
  }
  if (!context->getProperty(Username, username, flow_file)) {
    logger_->log_error("Username attribute is missing");
    context->yield();
    return;
  }
  context->getProperty(Password, password, flow_file);
  context->getProperty(PrivateKeyPath, private_key_path, flow_file);
  context->getProperty(PrivateKeyPassphrase, private_key_passphrase, flow_file);
  context->getProperty(Password, password, flow_file);
  context->getProperty(RemoteFile, remote_file, flow_file);
  context->getProperty(MoveDestinationDirectory, move_destination_directory, flow_file);
  context->getProperty(ProxyHost, proxy_host, flow_file);
  if (context->getProperty(ProxyPort, value, flow_file) && !value.empty()) {
    int port_tmp;
    if (!core::Property::StringToInt(value, port_tmp) ||
        port_tmp < std::numeric_limits<uint16_t>::min() ||
        port_tmp > std::numeric_limits<uint16_t>::max()) {
      logger_->log_error("Proxy Port attribute \"%s\" is invalid", value);
      context->yield();
      return;
    } else {
      proxy_port = static_cast<uint16_t>(port_tmp);
    }
  }
  context->getProperty(HttpProxyUsername, proxy_username, flow_file);
  context->getProperty(HttpProxyPassword, proxy_password, flow_file);

  /* Get SFTPClient from cache or create it */
  const SFTPProcessorBase::ConnectionCacheKey connection_cache_key = {hostname, port, username, proxy_type_, proxy_host, proxy_port, proxy_username};
  auto client = getOrCreateConnection(connection_cache_key,
                                      password,
                                      private_key_path,
                                      private_key_passphrase,
                                      proxy_password);
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
  } catch (const utils::SFTPError& error) {
    switch (error) {
      case utils::SFTPError::SFTP_ERROR_PERMISSION_DENIED:
        session->transfer(flow_file, PermissionDenied);
        put_connection_back_to_cache();
        return;
      case utils::SFTPError::SFTP_ERROR_FILE_NOT_EXISTS:
        session->transfer(flow_file, NotFound);
        put_connection_back_to_cache();
        return;
      case utils::SFTPError::SFTP_ERROR_COMMUNICATIONS_FAILURE:
      case utils::SFTPError::SFTP_ERROR_IO_ERROR:
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

  session->putAttribute(flow_file, ATTRIBUTE_SFTP_REMOTE_HOST, hostname);
  session->putAttribute(flow_file, ATTRIBUTE_SFTP_REMOTE_PORT, std::to_string(port));
  session->putAttribute(flow_file, ATTRIBUTE_SFTP_REMOTE_FILENAME, remote_file);
  flow_file->updateKeyedAttribute(FILENAME, child_path);
  if (!parent_path.empty()) {
    flow_file->updateKeyedAttribute(PATH, parent_path);
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

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
