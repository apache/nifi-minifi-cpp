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
#include "controllers/NetworkPrioritizerService.h"
#include <cstdio>
#include <utility>
#include <limits>
#include <string>
#include <vector>
#ifndef WIN32
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#endif
#include <cstring>
#include <cstdlib>

#include "utils/StringUtils.h"
#include "core/TypedValues.h"
#include "core/Resource.h"
#if ( defined(__APPLE__) || defined(__MACH__) || defined(BSD))
#include <net/if_dl.h>
#include <net/if_types.h>
#endif

namespace org::apache::nifi::minifi::controllers {

void NetworkPrioritizerService::initialize() {
  setSupportedProperties(Properties);
}

void NetworkPrioritizerService::yield() {
}

/**
 * If not an intersecting operation we will attempt to locate the highest priority interface available.
 */
io::NetworkInterface NetworkPrioritizerService::getInterface(uint32_t size = 0) {
  std::vector<std::string> controllers;
  std::string ifc;
  if (!network_controllers_.empty()) {
    if (sufficient_tokens(size) && size <= max_payload_) {
      controllers.insert(std::end(controllers), std::begin(network_controllers_), std::end(network_controllers_));
    }
  }

  if (!controllers.empty()) {
    ifc = get_nearest_interface(controllers);
    if (!ifc.empty()) {
      reduce_tokens(size);
      io::NetworkInterface newifc(ifc, sharedFromThis<NetworkPrioritizerService>());
      return newifc;
    }
  }
  for (const auto& service : linked_services_) {
    auto np = std::dynamic_pointer_cast<NetworkPrioritizerService>(service);
    if (np != nullptr) {
      auto ifcs = np->getInterfaces(size);
      ifc = get_nearest_interface(ifcs);
      if (!ifc.empty()) {
        np->reduce_tokens(size);
        io::NetworkInterface newifc(ifc, np);
        return newifc;
      }
    }
  }

  io::NetworkInterface newifc(ifc, nullptr);
  return newifc;
}

std::string NetworkPrioritizerService::get_nearest_interface(const std::vector<std::string> &ifcs) {
  for (auto ifc : ifcs) {
    if (!verify_interfaces_ || interface_online(ifc)) {
      logger_->log_debug("{} is online", ifc);
      return ifc;
    } else {
      logger_->log_debug("{} is not online", ifc);
    }
  }
  return "";
}

bool NetworkPrioritizerService::interface_online(const std::string &ifc) {
#ifndef WIN32
  struct ifreq ifr{};
  auto sockid = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  memset(&ifr, 0, sizeof(ifr));
  memcpy(ifr.ifr_name, ifc.data(), ifc.length());
  ifr.ifr_name[ifc.length()] = 0;
  if (ioctl(sockid, SIOCGIFFLAGS, &ifr) < 0) {
    logger_->log_trace("Could not use ioctl on {}", ifc);
    return false;
  }
  close(sockid);
  return (ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING);
#else
  return false;
#endif
}

std::vector<std::string> NetworkPrioritizerService::getInterfaces(uint32_t size = 0) {
  std::vector<std::string> interfaces;
  if (!network_controllers_.empty()) {
    if (sufficient_tokens(size) && size <= max_payload_) {
      return network_controllers_;
    }
  }
  return interfaces;
}

bool NetworkPrioritizerService::sufficient_tokens(uint32_t size) {
  std::lock_guard<std::mutex> lock(token_mutex_);
  auto ms = clock_->timeSinceEpoch().count();
  auto diff = ms - timestamp_;
  timestamp_ = ms;
  if (diff > 0) {
    tokens_ += gsl::narrow<uint32_t>(diff * tokens_per_ms);
  }
  if (bytes_per_token_ > 0 && size > 0) {
    return tokens_ * bytes_per_token_ >= size;
  }
  return true;
}

void NetworkPrioritizerService::reduce_tokens(uint32_t size) {
  std::lock_guard<std::mutex> lock(token_mutex_);
  if (bytes_per_token_ > 0 && size > 0) {
    uint32_t tokens = size / bytes_per_token_;
    tokens_ -= tokens;
  }
}

bool NetworkPrioritizerService::isRunning() const {
  return getState() == core::controller::ControllerServiceState::ENABLED;
}

bool NetworkPrioritizerService::isWorkAvailable() {
  return false;
}

void NetworkPrioritizerService::onEnable() {
  std::string controllers;
  if (getProperty(NetworkControllers, controllers) || !linked_services_.empty()) {
    // if this controller service is defined, it will be an intersection of this config with linked services.
    if (getProperty(MaxThroughput, max_throughput_)) {
      logger_->log_trace("Max throughput is {}", max_throughput_);
      if (max_throughput_ < 1000) {
        bytes_per_token_ = 1;
        tokens_ = gsl::narrow<uint32_t>(max_throughput_);
      } else {
        bytes_per_token_ = gsl::narrow<uint32_t>(max_throughput_ / 1000);
      }
    }

    getProperty(MaxPayload, max_payload_);

    if (!controllers.empty()) {
      network_controllers_ = utils::string::split(controllers, ",");
      for (const auto &ifc : network_controllers_) {
        logger_->log_trace("{} added to list of applied interfaces", ifc);
      }
    }
    bool is_default = false;
    if (getProperty(DefaultPrioritizer, is_default)) {
      if (is_default) {
        if (io::NetworkPrioritizerFactory::getInstance()->setPrioritizer(sharedFromThis<NetworkPrioritizerService>()) < 0) {
          throw std::runtime_error("Can only have one prioritizer");
        }
      }
    }
    getProperty(VerifyInterfaces, verify_interfaces_);
    timestamp_ = clock_->timeSinceEpoch().count();
    enabled_ = true;
    logger_->log_trace("Enabled");
  } else {
    logger_->log_trace("Could not enable ");
  }
}

REGISTER_RESOURCE(NetworkPrioritizerService, ControllerService);

}  // namespace org::apache::nifi::minifi::controllers
