/**
 * @file Site2SitePeer.cpp
 * Site2SitePeer class implementation
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
#include <stdio.h>
#include <chrono>
#include <thread>
#include <random>
#include <memory>
#include <iostream>

#include "sitetosite/Peer.h"
#include "io/ClientSocket.h"
#include "io/validation.h"
#include "FlowController.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sitetosite {

bool SiteToSitePeer::Open() {
  if (IsNullOrEmpty(host_))
    return false;

  /**
   * We may override the interface provided to us within the socket in this step; however, this is a
   * known configuration path, and thus we will allow the RPG configuration to override anything provided to us
   * previously by the socket preference.
   */
  if (!this->local_network_interface_.getInterface().empty()) {
    auto socket = static_cast<io::Socket*>(stream_.get());
    if (nullptr != socket) {
      socket->setInterface(io::NetworkInterface(local_network_interface_.getInterface(), nullptr));
    }
  }

  if (stream_->initialize() < 0)
    return false;

  uint16_t data_size = sizeof MAGIC_BYTES;

  if (stream_->writeData(reinterpret_cast<uint8_t *>(const_cast<char*>(MAGIC_BYTES)), data_size) != data_size) {
    return false;
  }

  return true;
}

void SiteToSitePeer::Close() {
  if (stream_ != nullptr)
    stream_->closeStream();
}

} /* namespace sitetosite */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
