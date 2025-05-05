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
#include <array>

#include "core/logging/LoggerFactory.h"
#include "core/Property.h"
#include "io/BaseStream.h"
#include "io/BufferStream.h"
#include "properties/Configure.h"
#include "http/BaseHTTPClient.h"
#include "utils/TimeUtil.h"
#include "io/NetworkPrioritizer.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::sitetosite {

static constexpr std::array<char, 4> MAGIC_BYTES = { 'N', 'i', 'F', 'i' };

class PeerStatus {
 public:
  PeerStatus(utils::Identifier port_id, std::string host, uint16_t port, uint32_t flow_file_count, bool query_for_peers)
      : port_id_(port_id),
        host_(std::move(host)),
        port_(port),
        flow_file_count_(flow_file_count),
        query_for_peers_(query_for_peers) {
  }

  PeerStatus(const PeerStatus &other) = default;
  PeerStatus(PeerStatus &&other) = default;

  PeerStatus& operator=(const PeerStatus &other) = default;
  PeerStatus& operator=(PeerStatus &&other) = default;

  const utils::Identifier &getPortId() const {
    return port_id_;
  }

  const std::string &getHost() const {
    return host_;
  }

  uint16_t getPort() const {
    return port_;
  }

  uint32_t getFlowFileCount() const {
    return flow_file_count_;
  }

  bool getQueryForPeers() const {
    return query_for_peers_;
  }

 protected:
  utils::Identifier port_id_;
  std::string host_;
  uint16_t port_;
  uint32_t flow_file_count_;
  bool query_for_peers_;
};

class SiteToSitePeer : public org::apache::nifi::minifi::io::BaseStreamImpl {
 public:
  SiteToSitePeer(std::unique_ptr<org::apache::nifi::minifi::io::BaseStream> injected_socket, const std::string& host, uint16_t port, const std::string& ifc)
      : SiteToSitePeer(host, port, ifc) {
    stream_ = std::move(injected_socket);
  }

  SiteToSitePeer(const std::string &host, uint16_t port, const std::string &ifc)
      : host_(host),
        port_(port),
        url_("nifi://" + host_ + ":" + std::to_string(port_)),
        local_network_interface_(io::NetworkInterface(ifc, nullptr)) {
    timeout_.store(30s);
  }

  SiteToSitePeer(SiteToSitePeer &&ss) = delete;
  SiteToSitePeer& operator=(SiteToSitePeer&& other) = delete;
  SiteToSitePeer(const SiteToSitePeer &parent) = delete;
  SiteToSitePeer &operator=(const SiteToSitePeer &parent) = delete;

  ~SiteToSitePeer() override {
    close();
  }

  std::string getURL() const {
    return url_;
  }

  void setInterface(std::string &ifc) {
    local_network_interface_ = io::NetworkInterface(ifc, nullptr);
  }

  std::string getInterface() const {
    return local_network_interface_.getInterface();
  }

  void setHostName(const std::string& host) {
    host_ = host;
    url_ = "nifi://" + host_ + ":" + std::to_string(port_);
  }

  void setPort(uint16_t port) {
    port_ = port;
    url_ = "nifi://" + host_ + ":" + std::to_string(port_);
  }

  std::string getHostName() const {
    return host_;
  }

  uint16_t getPort() const {
    return port_;
  }

  void setTimeout(std::chrono::milliseconds time) {
    timeout_ = time;
  }

  std::chrono::milliseconds getTimeout() const {
    return timeout_.load();
  }

  void setHTTPProxy(const http::HTTPProxy &proxy) {
    proxy_ = proxy;
  }

  http::HTTPProxy getHTTPProxy() const {
    return proxy_;
  }

  void setStream(std::unique_ptr<org::apache::nifi::minifi::io::BaseStream> stream) {
    stream_ = nullptr;
    if (stream) {
      stream_ = std::move(stream);
    }
  }

  org::apache::nifi::minifi::io::BaseStream* getStream() const {
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

  bool open();
  void close() override;

 private:
  std::unique_ptr<org::apache::nifi::minifi::io::BaseStream> stream_;
  std::string host_;
  uint16_t port_;
  std::string url_;
  std::atomic<std::chrono::milliseconds> timeout_{30s};
  io::NetworkInterface local_network_interface_;
  http::HTTPProxy proxy_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<SiteToSitePeer>::getLogger();
};

}  // namespace org::apache::nifi::minifi::sitetosite
