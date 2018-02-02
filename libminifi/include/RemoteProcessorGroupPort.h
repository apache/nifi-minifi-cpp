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
#ifndef __REMOTE_PROCESSOR_GROUP_PORT_H__
#define __REMOTE_PROCESSOR_GROUP_PORT_H__

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
class RPGLatch{
 public:
  RPGLatch(bool increment = true){
    static std::atomic<int> latch_count (0);
    count = &latch_count;
    if (increment)
      count++;
  }

  ~RPGLatch(){
    count--;
  }

  int getCount(){
    return *count;
  }

 private:
  std::atomic<int> *count;
};

// RemoteProcessorGroupPort Class
class RemoteProcessorGroupPort : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  RemoteProcessorGroupPort(const std::shared_ptr<io::StreamFactory> &stream_factory, std::string name, std::string url, const std::shared_ptr<Configure> &configure, uuid_t uuid = nullptr)
      : core::Processor(name, uuid),
        configure_(configure),
        direction_(sitetosite::SEND),
        transmitting_(false),
        timeout_(0),
        url_(url),
        http_enabled_(false),
        ssl_service(nullptr),
        logger_(logging::LoggerFactory<RemoteProcessorGroupPort>::getLogger()){
    client_type_ = sitetosite::CLIENT_TYPE::RAW;
    stream_factory_ = stream_factory;
    if (uuid != nullptr) {
      uuid_copy(protocol_uuid_, uuid);
    }
    site2site_port_ = -1;
    site2site_secure_ = false;
    peer_index_ = -1;
    // REST API port and host
    port_ = -1;
    utils::parse_url(&url_, &host_, &port_, &protocol_);
  }
  // Destructor
  virtual ~RemoteProcessorGroupPort() {

  }

  // Processor Name
  static const char *ProcessorName;
  // Supported Properties
  static core::Property hostName;
  static core::Property SSLContext;
  static core::Property port;
  static core::Property portUUID;
  // Supported Relationships
  static core::Relationship relation;
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
  // setURL
  void setURL(std::string val) {
    url_ = val;
    utils::parse_url(&url_, &host_, &port_, &protocol_);
    if (port_ == -1) {
      if (protocol_.find("https") != std::string::npos) {
        port_ = 443;
      } else if (protocol_.find("http") != std::string::npos) {
        port_ = 80;
      }
    }
  }

  // refresh remoteSite2SiteInfo via nifi rest api
  void refreshRemoteSite2SiteInfo();

  // refresh site2site peer list
  void refreshPeerList();

  virtual void notifyStop();

 protected:

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

  uuid_t protocol_uuid_;

  // rest API end point info
  std::string host_;
  int port_;
  std::string protocol_;
  std::string url_;
  bool http_enabled_;

  sitetosite::CLIENT_TYPE client_type_;


  // Remote Site2Site Info
  int site2site_port_;
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

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
