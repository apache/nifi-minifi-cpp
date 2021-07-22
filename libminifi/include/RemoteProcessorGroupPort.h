/**
 * @file RemoteProcessorGroupPort.h
 * RemoteProcessorGroupPort class declaration
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
#ifndef LIBMINIFI_INCLUDE_REMOTEPROCESSORGROUPPORT_H_
#define LIBMINIFI_INCLUDE_REMOTEPROCESSORGROUPPORT_H_

#include <string>
#include <utility>
#include <vector>
#include <mutex>
#include <memory>
#include <stack>
#include "utils/HTTPClient.h"
#include "concurrentqueue.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "sitetosite/SiteToSiteClient.h"
#include "io/StreamFactory.h"
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

/**
 * Count down latch implementation that's used across
 * all threads of the RPG. This is okay since the latch increments
 * and decrements based on its construction. Using RAII we should
 * never have the concern of thread safety.
 */
class RPGLatch {
 public:
  RPGLatch(bool increment = true) { // NOLINT
    static std::atomic<int> latch_count(0);
    count = &latch_count;
    if (increment)
      count++;
  }

  ~RPGLatch() {
    count--;
  }

  int getCount() {
    return *count;
  }

 private:
  std::atomic<int> *count;
};

struct RPG {
  std::string host_;
  int port_;
  std::string protocol_;
};

// RemoteProcessorGroupPort Class
class RemoteProcessorGroupPort : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  RemoteProcessorGroupPort(const std::shared_ptr<io::StreamFactory> &stream_factory, const std::string &name, std::string url, const std::shared_ptr<Configure> &configure, const utils::Identifier &uuid = {}) // NOLINT
      : core::Processor(name, uuid),
        configure_(configure),
        direction_(sitetosite::SEND),
        transmitting_(false),
        timeout_(0),
        http_enabled_(false),
        bypass_rest_api_(false),
        ssl_service(nullptr),
        logger_(logging::LoggerFactory<RemoteProcessorGroupPort>::getLogger()) {
    client_type_ = sitetosite::CLIENT_TYPE::RAW;
    stream_factory_ = stream_factory;
    protocol_uuid_ = uuid;
    site2site_secure_ = false;
    peer_index_ = -1;
    // REST API port and host
    setURL(url);
  }
  // Destructor
  virtual ~RemoteProcessorGroupPort() = default;

  // Processor Name
  MINIFIAPI static const char *ProcessorName;
  // Supported Properties
  MINIFIAPI static core::Property hostName;
  MINIFIAPI static core::Property SSLContext;
  MINIFIAPI static core::Property port;
  MINIFIAPI static core::Property portUUID;
  MINIFIAPI static core::Property idleTimeout;
  // Supported Relationships
  MINIFIAPI static core::Relationship relation;

 public:
  virtual void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory);
  // OnTrigger method, implemented by NiFi RemoteProcessorGroupPort
  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);

  // Initialize, over write by NiFi RemoteProcessorGroupPort
  virtual void initialize(void);
  // Set Direction
  void setDirection(sitetosite::TransferDirection direction) {
    direction_ = direction;
    if (direction_ == sitetosite::RECEIVE)
      this->setTriggerWhenEmpty(true);
  }
  // Set Timeout
  void setTimeOut(uint64_t timeout) {
    timeout_ = timeout;
  }
  // SetTransmitting
  void setTransmitting(bool val) {
    transmitting_ = val;
  }
  // setInterface
  void setInterface(const std::string &ifc) {
    local_network_interface_ = ifc;
  }
  std::string getInterface() {
    return local_network_interface_;
  }
  /**
   * Sets the url. Supports a CSV
   */
  void setURL(std::string val) {
    auto urls = utils::StringUtils::split(val, ",");
    for (const auto& url : urls) {
      utils::URL parsed_url{utils::StringUtils::trim(url)};
      if (parsed_url.isValid()) {
        logger_->log_debug("Parsed RPG URL '%s' -> '%s'", url, parsed_url.hostPort());
        nifi_instances_.push_back({parsed_url.host(), parsed_url.port(), parsed_url.protocol()});
      } else {
        logger_->log_error("Could not parse RPG URL '%s'", url);
      }
    }
  }

  void setHTTPProxy(const utils::HTTPProxy &proxy) {
    this->proxy_ = proxy;
  }
  utils::HTTPProxy getHTTPProxy() {
    return this->proxy_;
  }
  // refresh remoteSite2SiteInfo via nifi rest api
  std::pair<std::string, int> refreshRemoteSite2SiteInfo();

  // refresh site2site peer list
  void refreshPeerList();

  virtual void notifyStop();

  void enableHTTP() {
    client_type_ = sitetosite::HTTP;
  }

 protected:
  /**
   * Non static in case anything is loaded when this object is re-scheduled
   */
  bool is_http_disabled() {
    auto ptr = core::ClassLoader::getDefaultClassLoader().instantiateRaw("HTTPClient", "HTTPClient");
    if (ptr != nullptr) {
      delete ptr;
      return false;
    } else {
      return true;
    }
  }

  std::shared_ptr<io::StreamFactory> stream_factory_;
  std::unique_ptr<sitetosite::SiteToSiteClient> getNextProtocol(bool create);
  void returnProtocol(std::unique_ptr<sitetosite::SiteToSiteClient> protocol);

  moodycamel::ConcurrentQueue<std::unique_ptr<sitetosite::SiteToSiteClient>> available_protocols_;

  std::shared_ptr<Configure> configure_;
  // Transaction Direction
  sitetosite::TransferDirection direction_;
  // Transmitting
  std::atomic<bool> transmitting_;
  // timeout
  uint64_t timeout_;
  // local network interface
  std::string local_network_interface_;

  utils::Identifier protocol_uuid_;

  std::chrono::milliseconds idle_timeout_{15000};

  // rest API end point info
  std::vector<struct RPG> nifi_instances_;

  bool http_enabled_;
  // http proxy
  utils::HTTPProxy proxy_;

  bool bypass_rest_api_;

  sitetosite::CLIENT_TYPE client_type_;

  // Remote Site2Site Info
  bool site2site_secure_;
  std::vector<sitetosite::PeerStatus> peers_;
  std::atomic<int> peer_index_;
  std::mutex peer_mutex_;
  std::string rest_user_name_;
  std::string rest_password_;

  std::shared_ptr<controllers::SSLContextService> ssl_service;

 private:
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  static const char* RPG_SSL_CONTEXT_SERVICE_NAME;
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_REMOTEPROCESSORGROUPPORT_H_
