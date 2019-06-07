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

#include "PutSFTP.h"

#include <memory>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "utils/ByteArrayCallback.h"
#include "core/FlowFile.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "io/DataStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"
#include "utils/StringUtils.h"
#include "utils/ScopeGuard.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property PutSFTP::Hostname(
    core::PropertyBuilder::createProperty("Hostname")->withDescription("The fully qualified hostname or IP address of the remote system")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::Port(
    core::PropertyBuilder::createProperty("Port")->withDescription("The port that the remote system is listening on for file transfers")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::Username(
    core::PropertyBuilder::createProperty("Username")->withDescription("Username")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::Password(
    core::PropertyBuilder::createProperty("Password")->withDescription("Password for the user account")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::PrivateKeyPath(
    core::PropertyBuilder::createProperty("Private Key Path")->withDescription("The fully qualified path to the Private Key file")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::PrivateKeyPassphrase(
    core::PropertyBuilder::createProperty("Private Key Passphrase")->withDescription("Password for the private key")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::RemotePath(
    core::PropertyBuilder::createProperty("Remote Path")->withDescription("The path on the remote system from which to pull or push files")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::CreateDirectory(
    core::PropertyBuilder::createProperty("Create Directory")->withDescription("Specifies whether or not the remote directory should be created if it does not exist.")
        ->isRequired(true)->withDefaultValue<bool>(false)->build());
core::Property PutSFTP::DisableDirectoryListing(
    core::PropertyBuilder::createProperty("Disable Directory Listing")->withDescription("If set to 'true', directory listing is not performed prior to create missing directories. "
                                                                                        "By default, this processor executes a directory listing command to see target directory existence before creating missing directories. "
                                                                                        "However, there are situations that you might need to disable the directory listing such as the following. "
                                                                                        "Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. "
                                                                                        "Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, "
                                                                                        "then an error is returned because the directory already exists.")
        ->isRequired(false)->withDefaultValue<bool>(false)->build());
core::Property PutSFTP::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")->withDescription("The maximum number of FlowFiles to send in a single connection")
        ->isRequired(true)->withDefaultValue<uint64_t>(500)->build());
core::Property PutSFTP::ConnectionTimeout(
    core::PropertyBuilder::createProperty("Connection Timeout")->withDescription("Amount of time to wait before timing out while creating a connection")
        ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("30 sec")->build());
core::Property PutSFTP::DataTimeout(
    core::PropertyBuilder::createProperty("Data Timeout")->withDescription("When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems")
        ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("30 sec")->build());
core::Property PutSFTP::ConflictResolution(
    core::PropertyBuilder::createProperty("Conflict Resolution")->withDescription("Determines how to handle the problem of filename collisions")
        ->isRequired(true)
        ->withAllowableValues<std::string>({CONFLICT_RESOLUTION_REPLACE,
                                            CONFLICT_RESOLUTION_IGNORE,
                                            CONFLICT_RESOLUTION_RENAME,
                                            CONFLICT_RESOLUTION_REJECT,
                                            CONFLICT_RESOLUTION_FAIL,
                                            CONFLICT_RESOLUTION_NONE})
        ->withDefaultValue(CONFLICT_RESOLUTION_NONE)->build());
core::Property PutSFTP::RejectZeroByte(
    core::PropertyBuilder::createProperty("Reject Zero-Byte Files")->withDescription("Determines whether or not Zero-byte files should be rejected without attempting to transfer")
        ->isRequired(false)->withDefaultValue<bool>(true)->build());
core::Property PutSFTP::DotRename(
    core::PropertyBuilder::createProperty("Dot Rename")->withDescription("If true, then the filename of the sent file is prepended with a \".\" and then renamed back to the original once the file is completely sent. "
                                                                         "Otherwise, there is no rename. This property is ignored if the Temporary Filename property is set.")
        ->isRequired(false)->withDefaultValue<bool>(true)->build());
core::Property PutSFTP::TempFilename(
    core::PropertyBuilder::createProperty("Temporary Filename")->withDescription("If set, the filename of the sent file will be equal to the value specified during the transfer and after successful completion will be renamed to the original filename. "
                                                                                 "If this value is set, the Dot Rename property is ignored.")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::HostKeyFile(
    core::PropertyBuilder::createProperty("Host Key File")->withDescription("If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used")
        ->isRequired(false)->build());
core::Property PutSFTP::LastModifiedTime(
    core::PropertyBuilder::createProperty("Last Modified Time")->withDescription("The lastModifiedTime to assign to the file after transferring it. "
                                                                                  "If not set, the lastModifiedTime will not be changed. "
                                                                                  "Format must be yyyy-MM-dd'T'HH:mm:ssZ. "
                                                                                  "You may also use expression language such as ${file.lastModifiedTime}. "
                                                                                  "If the value is invalid, the processor will not be invalid but will fail to change lastModifiedTime of the file.")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::Permissions(
    core::PropertyBuilder::createProperty("Permissions")->withDescription("The permissions to assign to the file after transferring it. "
                                                                          "Format must be either UNIX rwxrwxrwx with a - in place of denied permissions (e.g. rw-r--r--) or an octal number (e.g. 644). "
                                                                          "If not set, the permissions will not be changed. "
                                                                          "You may also use expression language such as ${file.permissions}. "
                                                                          "If the value is invalid, the processor will not be invalid but will fail to change permissions of the file.")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::RemoteOwner(
    core::PropertyBuilder::createProperty("Remote Owner")->withDescription("Integer value representing the User ID to set on the file after transferring it. "
                                                                           "If not set, the owner will not be set. You may also use expression language such as ${file.owner}. "
                                                                           "If the value is invalid, the processor will not be invalid but will fail to change the owner of the file.")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::RemoteGroup(
    core::PropertyBuilder::createProperty("Remote Group")->withDescription("Integer value representing the Group ID to set on the file after transferring it. "
                                                                           "If not set, the group will not be set. You may also use expression language such as ${file.group}. "
                                                                           "If the value is invalid, the processor will not be invalid but will fail to change the group of the file.")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::StrictHostKeyChecking(
    core::PropertyBuilder::createProperty("Strict Host Key Checking")->withDescription("Indicates whether or not strict enforcement of hosts keys should be applied")
        ->isRequired(true)->withDefaultValue<bool>(false)->build());
core::Property PutSFTP::UseKeepaliveOnTimeout(
    core::PropertyBuilder::createProperty("Send Keep Alive On Timeout")->withDescription("Indicates whether or not to send a single Keep Alive message when SSH socket times out")
        ->isRequired(true)->withDefaultValue<bool>(true)->build());
core::Property PutSFTP::UseCompression(
    core::PropertyBuilder::createProperty("Use Compression")->withDescription("Indicates whether or not ZLIB compression should be used when transferring files")
        ->isRequired(true)->withDefaultValue<bool>(false)->build());
core::Property PutSFTP::ProxyType(
    core::PropertyBuilder::createProperty("Proxy Type")->withDescription("Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. "
                                                                         "Supported proxies: HTTP + AuthN, SOCKS + AuthN")
        ->isRequired(false)
        ->withAllowableValues<std::string>({PROXY_TYPE_DIRECT,
                                            PROXY_TYPE_HTTP,
                                            PROXY_TYPE_SOCKS})
        ->withDefaultValue(PROXY_TYPE_DIRECT)->build());
core::Property PutSFTP::ProxyHost(
    core::PropertyBuilder::createProperty("Proxy Host")->withDescription("The fully qualified hostname or IP address of the proxy server")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::ProxyPort(
    core::PropertyBuilder::createProperty("Proxy Port")->withDescription("The port of the proxy server")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::HttpProxyUsername(
    core::PropertyBuilder::createProperty("Http Proxy Username")->withDescription("Http Proxy Username")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property PutSFTP::HttpProxyPassword(
    core::PropertyBuilder::createProperty("Http Proxy Password")->withDescription("Http Proxy Password")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());

core::Relationship PutSFTP::Success("success", "FlowFiles that are successfully sent will be routed to success");
core::Relationship PutSFTP::Reject("reject", "FlowFiles that were rejected by the destination system");
core::Relationship PutSFTP::Failure("failure", "FlowFiles that failed to send to the remote system; failure is usually looped back to this processor");

void PutSFTP::initialize() {
  logger_->log_trace("Initializing PutSFTP");

  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(Hostname);
  properties.insert(Port);
  properties.insert(Username);
  properties.insert(Password);
  properties.insert(PrivateKeyPath);
  properties.insert(PrivateKeyPassphrase);
  properties.insert(RemotePath);
  properties.insert(CreateDirectory);
  properties.insert(DisableDirectoryListing);
  properties.insert(BatchSize);
  properties.insert(ConnectionTimeout);
  properties.insert(DataTimeout);
  properties.insert(ConflictResolution);
  properties.insert(RejectZeroByte);
  properties.insert(DotRename);
  properties.insert(TempFilename);
  properties.insert(HostKeyFile);
  properties.insert(LastModifiedTime);
  properties.insert(Permissions);
  properties.insert(RemoteOwner);
  properties.insert(RemoteGroup);
  properties.insert(StrictHostKeyChecking);
  properties.insert(UseKeepaliveOnTimeout);
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
  relationships.insert(Reject);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
}

PutSFTP::PutSFTP(std::string name, utils::Identifier uuid /*= utils::Identifier()*/)
  : SFTPProcessorBase(name, uuid),
    create_directory_(false),
    batch_size_(0),
    reject_zero_byte_(false),
    dot_rename_(false) {
  logger_ = logging::LoggerFactory<PutSFTP>::getLogger();
}

PutSFTP::~PutSFTP() {
}

void PutSFTP::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  std::string value;
  if (!context->getProperty(CreateDirectory.getName(), value)) {
    logger_->log_error("Create Directory attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, create_directory_);
  }
  if (!context->getProperty(BatchSize.getName(), value)) {
    logger_->log_error("Batch Size attribute is missing or invalid");
  } else {
    core::Property::StringToInt(value, batch_size_);
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
  context->getProperty(ConflictResolution.getName(), conflict_resolution_);
  if (context->getProperty(RejectZeroByte.getName(), value)) {
    utils::StringUtils::StringToBool(value, reject_zero_byte_);
  }
  if (context->getProperty(DotRename.getName(), value)) {
    utils::StringUtils::StringToBool(value, dot_rename_);
  }
  context->getProperty(HostKeyFile.getName(), host_key_file_);
  if (!context->getProperty(StrictHostKeyChecking.getName(), value)) {
    logger_->log_error("Strict Host Key Checking attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, strict_host_checking_);
  }
  if (!context->getProperty(UseKeepaliveOnTimeout.getName(), value)) {
    logger_->log_error("Send Keep Alive On Timeout attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, use_keepalive_on_timeout_);
  }
  if (!context->getProperty(UseCompression.getName(), value)) {
    logger_->log_error("Use Compression attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, use_compression_);
  }
  context->getProperty(ProxyType.getName(), proxy_type_);

  startKeepaliveThreadIfNeeded();
}

void PutSFTP::notifyStop() {
  logger_->log_debug("Got notifyStop, stopping keepalive thread and clearing connections");
  cleanupConnectionCache();
}

PutSFTP::ReadCallback::ReadCallback(const std::string& target_path,
                                    utils::SFTPClient& client,
                                    const std::string& conflict_resolution)
    : logger_(logging::LoggerFactory<PutSFTP::ReadCallback>::getLogger())
    , write_succeeded_(false)
    , target_path_(target_path)
    , client_(client)
    , conflict_resolution_(conflict_resolution) {
}

PutSFTP::ReadCallback::~ReadCallback() {
}

int64_t PutSFTP::ReadCallback::process(std::shared_ptr<io::BaseStream> stream) {
  if (!client_.putFile(target_path_,
      *stream,
      conflict_resolution_ == CONFLICT_RESOLUTION_REPLACE /*overwrite*/,
      stream->getSize() /*expected_size*/)) {
    throw client_.getLastError();
  }
  return stream->getSize();
}

bool PutSFTP::processOne(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<FlowFileRecord> flow_file = std::static_pointer_cast<FlowFileRecord>(session->get());
  if (flow_file == nullptr) {
    return false;
  }

  /* Parse possibly FlowFile-dependent properties */
  std::string filename;
  std::string hostname;
  uint16_t port = 0U;
  std::string username;
  std::string password;
  std::string private_key_path;
  std::string private_key_passphrase;
  std::string remote_path;
  bool disable_directory_listing = false;
  std::string temp_file_name;
  bool last_modified_time_set = false;
  int64_t last_modified_time = 0U;
  bool permissions_set = false;
  uint32_t permissions = 0U;
  bool remote_owner_set = false;
  uint64_t remote_owner = 0U;
  bool remote_group_set = false;
  uint64_t remote_group = 0U;
  std::string proxy_host;
  uint16_t proxy_port = 0U;
  std::string proxy_username;
  std::string proxy_password;

  flow_file->getKeyedAttribute(FILENAME, filename);

  std::string value;
  if (!context->getProperty(Hostname, hostname, flow_file)) {
    logger_->log_error("Hostname attribute is missing");
    context->yield();
    return false;
  }
  if (!context->getProperty(Port, value, flow_file)) {
    logger_->log_error("Port attribute is missing or invalid");
    context->yield();
    return false;
  } else {
    int port_tmp;
    if (!core::Property::StringToInt(value, port_tmp) ||
        port_tmp < std::numeric_limits<uint16_t>::min() ||
        port_tmp > std::numeric_limits<uint16_t>::max()) {
      logger_->log_error("Port attribute \"%s\" is invalid", value);
      context->yield();
      return false;
    } else {
      port = static_cast<uint16_t>(port_tmp);
    }
  }
  if (!context->getProperty(Username, username, flow_file)) {
    logger_->log_error("Username attribute is missing");
    context->yield();
    return false;
  }
  context->getProperty(Password, password, flow_file);
  context->getProperty(PrivateKeyPath, private_key_path, flow_file);
  context->getProperty(PrivateKeyPassphrase, private_key_passphrase, flow_file);
  context->getProperty(Password, password, flow_file);
  context->getProperty(RemotePath, remote_path, flow_file);
  if (context->getDynamicProperty(DisableDirectoryListing.getName(), value)) {
    utils::StringUtils::StringToBool(value, disable_directory_listing);
  } else if (context->getProperty(DisableDirectoryListing.getName(), value)) {
    utils::StringUtils::StringToBool(value, disable_directory_listing);
  }
  /* Remove trailing slashes */
  while (remote_path.size() > 1U && remote_path.back() == '/') {
    remote_path.resize(remote_path.size() - 1);
  }
  /* Empty path means current directory, so we change it to '.' */
  if (remote_path.empty()) {
    remote_path = ".";
  }
  context->getProperty(TempFilename, temp_file_name, flow_file);
  if (context->getProperty(LastModifiedTime, value, flow_file)) {
    if (core::Property::StringToDateTime(value, last_modified_time)) {
      last_modified_time_set = true;
    }
  }
  if (context->getProperty(Permissions, value, flow_file)) {
    if (core::Property::StringToPermissions(value, permissions)) {
      permissions_set = true;
    }
  }
  if (context->getProperty(RemoteOwner, value, flow_file)) {
    if (core::Property::StringToInt(value, remote_owner)) {
      remote_owner_set = true;
    }
  }
  if (context->getProperty(RemoteGroup, value, flow_file)) {
    if (core::Property::StringToInt(value, remote_group)) {
      remote_group_set = true;
    }
  }
  context->getProperty(ProxyHost, proxy_host, flow_file);
  if (context->getProperty(ProxyPort, value, flow_file) && !value.empty()) {
    int port_tmp;
    if (!core::Property::StringToInt(value, port_tmp) ||
        port_tmp < std::numeric_limits<uint16_t>::min() ||
        port_tmp > std::numeric_limits<uint16_t>::max()) {
      logger_->log_error("Proxy Port attribute \"%s\" is invalid", value);
      context->yield();
      return false;
    } else {
      proxy_port = static_cast<uint16_t>(port_tmp);
    }
  }
  context->getProperty(HttpProxyUsername, proxy_username, flow_file);
  context->getProperty(HttpProxyPassword, proxy_password, flow_file);

  /* Reject zero-byte files if needed */
  if (reject_zero_byte_ && flow_file->getSize() == 0U) {
    logger_->log_debug("Rejecting %s because it is zero bytes", filename);
    session->transfer(flow_file, Reject);
    return true;
  }

  /* Get SFTPClient from cache or create it */
  const SFTPProcessorBase::ConnectionCacheKey connection_cache_key = {hostname, port, username, proxy_type_, proxy_host, proxy_port, proxy_username};
  auto client = getOrCreateConnection(connection_cache_key,
                                      password,
                                      private_key_path,
                                      private_key_passphrase,
                                      proxy_password);
  if (client == nullptr) {
    context->yield();
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
  std::string resolved_filename = filename;
  if (conflict_resolution_ != CONFLICT_RESOLUTION_NONE) {
    std::stringstream target_path_ss;
    target_path_ss << remote_path << "/" << filename;
    auto target_path = target_path_ss.str();
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    if (!client->stat(target_path, true /*follow_symlinks*/, attrs)) {
      if (client->getLastError() != utils::SFTPError::SFTP_ERROR_FILE_NOT_EXISTS) {
        logger_->log_error("Failed to stat %s", target_path.c_str());
        session->transfer(flow_file, Failure);
        return true;
      }
    } else {
      if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) && LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
        logger_->log_error("Rejecting %s because a directory with the same name already exists", filename.c_str());
        session->transfer(flow_file, Reject);
        put_connection_back_to_cache();
        return true;
      }
      logger_->log_debug("Found file with the same name as the target file: %s", filename.c_str());
      if (conflict_resolution_ == CONFLICT_RESOLUTION_IGNORE) {
        logger_->log_debug("Routing %s to SUCCESS despite a file with the same name already existing", filename.c_str());
        session->transfer(flow_file, Success);
        put_connection_back_to_cache();
        return true;
      } else if (conflict_resolution_ == CONFLICT_RESOLUTION_REJECT) {
        logger_->log_debug("Routing %s to REJECT because a file with the same name already exists", filename.c_str());
        session->transfer(flow_file, Reject);
        put_connection_back_to_cache();
        return true;
      } else if (conflict_resolution_ == CONFLICT_RESOLUTION_FAIL) {
        logger_->log_debug("Routing %s to FAILURE because a file with the same name already exists", filename.c_str());
        session->transfer(flow_file, Failure);
        put_connection_back_to_cache();
        return true;
      } else if (conflict_resolution_ == CONFLICT_RESOLUTION_RENAME) {
        std::string possible_resolved_filename;
        bool unique_name_generated = false;
        for (int i = 1; i < 100; i++) {
          std::stringstream possible_resolved_filename_ss;
          possible_resolved_filename_ss << i << "." << filename;
          possible_resolved_filename = possible_resolved_filename_ss.str();
          auto possible_resolved_path = remote_path + "/" + possible_resolved_filename;
          if (!client->stat(possible_resolved_path, true /*follow_symlinks*/, attrs)) {
            if (client->getLastError() == utils::SFTPError::SFTP_ERROR_FILE_NOT_EXISTS) {
              unique_name_generated = true;
              break;
            } else {
              logger_->log_error("Failed to stat %s", possible_resolved_path.c_str());
              session->transfer(flow_file, Failure);
              return true;
            }
          }
        }
        if (unique_name_generated) {
          logger_->log_debug("Resolved %s to %s", filename.c_str(), possible_resolved_filename.c_str());
          resolved_filename = std::move(possible_resolved_filename);
        } else {
          logger_->log_error("Rejecting %s because a unique name could not be determined after 99 attempts", filename.c_str());
          session->transfer(flow_file, Reject);
          put_connection_back_to_cache();
          return true;
        }
      }
    }
  }

  /* Create remote directory if needed */
  if (create_directory_) {
    auto res = createDirectoryHierarchy(*client, remote_path, disable_directory_listing);
    switch (res) {
      case SFTPProcessorBase::CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_OK:
        break;
      case SFTPProcessorBase::CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_STAT_FAILED:
        context->yield();
        return false;
      case SFTPProcessorBase::CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_NOT_A_DIRECTORY:
        session->transfer(flow_file, Failure);
        put_connection_back_to_cache();
        return true;
      case SFTPProcessorBase::CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_NOT_FOUND:
      case SFTPProcessorBase::CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_PERMISSION_DENIED:
        session->transfer(flow_file, Failure);
        put_connection_back_to_cache();
        return true;
      default:
        logger_->log_error("Unknown createDirectoryHierarchy result: %hhu", static_cast<uint8_t>(res));
        context->yield();
        return false;
    }
  }

  /* Upload file */
  std::stringstream target_path_ss;
  target_path_ss << remote_path << "/";
  if (!IsNullOrEmpty(temp_file_name)) {
    target_path_ss << temp_file_name;
  } else if (dot_rename_) {
    target_path_ss << "." << resolved_filename;
  } else {
    target_path_ss << resolved_filename;
  }
  auto target_path = target_path_ss.str();
  std::stringstream final_target_path_ss;
  final_target_path_ss << remote_path << "/" << resolved_filename;
  auto final_target_path = final_target_path_ss.str();
  logger_->log_debug("The target path is %s, final target path is %s", target_path.c_str(), final_target_path.c_str());

  ReadCallback read_callback(target_path.c_str(), *client, conflict_resolution_);
  try {
    session->read(flow_file, &read_callback);
  } catch (const utils::SFTPError&) {
    session->transfer(flow_file, Failure);
    return true;
  }

  /* Move file to its final place */
  if (target_path != final_target_path) {
    if (!client->rename(target_path, final_target_path, conflict_resolution_ == CONFLICT_RESOLUTION_REPLACE /*overwrite*/)) {
      logger_->log_error("Failed to move temporary file %s to final path %s", target_path, final_target_path);
      if (!client->removeFile(target_path)) {
        logger_->log_error("Failed to remove temporary file %s", target_path.c_str());
      }
      session->transfer(flow_file, Failure);
      return true;
    }
  }

  /* Set file attributes if needed */
  if (last_modified_time_set ||
      permissions_set ||
      remote_owner_set ||
      remote_group_set) {
    utils::SFTPClient::SFTPAttributes attrs;
    attrs.flags = 0U;
    if (last_modified_time_set) {
      /*
       * NiFi doesn't set atime, only mtime, but because they can only be set together,
       * if we don't want to modify atime, we first have to get it.
       * Therefore setting them both saves an extra protocol round.
       */
      attrs.flags |= utils::SFTPClient::SFTP_ATTRIBUTE_MTIME | utils::SFTPClient::SFTP_ATTRIBUTE_ATIME;
      attrs.mtime = last_modified_time;
      attrs.atime = last_modified_time;
    }
    if (permissions_set) {
      attrs.flags |= utils::SFTPClient::SFTP_ATTRIBUTE_PERMISSIONS;
      attrs.permissions = permissions;
    }
    if (remote_owner_set) {
      attrs.flags |= utils::SFTPClient::SFTP_ATTRIBUTE_UID;
      attrs.uid = remote_owner;
    }
    if (remote_group_set) {
      attrs.flags |= utils::SFTPClient::SFTP_ATTRIBUTE_GID;
      attrs.gid = remote_group;
    }
    if (!client->setAttributes(final_target_path, attrs)) {
      /* This is not fatal, just log a warning */
      logger_->log_warn("Failed to set file attributes for %s", target_path);
    }
  }

  session->transfer(flow_file, Success);
  put_connection_back_to_cache();
  return true;
}

void PutSFTP::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  const uint64_t limit = batch_size_ > 0 ? batch_size_ : std::numeric_limits<uint64_t>::max();
  for (uint64_t i = 0; i < limit; i++) {
    if (!this->processOne(context, session)) {
      return;
    }
  }
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
