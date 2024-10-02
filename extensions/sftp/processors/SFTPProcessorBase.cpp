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
#include "ResourceClaim.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::processors {

SFTPProcessorBase::SFTPProcessorBase(const std::string_view name, const utils::Identifier& uuid)
    : ProcessorImpl(name, uuid),
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

void SFTPProcessorBase::parseCommonPropertiesOnSchedule(core::ProcessContext& context) {
  std::string value;
  if (!context.getProperty(StrictHostKeyChecking, value)) {
    logger_->log_error("Strict Host Key Checking attribute is missing or invalid");
  } else {
    strict_host_checking_ = utils::string::toBool(value).value_or(false);
  }
  context.getProperty(HostKeyFile, host_key_file_);
  if (auto connection_timeout = context.getProperty<core::TimePeriodValue>(ConnectionTimeout)) {
    connection_timeout_ = connection_timeout->getMilliseconds();
  } else {
    logger_->log_error("Connection Timeout attribute is missing or invalid");
  }

  if (auto data_timeout = context.getProperty<core::TimePeriodValue>(DataTimeout)) {
    data_timeout_ = data_timeout->getMilliseconds();
  } else {
    logger_->log_error("Data Timeout attribute is missing or invalid");
  }

  if (!context.getProperty(SendKeepaliveOnTimeout, value)) {
    logger_->log_error("Send Keep Alive On Timeout attribute is missing or invalid");
  } else {
    use_keepalive_on_timeout_ = utils::string::toBool(value).value_or(true);
  }
  context.getProperty(ProxyType, proxy_type_);
}

SFTPProcessorBase::CommonProperties::CommonProperties()
    : port(0U)
    , proxy_port(0U) {
}

bool SFTPProcessorBase::parseCommonPropertiesOnTrigger(core::ProcessContext& context, const core::FlowFile* const flow_file, CommonProperties& common_properties) {
  std::string value;
  if (!context.getProperty(Hostname, common_properties.hostname, flow_file)) {
    logger_->log_error("Hostname attribute is missing");
    return false;
  }
  if (!context.getProperty(Port, value, flow_file)) {
    logger_->log_error("Port attribute is missing or invalid");
    return false;
  } else {
    int port_tmp = 0;
    if (!core::Property::StringToInt(value, port_tmp) ||
        port_tmp <= std::numeric_limits<uint16_t>::min() ||
        port_tmp > std::numeric_limits<uint16_t>::max()) {
      logger_->log_error("Port attribute \"{}\" is invalid", value);
      return false;
    } else {
      common_properties.port = static_cast<uint16_t>(port_tmp);
    }
  }
  if (!context.getProperty(Username, common_properties.username, flow_file)) {
    logger_->log_error("Username attribute is missing");
    return false;
  }
  context.getProperty(Password, common_properties.password, flow_file);
  context.getProperty(PrivateKeyPath, common_properties.private_key_path, flow_file);
  context.getProperty(PrivateKeyPassphrase, common_properties.private_key_passphrase, flow_file);
  context.getProperty(Password, common_properties.password, flow_file);
  context.getProperty(ProxyHost, common_properties.proxy_host, flow_file);
  if (context.getProperty(ProxyPort, value, flow_file) && !value.empty()) {
    int port_tmp = 0;
    if (!core::Property::StringToInt(value, port_tmp) ||
        port_tmp <= std::numeric_limits<uint16_t>::min() ||
        port_tmp > std::numeric_limits<uint16_t>::max()) {
      logger_->log_error("Proxy Port attribute \"{}\" is invalid", value);
      return false;
    } else {
      common_properties.proxy_port = static_cast<uint16_t>(port_tmp);
    }
  }
  context.getProperty(HttpProxyUsername, common_properties.proxy_username, flow_file);
  context.getProperty(HttpProxyPassword, common_properties.proxy_password, flow_file);

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

  logger_->log_debug("Removing {}@{}:{} from SFTP connection pool",
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
    logger_->log_debug("SFTP connection pool is full, removing {}@{}:{}",
                       lru_key.username,
                       lru_key.hostname,
                       lru_key.port);
    connections_.erase(lru_key);
    lru_.pop_back();
  }

  logger_->log_debug("Adding {}@{}:{} to SFTP connection pool",
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
        logger_->log_debug("Sent keepalive to {}@{}:{} if needed, next keepalive in {} s",
                           connection.first.username,
                           connection.first.hostname,
                           connection.first.port,
                           seconds_to_next);
        if (seconds_to_next < min_wait) {
          min_wait = seconds_to_next;
        }
      } else {
        logger_->log_debug("Failed to send keepalive to {}@{}:{}",
                           connection.first.username,
                           connection.first.hostname,
                           connection.first.port);
      }
    }

    /* Avoid busy loops */
    if (min_wait < 1) {
      min_wait = 1;
    }

    logger_->log_trace("Keepalive thread is going to sleep for {} s", min_wait);
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
    client = std::make_unique<utils::SFTPClient>(connection_cache_key.hostname,
                                                 connection_cache_key.port,
                                                 connection_cache_key.username);
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
      http::HTTPProxy proxy;
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
        logger_->log_error("Failed to stat {}", remote_path.c_str());
      }
      should_create_directory = true;
    } else {
      if (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS && !LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
        logger_->log_error("Remote path {} is not a directory", remote_path.c_str());
        return CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_NOT_A_DIRECTORY;
      }
      logger_->log_debug("Found remote directory {}", remote_path.c_str());
    }
  }
  if (should_create_directory) {
    (void) client.createDirectoryHierarchy(remote_path);
    if (!disable_directory_listing) {
      LIBSSH2_SFTP_ATTRIBUTES attrs;
      if (!client.stat(remote_path, true /*follow_symlinks*/, attrs)) {
        auto last_error = client.getLastError();
        if (last_error == utils::SFTPError::FileDoesNotExist) {
          logger_->log_error("Could not find remote directory {} after creating it", remote_path.c_str());
          return CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_NOT_FOUND;
        } else if (last_error == utils::SFTPError::PermissionDenied) {
          logger_->log_error("Permission denied when reading remote directory {} after creating it", remote_path.c_str());
          return CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_PERMISSION_DENIED;
        } else {
          logger_->log_error("Failed to stat {}", remote_path.c_str());
          return CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_STAT_FAILED;
        }
      } else {
        if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) && !LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
          logger_->log_error("Remote path {} is not a directory", remote_path.c_str());
          return CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_NOT_A_DIRECTORY;
        }
      }
    }
  }

  return CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_OK;
}

}  // namespace org::apache::nifi::minifi::processors
