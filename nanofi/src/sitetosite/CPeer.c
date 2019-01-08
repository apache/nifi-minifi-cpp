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
#include <string.h>
#include "sitetosite/CPeer.h"


int openPeer(struct SiteToSiteCPeer * peer) {
  if (peer->host_ == NULL || strlen(peer->host_) == 0) {
    return -1;
  }

  //In case there was no socket injected, let's create it
  if(peer->stream_ == NULL && peer->owns_socket_ == True) {
    peer->stream_ = create_socket(peer->host_, peer->port_);
    if(peer->stream_ == NULL) {
      return -1;
    }
  }

  /**
   * We may override the interface provided to us within the socket in this step; however, this is a
   * known configuration path, and thus we will allow the RPG configuration to override anything provided to us
   * previously by the socket preference.
   */

  /*if (!peer->local_network_interface_.getInterface().empty()) {
    auto socket = static_cast<io::Socket*>(stream_.get());
    if (nullptr != socket) {
      socket->setInterface(io::NetworkInterface(peer->local_network_interface_.getInterface(), nullptr));
    }
  } */

  if(open_stream(peer->stream_) != 0) {
    return -1;
  }

  uint16_t data_size = sizeof MAGIC_BYTES;

  if(write_buffer((uint8_t*)MAGIC_BYTES, data_size, peer->stream_) != data_size) {
    return -1;
  }

  return 0;
}

void closePeer(struct SiteToSiteCPeer * peer) {
  close_stream(peer->stream_);
}
