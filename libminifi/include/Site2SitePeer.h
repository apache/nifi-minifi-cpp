/**
 * @file Site2SitePeer.h
 * Site2SitePeer class declaration for site to site peer  
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
#ifndef __SITE2SITE_PEER_H__
#define __SITE2SITE_PEER_H__

#include <stdio.h>
#include <fcntl.h>
#include <resolv.h>
#include <netdb.h>
#include <string>
#include <errno.h>
#include <mutex>
#include <atomic>
#include <memory>

#include "core/Property.h"
#include "core/logging/Logger.h"
#include "properties/Configure.h"
#include "io/ClientSocket.h"
#include "io/BaseStream.h"
#include "utils/TimeUtil.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

static const char MAGIC_BYTES[] = { 'N', 'i', 'F', 'i' };

// Site2SitePeer Class
class Site2SitePeer : public org::apache::nifi::minifi::io::BaseStream {
 public:

  Site2SitePeer()
      : stream_(nullptr),
        host_(""),
        port_(-1) {

  }
  /*
   * Create a new site2site peer
   */
  explicit Site2SitePeer(
      std::unique_ptr<org::apache::nifi::minifi::io::DataStream> injected_socket,
      const std::string host_, uint16_t port_)
      : host_(host_),
        port_(port_),
        stream_(injected_socket.release()) {
    logger_ = logging::Logger::getLogger();
    _yieldExpiration = 0;
    _timeOut = 30000;  // 30 seconds
    _url = "nifi://" + host_ + ":" + std::to_string(port_);
  }

  explicit Site2SitePeer(Site2SitePeer &&ss)
      : stream_(ss.stream_.release()),
        host_(std::move(ss.host_)),
        port_(std::move(ss.port_)) {
    logger_ = logging::Logger::getLogger();
    _yieldExpiration.store(ss._yieldExpiration);
    _timeOut.store(ss._timeOut);
    _url = std::move(ss._url);
  }
  // Destructor
  virtual ~Site2SitePeer() {
    Close();
  }
  // Set Processor yield period in MilliSecond
  void setYieldPeriodMsec(uint64_t period) {
    _yieldPeriodMsec = period;
  }
  // get URL
  std::string getURL() {
    return _url;
  }
  // Get Processor yield period in MilliSecond
  uint64_t getYieldPeriodMsec(void) {
    return (_yieldPeriodMsec);
  }
  // Yield based on the yield period
  void yield() {
    _yieldExpiration = (getTimeMillis() + _yieldPeriodMsec);
  }
  // setHostName
  void setHostName(std::string host_) {
    this->host_ = host_;
    _url = "nifi://" + host_ + ":" + std::to_string(port_);
  }
  // setPort
  void setPort(uint16_t port_) {
    this->port_ = port_;
    _url = "nifi://" + host_ + ":" + std::to_string(port_);
  }
  // getHostName
  std::string getHostName() {
    return host_;
  }
  // getPort
  uint16_t getPort() {
    return port_;
  }
  // Yield based on the input time
  void yield(uint64_t time) {
    _yieldExpiration = (getTimeMillis() + time);
  }
  // whether need be to yield
  bool isYield() {
    if (_yieldExpiration > 0)
      return (_yieldExpiration >= getTimeMillis());
    else
      return false;
  }
  // clear yield expiration
  void clearYield() {
    _yieldExpiration = 0;
  }
  // Yield based on the yield period
  void yield(std::string portId) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t yieldExpiration = (getTimeMillis() + _yieldPeriodMsec);
    _yieldExpirationPortIdMap[portId] = yieldExpiration;
  }
  // Yield based on the input time
  void yield(std::string portId, uint64_t time) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t yieldExpiration = (getTimeMillis() + time);
    _yieldExpirationPortIdMap[portId] = yieldExpiration;
  }
  // whether need be to yield
  bool isYield(std::string portId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, uint64_t>::iterator it = this
        ->_yieldExpirationPortIdMap.find(portId);
    if (it != _yieldExpirationPortIdMap.end()) {
      uint64_t yieldExpiration = it->second;
      return (yieldExpiration >= getTimeMillis());
    } else {
      return false;
    }
  }
  // clear yield expiration
  void clearYield(std::string portId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, uint64_t>::iterator it = this
        ->_yieldExpirationPortIdMap.find(portId);
    if (it != _yieldExpirationPortIdMap.end()) {
      _yieldExpirationPortIdMap.erase(portId);
    }
  }
  // setTimeOut
  void setTimeOut(uint64_t time) {
    _timeOut = time;
  }
  // getTimeOut
  uint64_t getTimeOut() {
    return _timeOut;
  }
  int write(uint8_t value) {
    return Serializable::write(value, stream_.get());
  }
  int write(char value) {
    return Serializable::write(value, stream_.get());
  }
  int write(uint32_t value) {

    return Serializable::write(value, stream_.get());

  }
  int write(uint16_t value) {
    return Serializable::write(value, stream_.get());
  }
  int write(uint8_t *value, int len) {
    return Serializable::write(value, len, stream_.get());
  }
  int write(uint64_t value) {
    return Serializable::write(value, stream_.get());
  }
  int write(bool value) {
    uint8_t temp = value;
    return Serializable::write(temp, stream_.get());
  }
  int writeUTF(std::string str, bool widen = false) {
    return Serializable::writeUTF(str, stream_.get(), widen);
  }
  int read(uint8_t &value) {
    return Serializable::read(value, stream_.get());
  }
  int read(uint16_t &value) {
    return Serializable::read(value, stream_.get());
  }
  int read(char &value) {
    return Serializable::read(value, stream_.get());
  }
  int read(uint8_t *value, int len) {
    return Serializable::read(value, len, stream_.get());
  }
  int read(uint32_t &value) {
    return Serializable::read(value, stream_.get());
  }
  int read(uint64_t &value) {
    return Serializable::read(value, stream_.get());
  }
  int readUTF(std::string &str, bool widen = false) {
    return org::apache::nifi::minifi::io::Serializable::readUTF(str,
                                                                stream_.get(),
                                                                widen);
  }
  // open connection to the peer
  bool Open();
  // close connection to the peer
  void Close();

  /**
   * Move assignment operator.
   */
  Site2SitePeer& operator=(Site2SitePeer&& other) {
    stream_ = std::unique_ptr<org::apache::nifi::minifi::io::DataStream>(
        other.stream_.release());
    host_ = std::move(other.host_);
    port_ = std::move(other.port_);
    logger_ = logging::Logger::getLogger();
    _yieldExpiration = 0;
    _timeOut = 30000;  // 30 seconds
    _url = "nifi://" + host_ + ":" + std::to_string(port_);

    return *this;
  }

  Site2SitePeer(const Site2SitePeer &parent) = delete;
  Site2SitePeer &operator=(const Site2SitePeer &parent) = delete;

 protected:

 private:

  std::unique_ptr<org::apache::nifi::minifi::io::DataStream> stream_;

  std::string host_;
  uint16_t port_;

  // Mutex for protection
  std::mutex mutex_;
  // URL
  std::string _url;
  // socket timeout;
  std::atomic<uint64_t> _timeOut;
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  // Yield Period in Milliseconds
  std::atomic<uint64_t> _yieldPeriodMsec;
  // Yield Expiration
  std::atomic<uint64_t> _yieldExpiration;
  // Yield Expiration per destination PortID
  std::map<std::string, uint64_t> _yieldExpirationPortIdMap;
  // OpenSSL connection state
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer

};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
