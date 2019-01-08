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
#ifndef LIBMINIFI_INCLUDE_CSITETOSITE_CPEER_H_
#define LIBMINIFI_INCLUDE_CSITETOSITE_CPEER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <uuid/uuid.h>
#include "core/cstream.h"

#ifdef __cplusplus
extern "C" {
#endif

static const char MAGIC_BYTES[] = { 'N', 'i', 'F', 'i' };

struct SiteToSiteCPeer;

// open connection to the peer
int openPeer(struct SiteToSiteCPeer * peer);

// close connection to the peer
void closePeer(struct SiteToSiteCPeer * peer);

// Site2SitePeer Class
struct SiteToSiteCPeer {

  cstream * stream_;

  // URL
  char * url_;

  char * host_;

  uint16_t port_;

  enum Bool owns_socket_;

  //io::NetworkInterface local_network_interface_;

  // Logger
  //std::shared_ptr<logging::Logger> logger_;
};

static const char * getURL(const struct SiteToSiteCPeer * peer) {
  return peer->url_;
}

static void setHostName(struct SiteToSiteCPeer * peer, const char * hostname) {
  if(peer->host_) {
    free(peer->host_);
  }
  if(peer->url_) {
    free(peer->url_);
  }
  if(hostname == NULL || strlen(hostname) == 0) {
    peer->host_ = NULL;
    peer->url_ = NULL;
    return;
  }
  size_t host_len = strlen(hostname);
  peer->host_ = (char*)malloc(host_len + 1); // +1 for trailing zero
  peer->url_ = (char*)malloc(host_len + 14); // +1 for trailing zero, 1 for ':', at most 5 for port, 7 for "nifi://" suffix
  memset(peer->url_, 0, host_len + 14); // make sure to have zero padding no matter the length of the port
  strncpy(peer->host_, hostname, host_len);
  strncpy(peer->url_, "nifi://", 7);
  strncpy(peer->url_ + 7, hostname, host_len);
  peer->host_[host_len] = '\0';
  peer->url_[host_len + 7] = ':';
  if(peer->port_ != 0) {
    sprintf(peer->url_ + host_len + 8, "%d", peer->port_);
  }
  return;
}

static void setPort(struct SiteToSiteCPeer * peer, uint16_t port) {
  peer->port_ = port;
  if(peer->url_ != NULL) {
    int i;
    for(i = 8; i < strlen(peer->url_); ++i) { //8 to begin searching for ':' after the "nifi://" suffix
      if(peer->url_[i] == ':'){
        memset(peer->url_ + i + 1, 0, 6); // zero the port area  - the new port can be shorter
        sprintf(peer->url_ + i + 1, "%d", peer->port_);
      }
    }
  }
}

static void initPeer(struct SiteToSiteCPeer * peer, cstream * injected_socket, const char * host, uint16_t port, const char * ifc) {
  peer->stream_ = injected_socket;
  //peer->local_network_interface_= std::move(io::NetworkInterface(ifc, nullptr));
  peer->host_ = NULL;
  peer->url_ = NULL;
  peer->port_ = 0;
  setHostName(peer, host);
  setPort(peer, port);

  if(peer->stream_ == NULL) {
    peer->owns_socket_ = True;
  }
}

static void freePeer(struct SiteToSiteCPeer * peer) {
  closePeer(peer);
  setHostName(peer, NULL);

  if(peer->owns_socket_ == True && peer->stream_ != NULL) {
    free_socket(peer->stream_);
    peer->stream_ = NULL;
  }
}

#ifdef __cplusplus
}
#endif

#endif /* LIBMINIFI_INCLUDE_CSITETOSITE_CPEER_H_ */
