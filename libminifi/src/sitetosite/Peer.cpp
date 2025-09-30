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
#include "sitetosite/Peer.h"

#include "io/validation.h"
#include "utils/net/AsioSocketUtils.h"

namespace org::apache::nifi::minifi::sitetosite {

bool SiteToSitePeer::open() {
  if (IsNullOrEmpty(host_)) {
    return false;
  }

  /**
   * We may override the interface provided to us within the socket in this step; however, this is a
   * known configuration path, and thus we will allow the RPG configuration to override anything provided to us
   * previously by the socket preference.
   */
  if (!local_network_interface_.getInterface().empty()) {
    if (auto socket = dynamic_cast<utils::net::AsioSocketConnection*>(stream_.get())) {
      socket->setInterface(local_network_interface_.getInterface());
    }
  }

  if (stream_->initialize() < 0) {
    return false;
  }

  return stream_->write(reinterpret_cast<const uint8_t *>(MAGIC_BYTES.data()), MAGIC_BYTES.size()) == MAGIC_BYTES.size();
}

void SiteToSitePeer::close() {
  if (stream_) {
    stream_->close();
  }
}

}  // namespace org::apache::nifi::minifi::sitetosite
