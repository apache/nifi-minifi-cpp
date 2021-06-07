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

#include "SFTPProcessorBase.h"

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
#include "io/BufferStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property SFTPProcessorBase::Hostname(core::PropertyBuilder::createProperty("Hostname")
    ->withDescription("The fully qualified hostname or IP address of the remote system")
    ->isRequired(true)->supportsExpressionLanguage(true)->build());

core::Property SFTPProcessorBase::Port(core::PropertyBuilder::createProperty("Port")
    ->withDescription("The port that the remote system is listening on for file transfers")
    ->isRequired(true)->supportsExpressionLanguage(true)->build());

core::Property SFTPProcessorBase::Username(core::PropertyBuilder::createProperty("Username")
    ->withDescription("Username")
    ->isRequired(true)->supportsExpressionLanguage(true)->build());

core::Property SFTPProcessorBase::Password(core::PropertyBuilder::createProperty("Password")
    ->withDescription("Password for the user account")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

core::Property SFTPProcessorBase::PrivateKeyPath(core::PropertyBuilder::createProperty("Private Key Path")
    ->withDescription("The fully qualified path to the Private Key file")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

core::Property SFTPProcessorBase::PrivateKeyPassphrase(core::PropertyBuilder::createProperty("Private Key Passphrase")
    ->withDescription("Password for the private key")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

core::Property SFTPProcessorBase::StrictHostKeyChecking(core::PropertyBuilder::createProperty("Strict Host Key Checking")
    ->withDescription("Indicates whether or not strict enforcement of hosts keys should be applied")
    ->isRequired(true)->withDefaultValue<bool>(false)->build());

core::Property SFTPProcessorBase::HostKeyFile(core::PropertyBuilder::createProperty("Host Key File")
    ->withDescription("If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used")
    ->isRequired(false)->build());

core::Property SFTPProcessorBase::ConnectionTimeout(core::PropertyBuilder::createProperty("Connection Timeout")
    ->withDescription("Amount of time to wait before timing out while creating a connection")
    ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("30 sec")->build());

core::Property SFTPProcessorBase::DataTimeout(core::PropertyBuilder::createProperty("Data Timeout")
    ->withDescription("When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems")
    ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("30 sec")->build());

core::Property SFTPProcessorBase::SendKeepaliveOnTimeout(core::PropertyBuilder::createProperty("Send Keep Alive On Timeout")
    ->withDescription("Indicates whether or not to send a single Keep Alive message when SSH socket times out")
    ->isRequired(true)->withDefaultValue<bool>(true)->build());

core::Property SFTPProcessorBase::ProxyType(core::PropertyBuilder::createProperty("Proxy Type")
    ->withDescription("Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. "
                       "Supported proxies: HTTP + AuthN, SOCKS + AuthN")
    ->isRequired(false)
    ->withAllowableValues<std::string>({PROXY_TYPE_DIRECT,
                                        PROXY_TYPE_HTTP,
                                        PROXY_TYPE_SOCKS})
    ->withDefaultValue(PROXY_TYPE_DIRECT)->build());

core::Property SFTPProcessorBase::ProxyHost(core::PropertyBuilder::createProperty("Proxy Host")
    ->withDescription("The fully qualified hostname or IP address of the proxy server")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

core::Property SFTPProcessorBase::ProxyPort(core::PropertyBuilder::createProperty("Proxy Port")
    ->withDescription("The port of the proxy server")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

core::Property SFTPProcessorBase::HttpProxyUsername(core::PropertyBuilder::createProperty("Http Proxy Username")
    ->withDescription("Http Proxy Username")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

core::Property SFTPProcessorBase::HttpProxyPassword(core::PropertyBuilder::createProperty("Http Proxy Password")
    ->withDescription("Http Proxy Password")
    ->isRequired(false)->supportsExpressionLanguage(true)->build());

constexpr char const* SFTPProcessorBase::PROXY_TYPE_DIRECT;
constexpr char const* SFTPProcessorBase::PROXY_TYPE_HTTP;
constexpr char const* SFTPProcessorBase::PROXY_TYPE_SOCKS;

constexpr size_t SFTPProcessorBase::CONNECTION_CACHE_MAX_SIZE;

SFTPProcessorBase::SFTPProcessorBase(const std::string& name, const utils::Identifier& uuid)
    : Processor(name, uuid),
      connection_timeout_(0),
      data_timeout_(0),
      strict_host_checking_(false),
      use_keepalive_on_timeout_(false),
      use_compression_(false),
      running_(true) {
}

SFTPProcessorBase::~SFTPProcessorBase() {
  if (keepalive_thread_.joinable()) {
    {
      std::lock_guard<std::mutex> lock(connections_mutex_);
      running_ = false;
      keepalive_cv_.notify_one();
    }
    keepalive_thread_.join();
  }
}

void SFTPProcessorBase::notifyStop() {
  logger_->log_debug("Got notifyStop, stopping keepalive thread and clearing connections");
  cleanupConnectionCache();
}

void SFTPProcessorBase::addSupportedCommonProperties(std::set<core::Property>& supported_properties) {
  supported_properties.insert(Hostname);
  supported_properties.insert(Port);
  supported_properties.insert(Username);
  supported_properties.insert(Password);
  supported_properties.insert(PrivateKeyPath);
  supported_properties.insert(PrivateKeyPassphrase);
  supported_properties.insert(StrictHostKeyChecking);
  supported_properties.insert(HostKeyFile);
  supported_properties.insert(ConnectionTimeout);
  supported_properties.insert(DataTimeout);
  supported_properties.insert(SendKeepaliveOnTimeout);
  supported_properties.insert(ProxyType);
  supported_properties.insert(ProxyHost);
  supported_properties.insert(ProxyPort);
  supported_properties.insert(HttpProxyUsername);
  supported_properties.insert(HttpProxyPassword);
}

void SFTPProcessorBase::parseCommonPropertiesOnSchedule(const std::shared_ptr<core::ProcessContext>& context) {
  std::string value;
  if (!context->getProperty(StrictHostKeyChecking.getName(), value)) {
    logger_->log_error("Strict Host Key Checking attribute is missing or invalid");
  } else {
    strict_host_checking_ = utils::StringUtils::toBool(value).value_or(false);
  }
  context->getProperty(HostKeyFile.getName(), host_key_file_);
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
    use_keepalive_on_timeout_ = utils::StringUtils::toBool(value).value_or(true);
  }
  context->getProperty(ProxyType.getName(), proxy_type_);
}

SFTPProcessorBase::CommonProperties::CommonProperties()
    : port(0U)
    , proxy_port(0U) {
}

bool SFTPProcessorBase::parseCommonPropertiesOnTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file, CommonProperties& common_properties) {
  std::string value;
  if (!context->getProperty(Hostname, common_properties.hostname, flow_file)) {
    logger_->log_error("Hostname attribute is missing");
    return false;
  }
  if (!context->getProperty(Port, value, flow_file)) {
    logger_->log_error("Port attribute is missing or invalid");
    return false;
  } else {
    int port_tmp;
    if (!core::Property::StringToInt(value, port_tmp) ||
        port_tmp <= std::numeric_limits<uint16_t>::min() ||
        port_tmp > std::numeric_limits<uint16_t>::max()) {
      logger_->log_error("Port attribute \"%s\" is invalid", value);
      return false;
    } else {
      common_properties.port = static_cast<uint16_t>(port_tmp);
    }
  }
  if (!context->getProperty(Username, common_properties.username, flow_file)) {
    logger_->log_error("Username attribute is missing");
    return false;
  }
  context->getProperty(Password, common_properties.password, flow_file);
  context->getProperty(PrivateKeyPath, common_properties.private_key_path, flow_file);
  context->getProperty(PrivateKeyPassphrase, common_properties.private_key_passphrase, flow_file);
  context->getProperty(Password, common_properties.password, flow_file);
  context->getProperty(ProxyHost, common_properties.proxy_host, flow_file);
  if (context->getProperty(ProxyPort, value, flow_file) && !value.empty()) {
    int port_tmp;
    if (!core::Property::StringToInt(value, port_tmp) ||
        port_tmp <= std::numeric_limits<uint16_t>::min() ||
        port_tmp > std::numeric_limits<uint16_t>::max()) {
      logger_->log_error("Proxy Port attribute \"%s\" is invalid", value);
      return false;
    } else {
      common_properties.proxy_port = static_cast<uint16_t>(port_tmp);
    }
  }
  context->getProperty(HttpProxyUsername, common_properties.proxy_username, flow_file);
  context->getProperty(HttpProxyPassword, common_properties.proxy_password, flow_file);

  return true;
}

bool SFTPProcessorBase::ConnectionCacheKey::operator<(const SFTPProcessorBase::ConnectionCacheKey& other) const {
  return std::tie(hostname, port, username, proxy_type, proxy_host, proxy_port, proxy_username) <
         std::tie(other.hostname, other.port, other.username, other.proxy_type, other.proxy_host, other.proxy_port, other.proxy_username);
}

bool SFTPProcessorBase::ConnectionCacheKey::operator==(const SFTPProcessorBase::ConnectionCacheKey& other) const {
  return std::tie(hostname, port, username, proxy_type, proxy_host, proxy_port, proxy_username) ==
         std::tie(other.hostname, other.port, other.username, other.proxy_type, other.proxy_host, other.proxy_port, other.proxy_username);
}

std::unique_ptr<utils::SFTPClient> SFTPProcessorBase::getConnectionFromCache(const SFTPProcessorBase::ConnectionCacheKey& key) {
  std::lock_guard<std::mutex> lock(connections_mutex_);

  auto it = connections_.find(key);
  if (it == connections_.end()) {
    return nullptr;
  }

  logger_->log_debug("Removing %s@%s:%hu from SFTP connection pool",
                     key.username,
                     key.hostname,
                     key.port);

  auto lru_it = std::find(lru_.begin(), lru_.end(), key);
  if (lru_it == lru_.end()) {
    logger_->log_trace("Assertion error: can't find key in LRU cache");
  } else {
    lru_.erase(lru_it);
  }

  auto connection = std::move(it->second);
  connections_.erase(it);
  return connection;
}

void SFTPProcessorBase::addConnectionToCache(const SFTPProcessorBase::ConnectionCacheKey& key, std::unique_ptr<utils::SFTPClient>&& connection) {
  std::lock_guard<std::mutex> lock(connections_mutex_);

  while (connections_.size() >= SFTPProcessorBase::CONNECTION_CACHE_MAX_SIZE) {
    const auto& lru_key = lru_.back();
    logger_->log_debug("SFTP connection pool is full, removing %s@%s:%hu",
                       lru_key.username,
                       lru_key.hostname,
                       lru_key.port);
    connections_.erase(lru_key);
    lru_.pop_back();
  }

  logger_->log_debug("Adding %s@%s:%hu to SFTP connection pool",
                     key.username,
                     key.hostname,
                     key.port);
  connections_.emplace(key, std::move(connection));
  lru_.push_front(key);
  keepalive_cv_.notify_one();
}

void SFTPProcessorBase::keepaliveThreadFunc() {
  std::unique_lock<std::mutex> lock(connections_mutex_);

  while (true) {
    if (connections_.empty()) {
      keepalive_cv_.wait(lock, [this] {
        return !running_ || !connections_.empty();
      });
    }
    if (!running_) {
      logger_->log_trace("Stopping keepalive thread");
      lock.unlock();
      return;
    }

    int min_wait = 10;
    for (auto &connection : connections_) {
      int seconds_to_next = 0;
      if (connection.second->sendKeepAliveIfNeeded(seconds_to_next)) {
        logger_->log_debug("Sent keepalive to %s@%s:%hu if needed, next keepalive in %d s",
                           connection.first.username,
                           connection.first.hostname,
                           connection.first.port,
                           seconds_to_next);
        if (seconds_to_next < min_wait) {
          min_wait = seconds_to_next;
        }
      } else {
        logger_->log_debug("Failed to send keepalive to %s@%s:%hu",
                           connection.first.username,
                           connection.first.hostname,
                           connection.first.port);
      }
    }

    /* Avoid busy loops */
    if (min_wait < 1) {
      min_wait = 1;
    }

    logger_->log_trace("Keepalive thread is going to sleep for %d s", min_wait);
    keepalive_cv_.wait_for(lock, std::chrono::seconds(min_wait), [this] {
      return !running_;
    });
    if (!running_) {
      lock.unlock();
      return;
    }
  }
}

void SFTPProcessorBase::startKeepaliveThreadIfNeeded() {
  if (use_keepalive_on_timeout_ && !keepalive_thread_.joinable()) {
    running_ = true;
    keepalive_thread_ = std::thread(&SFTPProcessorBase::keepaliveThreadFunc, this);
  }
}

void SFTPProcessorBase::cleanupConnectionCache() {
  if (keepalive_thread_.joinable()) {
    {
      std::lock_guard<std::mutex> lock(connections_mutex_);
      running_ = false;
      keepalive_cv_.notify_one();
    }
    keepalive_thread_.join();
  }
  /* The thread is no longer running, we don't have to lock */
  connections_.clear();
  lru_.clear();
}

std::unique_ptr<utils::SFTPClient> SFTPProcessorBase::getOrCreateConnection(
    const SFTPProcessorBase::ConnectionCacheKey& connection_cache_key,
    const std::string& password,
    const std::string& private_key_path,
    const std::string& private_key_passphrase,
    const std::string& proxy_password) {
  auto client = getConnectionFromCache(connection_cache_key);
  if (client == nullptr) {
    client = std::unique_ptr<utils::SFTPClient>(new utils::SFTPClient(connection_cache_key.hostname,
                                                                      connection_cache_key.port,
                                                                      connection_cache_key.username));
    if (!IsNullOrEmpty(host_key_file_)) {
      if (!client->setHostKeyFile(host_key_file_, strict_host_checking_)) {
        logger_->log_error("Cannot set host key file");
        return nullptr;
      }
    }
    if (!IsNullOrEmpty(password)) {
      client->setPasswordAuthenticationCredentials(password);
    }
    if (!IsNullOrEmpty(private_key_path)) {
      client->setPublicKeyAuthenticationCredentials(private_key_path, private_key_passphrase);
    }
    if (connection_cache_key.proxy_type != PROXY_TYPE_DIRECT) {
      utils::HTTPProxy proxy;
      proxy.host = connection_cache_key.proxy_host;
      proxy.port = connection_cache_key.proxy_port;
      proxy.username = connection_cache_key.proxy_username;
      proxy.password = proxy_password;
      if (!client->setProxy(
          connection_cache_key.proxy_type == PROXY_TYPE_HTTP ? utils::SFTPClient::ProxyType::Http : utils::SFTPClient::ProxyType::Socks,
          proxy)) {
        logger_->log_error("Cannot set proxy");
        return nullptr;
      }
    }
    if (!client->setConnectionTimeout(connection_timeout_)) {
      logger_->log_error("Cannot set connection timeout");
      return nullptr;
    }
    client->setDataTimeout(data_timeout_);
    client->setSendKeepAlive(use_keepalive_on_timeout_);
    if (!client->setUseCompression(use_compression_)) {
      logger_->log_error("Cannot set compression");
      return nullptr;
    }

    /* Connect to SFTP server */
    if (!client->connect()) {
      logger_->log_error("Cannot connect to SFTP server");
      return nullptr;
    }
  }

  return client;
}

SFTPProcessorBase::CreateDirectoryHierarchyError SFTPProcessorBase::createDirectoryHierarchy(
    utils::SFTPClient& client,
    const std::string& remote_path,
    bool disable_directory_listing) {
  bool should_create_directory = disable_directory_listing;
  if (!disable_directory_listing) {
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    if (!client.stat(remote_path, true /*follow_symlinks*/, attrs)) {
      if (client.getLastError() != utils::SFTPError::FileDoesNotExist) {
        logger_->log_error("Failed to stat %s", remote_path.c_str());
      }
      should_create_directory = true;
    } else {
      if (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS && !LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
        logger_->log_error("Remote path %s is not a directory", remote_path.c_str());
        return CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_NOT_A_DIRECTORY;
      }
      logger_->log_debug("Found remote directory %s", remote_path.c_str());
    }
  }
  if (should_create_directory) {
    (void) client.createDirectoryHierarchy(remote_path);
    if (!disable_directory_listing) {
      LIBSSH2_SFTP_ATTRIBUTES attrs;
      if (!client.stat(remote_path, true /*follow_symlinks*/, attrs)) {
        auto last_error = client.getLastError();
        if (last_error == utils::SFTPError::FileDoesNotExist) {
          logger_->log_error("Could not find remote directory %s after creating it", remote_path.c_str());
          return CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_NOT_FOUND;
        } else if (last_error == utils::SFTPError::PermissionDenied) {
          logger_->log_error("Permission denied when reading remote directory %s after creating it", remote_path.c_str());
          return CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_PERMISSION_DENIED;
        } else {
          logger_->log_error("Failed to stat %s", remote_path.c_str());
          return CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_STAT_FAILED;
        }
      } else {
        if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) && !LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
          logger_->log_error("Remote path %s is not a directory", remote_path.c_str());
          return CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_NOT_A_DIRECTORY;
        }
      }
    }
  }

  return CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_OK;
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
