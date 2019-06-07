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

constexpr size_t SFTPProcessorBase::CONNECTION_CACHE_MAX_SIZE;

SFTPProcessorBase::SFTPProcessorBase(std::string name, utils::Identifier uuid)
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
      if (client.getLastError() != utils::SFTPError::SFTP_ERROR_FILE_NOT_EXISTS) {
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
        if (last_error == utils::SFTPError::SFTP_ERROR_FILE_NOT_EXISTS) {
          logger_->log_error("Could not find remote directory %s after creating it", remote_path.c_str());
          return CreateDirectoryHierarchyError::CREATE_DIRECTORY_HIERARCHY_ERROR_NOT_FOUND;
        } else if (last_error == utils::SFTPError::SFTP_ERROR_PERMISSION_DENIED) {
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
