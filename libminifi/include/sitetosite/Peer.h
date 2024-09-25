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
#pragma once

#include <errno.h>
#include <stdio.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "core/logging/LoggerFactory.h"
#include "core/Property.h"
#include "io/BaseStream.h"
#include "io/BufferStream.h"
#include "properties/Configure.h"
#include "http/BaseHTTPClient.h"
#include "utils/TimeUtil.h"
#include "io/NetworkPrioritizer.h"

namespace org::apache::nifi::minifi::sitetosite {

class Peer {
 public:
  explicit Peer(const utils::Identifier &port_id, const std::string &host, uint16_t port, bool secure = false)
      : host_(host),
        port_(port),
        secure_(secure) {
    port_id_ = port_id;
  }

  explicit Peer(const std::string &host, uint16_t port, bool secure = false)
      : host_(host),
        port_(port),
        secure_(secure) {
  }

  explicit Peer(const Peer &other)
      : host_(other.host_),
        port_(other.port_),
        secure_(other.secure_) {
    port_id_ = other.port_id_;
  }

  explicit Peer(Peer &&other)
      : host_(std::move(other.host_)),
        port_(std::move(other.port_)),
        secure_(std::move(other.secure_)) {
    port_id_ = other.port_id_;
  }

  uint16_t getPort() const {
    return port_;
  }

  const std::string &getHost() const {
    return host_;
  }

  bool isSecure() const {
    return secure_;
  }

  utils::Identifier getPortId() const {
    return port_id_;
  }

 protected:
  std::string host_;

  uint16_t port_;

  utils::Identifier port_id_;

  // secore comms

  bool secure_;
};

class PeerStatus {
 public:
  PeerStatus(const std::shared_ptr<Peer> &peer, uint32_t flow_file_count, bool query_for_peers)
      : peer_(peer),
        flow_file_count_(flow_file_count),
        query_for_peers_(query_for_peers) {
  }

  PeerStatus(const PeerStatus &other) = default;
  PeerStatus(PeerStatus &&other) = default;

  PeerStatus& operator=(const PeerStatus &other) = default;
  PeerStatus& operator=(PeerStatus &&other) = default;

  const std::shared_ptr<Peer> &getPeer() const {
    return peer_;
  }

  uint32_t getFlowFileCount() {
    return flow_file_count_;
  }

  bool getQueryForPeers() {
    return query_for_peers_;
  }

 protected:
  std::shared_ptr<Peer> peer_;
  uint32_t flow_file_count_;
  bool query_for_peers_;
};

static const char MAGIC_BYTES[] = { 'N', 'i', 'F', 'i' };

// Site2SitePeer Class
class SiteToSitePeer : public org::apache::nifi::minifi::io::BaseStreamImpl {
 public:
  SiteToSitePeer()
      : stream_(nullptr),
        host_(""),
        port_(-1) {
  }
  /*
   * Create a new site2site peer
   */
  explicit SiteToSitePeer(std::unique_ptr<org::apache::nifi::minifi::io::BaseStream> injected_socket,
                          const std::string host,
                          uint16_t port,
                          const std::string &ifc)
      : SiteToSitePeer(host, port, ifc) {
    stream_ = std::move(injected_socket);
  }

  explicit SiteToSitePeer(const std::string &host, uint16_t port, const std::string &ifc)
      : stream_(nullptr),
        host_(host),
        port_(port),
        timeout_(std::chrono::seconds(30)),
        logger_(core::logging::LoggerFactory<SiteToSitePeer>::getLogger()) {
    url_ = "nifi://" + host_ + ":" + std::to_string(port_);
    yield_expiration_ = std::chrono::system_clock::time_point();
    timeout_ = std::chrono::seconds(30);
    local_network_interface_ = io::NetworkInterface(ifc, nullptr);
  }

