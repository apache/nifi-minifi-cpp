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
#ifndef __PUT_SFTP_H__
#define __PUT_SFTP_H__

#include <memory>
#include <string>
#include <list>
#include <map>
#include <mutex>
#include <thread>

#include "utils/ByteArrayCallback.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/Resource.h"
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"
#include "../client/SFTPClient.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class PutSFTP : public core::Processor {
 public:

  static constexpr char const *CONFLICT_RESOLUTION_REPLACE = "REPLACE";
  static constexpr char const *CONFLICT_RESOLUTION_IGNORE = "IGNORE";
  static constexpr char const *CONFLICT_RESOLUTION_RENAME = "RENAME";
  static constexpr char const *CONFLICT_RESOLUTION_REJECT = "REJECT";
  static constexpr char const *CONFLICT_RESOLUTION_FAIL = "FAIL";
  static constexpr char const *CONFLICT_RESOLUTION_NONE = "NONE";

  static constexpr char const *PROXY_TYPE_DIRECT = "DIRECT";
  static constexpr char const *PROXY_TYPE_HTTP = "HTTP";
  static constexpr char const *PROXY_TYPE_SOCKS = "SOCKS";

  static constexpr char const* ProcessorName = "PutSFTP";


  /*!
   * Create a new processor
   */
  PutSFTP(std::string name, utils::Identifier uuid = utils::Identifier());
  virtual ~PutSFTP();

  // Supported Properties
  static core::Property Hostname;
  static core::Property Port;
  static core::Property Username;
  static core::Property Password;
  static core::Property PrivateKeyPath;
  static core::Property PrivateKeyPassphrase;
  static core::Property RemotePath;
  static core::Property CreateDirectory;
  static core::Property DisableDirectoryListing;
  static core::Property BatchSize;
  static core::Property ConnectionTimeout;
  static core::Property DataTimeout;
  static core::Property ConflictResolution;
  static core::Property RejectZeroByte;
  static core::Property DotRename;
  static core::Property TempFilename;
  static core::Property HostKeyFile;
  static core::Property LastModifiedTime;
  static core::Property Permissions;
  static core::Property RemoteOwner;
  static core::Property RemoteGroup;
  static core::Property StrictHostKeyChecking;
  static core::Property UseKeepaliveOnTimeout;
  static core::Property UseCompression;
  static core::Property ProxyType;
  static core::Property ProxyHost;
  static core::Property ProxyPort;
  static core::Property HttpProxyUsername;
  static core::Property HttpProxyPassword;

  // Supported Relationships
  static core::Relationship Success;
  static core::Relationship Reject;
  static core::Relationship Failure;

  virtual bool supportsDynamicProperties() override {
    return true;
  }

  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  virtual void initialize() override;
  virtual void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  virtual void notifyStop() override;

  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(const std::string& target_path,
        utils::SFTPClient& client,
        const std::string& conflict_resolution);
    ~ReadCallback();
    virtual int64_t process(std::shared_ptr<io::BaseStream> stream) override;
    bool commit();

   private:
    std::shared_ptr<logging::Logger> logger_;
    bool write_succeeded_;
    const std::string target_path_;
    utils::SFTPClient& client_;
    const std::string conflict_resolution_;
  };

 private:

  std::shared_ptr<logging::Logger> logger_;

  bool create_directory_;
  uint64_t batch_size_;
  int64_t connection_timeout_;
  int64_t data_timeout_;
  std::string conflict_resolution_;
  bool reject_zero_byte_;
  bool dot_rename_;
  std::string host_key_file_;
  bool strict_host_checking_;
  bool use_keepalive_on_timeout_;
  bool use_compression_;
  std::string proxy_type_;

  static constexpr size_t CONNECTION_CACHE_MAX_SIZE = 8U;
  struct ConnectionCacheKey {
    std::string hostname;
    uint16_t port;
    std::string username;
    std::string proxy_type;
    std::string proxy_host;
    uint16_t proxy_port;

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

  bool processOne(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);
};

REGISTER_RESOURCE(PutSFTP, "Sends FlowFiles to an SFTP Server")

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
