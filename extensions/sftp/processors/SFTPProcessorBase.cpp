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

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "minifi-cpp/ResourceClaim.h"
#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/Relationship.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "io/BufferStream.h"
#include "utils/ByteArrayCallback.h"
#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"
#include "io/validation.h"

namespace org::apache::nifi::minifi::processors {

SFTPProcessorBase::SFTPProcessorBase(core::ProcessorMetadata metadata)
    : ProcessorImpl(std::move(metadata)),
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
  strict_host_checking_ = utils::parseBoolProperty(context, StrictHostKeyChecking);
  host_key_file_ = utils::parseOptionalProperty(context, HostKeyFile).value_or("");
  connection_timeout_ = utils::parseDurationProperty(context, ConnectionTimeout);
  data_timeout_ = utils::parseDurationProperty(context, DataTimeout);
  use_keepalive_on_timeout_ = utils::parseBoolProperty(context, SendKeepaliveOnTimeout);
  proxy_type_ = utils::parseProperty(context, ProxyType);
}

SFTPProcessorBase::CommonProperties::CommonProperties()
    : port(0U)
    , proxy_port(0U) {
}

bool SFTPProcessorBase::parseCommonPropertiesOnTrigger(core::ProcessContext& context, const core::FlowFile* const flow_file, CommonProperties& common_properties) {
  try {
    common_properties.hostname = utils::parseProperty(context, Hostname, flow_file);
    common_properties.port = gsl::narrow<uint16_t>(utils::parseU64Property(context, Port, flow_file));
    common_properties.username = utils::parseOptionalProperty(context, Username, flow_file).value_or("");
    common_properties.private_key_path = utils::parseOptionalProperty(context, PrivateKeyPath, flow_file).value_or("");
    common_properties.private_key_passphrase = utils::parseOptionalProperty(context, PrivateKeyPassphrase, flow_file).value_or("");
    common_properties.password = utils::parseOptionalProperty(context, Password, flow_file).value_or("");
    common_properties.proxy_host = utils::parseOptionalProperty(context, ProxyHost, flow_file).value_or("");
    common_properties.proxy_port = gsl::narrow<uint16_t>(utils::parseOptionalU64Property(context, ProxyHost, flow_file).value_or(0));
    common_properties.proxy_username = utils::parseOptionalProperty(context, HttpProxyUsername, flow_file).value_or("");
    common_properties.proxy_password = utils::parseOptionalProperty(context, HttpProxyPassword, flow_file).value_or("");
  } catch (const std::exception& e) {
    logger_->log_error("Failed to parse common properties on {}", e.what());
    return false;
  }

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
        min_wait = std::min(min_wait, seconds_to_next);
      } else {
        logger_->log_debug("Failed to send keepalive to {}@{}:{}",
                           connection.first.username,
                           connection.first.hostname,
                           connection.first.port);
      }
    }

    /* Avoid busy loops */
    min_wait = std::max(min_wait, 1);

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
    const std::string& proxy_password,
    const size_t buffer_size) {
  auto client = getConnectionFromCache(connection_cache_key);
  if (client == nullptr) {
    client = std::make_unique<utils::SFTPClient>(connection_cache_key.hostname,
                                                 connection_cache_key.port,
                                                 connection_cache_key.username,
                                                 buffer_size);
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
