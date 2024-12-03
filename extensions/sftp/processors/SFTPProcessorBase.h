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
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "controllers/SSLContextService.h"
#include "utils/Id.h"
#include "../client/SFTPClient.h"

namespace org::apache::nifi::minifi::processors {

class SFTPProcessorBase : public core::ProcessorImpl {
 public:
  SFTPProcessorBase(std::string_view name, const utils::Identifier& uuid);
  ~SFTPProcessorBase() override;

  static constexpr std::string_view PROXY_TYPE_DIRECT = "DIRECT";
  static constexpr std::string_view PROXY_TYPE_HTTP = "HTTP";
  static constexpr std::string_view PROXY_TYPE_SOCKS = "SOCKS";

  EXTENSIONAPI static constexpr auto Hostname = core::PropertyDefinitionBuilder<>::createProperty("Hostname")
      .withDescription("The fully qualified hostname or IP address of the remote system")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Port = core::PropertyDefinitionBuilder<>::createProperty("Port")
      .withDescription("The port that the remote system is listening on for file transfers")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Username = core::PropertyDefinitionBuilder<>::createProperty("Username")
      .withDescription("Username")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Password = core::PropertyDefinitionBuilder<>::createProperty("Password")
      .withDescription("Password for the user account")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto PrivateKeyPath = core::PropertyDefinitionBuilder<>::createProperty("Private Key Path")
      .withDescription("The fully qualified path to the Private Key file")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto PrivateKeyPassphrase = core::PropertyDefinitionBuilder<>::createProperty("Private Key Passphrase")
      .withDescription("Password for the private key")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto StrictHostKeyChecking = core::PropertyDefinitionBuilder<>::createProperty("Strict Host Key Checking")
      .withDescription("Indicates whether or not strict enforcement of hosts keys should be applied")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
          .build();
  EXTENSIONAPI static constexpr auto HostKeyFile = core::PropertyDefinitionBuilder<>::createProperty("Host Key File")
      .withDescription("If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used")
      .isRequired(false)
      .build();
  EXTENSIONAPI static constexpr auto ConnectionTimeout = core::PropertyDefinitionBuilder<>::createProperty("Connection Timeout")
      .withDescription("Amount of time to wait before timing out while creating a connection")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("30 sec")
      .build();
  EXTENSIONAPI static constexpr auto DataTimeout = core::PropertyDefinitionBuilder<>::createProperty("Data Timeout")
      .withDescription("When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("30 sec")
      .build();
  EXTENSIONAPI static constexpr auto SendKeepaliveOnTimeout = core::PropertyDefinitionBuilder<>::createProperty("Send Keep Alive On Timeout")
      .withDescription("Indicates whether or not to send a single Keep Alive message when SSH socket times out")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto ProxyType = core::PropertyDefinitionBuilder<3>::createProperty("Proxy Type")
      .withDescription("Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. "
          "Supported proxies: HTTP + AuthN, SOCKS + AuthN")
      .isRequired(false)
      .withAllowedValues({PROXY_TYPE_DIRECT, PROXY_TYPE_HTTP, PROXY_TYPE_SOCKS})
      .withDefaultValue(PROXY_TYPE_DIRECT)
      .build();
  EXTENSIONAPI static constexpr auto ProxyHost = core::PropertyDefinitionBuilder<>::createProperty("Proxy Host")
      .withDescription("The fully qualified hostname or IP address of the proxy server")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ProxyPort = core::PropertyDefinitionBuilder<>::createProperty("Proxy Port")
      .withDescription("The port of the proxy server")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto HttpProxyUsername = core::PropertyDefinitionBuilder<>::createProperty("Http Proxy Username")
      .withDescription("Http Proxy Username")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto HttpProxyPassword = core::PropertyDefinitionBuilder<>::createProperty("Http Proxy Password")
      .withDescription("Http Proxy Password")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
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
  });


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

  void parseCommonPropertiesOnSchedule(core::ProcessContext& context);
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
  bool parseCommonPropertiesOnTrigger(core::ProcessContext& context, const core::FlowFile* const flow_file, CommonProperties& common_properties);

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
