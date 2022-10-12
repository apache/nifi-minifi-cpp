/**
 * PutSFTP class declaration
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
#pragma once

#include <memory>
#include <string>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "controllers/SSLContextService.h"
#include "utils/Id.h"
#include "../client/SFTPClient.h"

namespace org::apache::nifi::minifi::processors {

class SFTPProcessorBase : public core::Processor {
 public:
  SFTPProcessorBase(std::string name, const utils::Identifier& uuid);
  ~SFTPProcessorBase() override;

  EXTENSIONAPI static const core::Property Hostname;
  EXTENSIONAPI static const core::Property Port;
  EXTENSIONAPI static const core::Property Username;
  EXTENSIONAPI static const core::Property Password;
  EXTENSIONAPI static const core::Property PrivateKeyPath;
  EXTENSIONAPI static const core::Property PrivateKeyPassphrase;
  EXTENSIONAPI static const core::Property StrictHostKeyChecking;
  EXTENSIONAPI static const core::Property HostKeyFile;
  EXTENSIONAPI static const core::Property ConnectionTimeout;
  EXTENSIONAPI static const core::Property DataTimeout;
  EXTENSIONAPI static const core::Property SendKeepaliveOnTimeout;
  EXTENSIONAPI static const core::Property ProxyType;
  EXTENSIONAPI static const core::Property ProxyHost;
  EXTENSIONAPI static const core::Property ProxyPort;
  EXTENSIONAPI static const core::Property HttpProxyUsername;
  EXTENSIONAPI static const core::Property HttpProxyPassword;
  static auto properties() {
    return std::array{
      Hostname,
      Port,
      Username,
      Password,
      PrivateKeyPath,
      PrivateKeyPassphrase,
      StrictHostKeyChecking,
      HostKeyFile,
      ConnectionTimeout,
      DataTimeout,
      SendKeepaliveOnTimeout,
      ProxyType,
      ProxyHost,
      ProxyPort,
      HttpProxyUsername,
      HttpProxyPassword
    };
  }

  static constexpr char const *PROXY_TYPE_DIRECT = "DIRECT";
  static constexpr char const *PROXY_TYPE_HTTP = "HTTP";
  static constexpr char const *PROXY_TYPE_SOCKS = "SOCKS";

  void notifyStop() override;

 protected:
  std::shared_ptr<core::logging::Logger> logger_;

  std::chrono::milliseconds connection_timeout_;
  std::chrono::milliseconds data_timeout_;
  std::string host_key_file_;
  bool strict_host_checking_;
  bool use_keepalive_on_timeout_;
  bool use_compression_;
  std::string proxy_type_;

  void parseCommonPropertiesOnSchedule(const std::shared_ptr<core::ProcessContext>& context);
  struct CommonProperties {
    std::string hostname;
    uint16_t port;
    std::string username;
    std::string password;
    std::string private_key_path;
    std::string private_key_passphrase;
    std::string remote_path;
    std::string proxy_host;
    uint16_t proxy_port;
    std::string proxy_username;
    std::string proxy_password;

    CommonProperties();
  };
  bool parseCommonPropertiesOnTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file, CommonProperties& common_properties);

  static constexpr size_t CONNECTION_CACHE_MAX_SIZE = 8U;
  struct ConnectionCacheKey {
    std::string hostname;
    uint16_t port;
    std::string username;
    std::string proxy_type;
    std::string proxy_host;
    uint16_t proxy_port;
    std::string proxy_username;

    bool operator<(const ConnectionCacheKey& other) const;
    bool operator==(const ConnectionCacheKey& other) const;
  };
  std::mutex connections_mutex_;
  std::map<ConnectionCacheKey, std::unique_ptr<utils::SFTPClient>> connections_;
  std::list<ConnectionCacheKey> lru_;
  std::unique_ptr<utils::SFTPClient> getConnectionFromCache(const ConnectionCacheKey& key);
  void addConnectionToCache(const ConnectionCacheKey& key, std::unique_ptr<utils::SFTPClient>&& connection);

  std::thread keepalive_thread_;
  bool running_;
  std::condition_variable keepalive_cv_;
  void keepaliveThreadFunc();

  void startKeepaliveThreadIfNeeded();
  void cleanupConnectionCache();
  std::unique_ptr<utils::SFTPClient> getOrCreateConnection(
      const SFTPProcessorBase::ConnectionCacheKey& connection_cache_key,
      const std::string& password,
      const std::string& private_key_path,
      const std::string& private_key_passphrase,
      const std::string& proxy_password);

  enum class CreateDirectoryHierarchyError : uint8_t {
    CREATE_DIRECTORY_HIERARCHY_ERROR_OK = 0,
    CREATE_DIRECTORY_HIERARCHY_ERROR_STAT_FAILED,
    CREATE_DIRECTORY_HIERARCHY_ERROR_NOT_A_DIRECTORY,
    CREATE_DIRECTORY_HIERARCHY_ERROR_NOT_FOUND,
    CREATE_DIRECTORY_HIERARCHY_ERROR_PERMISSION_DENIED,
  };
  CreateDirectoryHierarchyError createDirectoryHierarchy(utils::SFTPClient& client, const std::string& remote_path, bool disable_directory_listing);
};

}  // namespace org::apache::nifi::minifi::processors