  explicit SiteToSitePeer(SiteToSitePeer &&ss)
      : stream_(ss.stream_.release()),
        host_(std::move(ss.host_)),
        port_(std::move(ss.port_)),
        local_network_interface_(std::move(ss.local_network_interface_)),
        proxy_(std::move(ss.proxy_)),
        logger_(std::move(ss.logger_)) {
    yield_expiration_.store(ss.yield_expiration_);
    timeout_.store(ss.timeout_);
    url_ = std::move(ss.url_);
  }
  // Destructor
  ~SiteToSitePeer() {
    Close();
  }
  // Set Processor yield period in MilliSecond
  void setYieldPeriodMsec(std::chrono::milliseconds period) {
    yield_period_msec_ = period;
  }
  // get URL
  std::string getURL() {
    return url_;
  }
  // setInterface
  void setInterface(std::string &ifc) {
    local_network_interface_ = io::NetworkInterface(ifc, nullptr);
  }
  std::string getInterface() {
    return local_network_interface_.getInterface();
  }
  // Get Processor yield period in MilliSecond
  std::chrono::milliseconds getYieldPeriodMsec(void) {
    return (yield_period_msec_);
  }
  // Yield based on the yield period
  void yield() {
    yield_expiration_ = std::chrono::system_clock::now() + yield_period_msec_.load();
  }
  // setHostName
  void setHostName(std::string host_) {
    this->host_ = host_;
    url_ = "nifi://" + host_ + ":" + std::to_string(port_);
  }
  // setPort
  void setPort(uint16_t port_) {
    this->port_ = port_;
    url_ = "nifi://" + host_ + ":" + std::to_string(port_);
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
  void yield(std::chrono::milliseconds time) {
    yield_expiration_ = (std::chrono::system_clock::now() + time);
  }
  // whether need be to yield
  bool isYield() {
    return yield_expiration_.load() >= std::chrono::system_clock::now();
  }
  // clear yield expiration
  void clearYield() {
    yield_expiration_ = std::chrono::system_clock::time_point();
  }
  // Yield based on the yield period
  void yield(std::string portId) {
    std::lock_guard<std::mutex> lock(mutex_);
    yield_expiration_PortIdMap[portId] = std::chrono::system_clock::now() + yield_period_msec_.load();
  }
  // Yield based on the input time
  void yield(std::string portId, std::chrono::milliseconds time) {
    yield_expiration_PortIdMap[portId] = std::chrono::system_clock::now() + time;
  }
  // whether need be to yield
  bool isYield(std::string portId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = this->yield_expiration_PortIdMap.find(portId);
    if (it != yield_expiration_PortIdMap.end()) {
      auto yieldExpiration = it->second;
      return (yieldExpiration >= std::chrono::system_clock::now());
    } else {
      return false;
    }
  }
  // clear yield expiration
  void clearYield(std::string portId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = this->yield_expiration_PortIdMap.find(portId);
    if (it != yield_expiration_PortIdMap.end()) {
      yield_expiration_PortIdMap.erase(portId);
    }
  }
  // setTimeout
  void setTimeout(std::chrono::milliseconds time) {
    timeout_ = time;
  }
  // getTimeout
  std::chrono::milliseconds getTimeout() {
    return timeout_;
  }
  void setHTTPProxy(const http::HTTPProxy &proxy) {
    this->proxy_ = proxy;
  }
  http::HTTPProxy getHTTPProxy() {
    return this->proxy_;
  }

  void setStream(std::unique_ptr<org::apache::nifi::minifi::io::BaseStream> stream) {
    stream_ = nullptr;
    if (stream)
      stream_ = std::move(stream);
  }

  org::apache::nifi::minifi::io::BaseStream *getStream() {
    return stream_.get();
  }

  using BaseStream::write;
  using BaseStream::read;

  size_t write(const uint8_t* data, size_t len) override {
    return stream_->write(data, len);
  }

  size_t read(std::span<std::byte> data) override {
    return stream_->read(data);
  }

  // open connection to the peer
  bool Open();
  // close connection to the peer
  void Close();

  /**
   * Move assignment operator.
   */
  SiteToSitePeer& operator=(SiteToSitePeer&& other) {
    if (this == &other) {
      return *this;
    }
    stream_ = std::move(other.stream_);
    host_ = std::move(other.host_);
    port_ = std::move(other.port_);
    local_network_interface_ = std::move(other.local_network_interface_);
    yield_expiration_ = std::chrono::system_clock::time_point();
    timeout_ = std::chrono::seconds(30);
    url_ = "nifi://" + host_ + ":" + std::to_string(port_);

    return *this;
  }

  SiteToSitePeer(const SiteToSitePeer &parent) = delete;
  SiteToSitePeer &operator=(const SiteToSitePeer &parent) = delete;

 private:
  std::unique_ptr<org::apache::nifi::minifi::io::BaseStream> stream_;

  std::string host_;

  uint16_t port_;

  io::NetworkInterface local_network_interface_;

  http::HTTPProxy proxy_;

  // Mutex for protection
  std::mutex mutex_;
  // URL
  std::string url_;
  // socket timeout;
  std::atomic<std::chrono::milliseconds> timeout_{};
  // Yield Period in Milliseconds
  std::atomic<std::chrono::milliseconds> yield_period_msec_;
  // Yield Expiration
  std::atomic<std::chrono::time_point<std::chrono::system_clock>> yield_expiration_{};
  // Yield Expiration per destination PortID
  std::map<std::string, std::chrono::time_point<std::chrono::system_clock>> yield_expiration_PortIdMap;
  // Logger
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<SiteToSitePeer>::getLogger();
};

}  // namespace org::apache::nifi::minifi::sitetosite
