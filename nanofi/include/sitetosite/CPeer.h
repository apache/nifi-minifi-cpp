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

  cstream * _stream;

  // URL
  char * _url;

  char * _host;

  uint16_t _port;

  enum Bool _owns_resource;
};

static const char * getURL(const struct SiteToSiteCPeer * peer) {
  return peer->_url;
}

static void setHostName(struct SiteToSiteCPeer * peer, const char * hostname) {
  if(peer->_host) {
    free(peer->_host);
  }
  if(peer->_url) {
    free(peer->_url);
  }
  if(hostname == NULL || strlen(hostname) == 0) {
    peer->_host = NULL;
    peer->_url = NULL;
    return;
  }
  size_t host_len = strlen(hostname);
  peer->_host = (char*)malloc(host_len + 1); // +1 for trailing zero
  peer->_url = (char*)malloc(host_len + 14); // +1 for trailing zero, 1 for ':', at most 5 for port, 7 for "nifi://" suffix
  memset(peer->_url, 0, host_len + 14); // make sure to have zero padding no matter the length of the port
  strncpy(peer->_host, hostname, host_len);
  strncpy(peer->_url, "nifi://", 7);
  strncpy(peer->_url + 7, hostname, host_len);
  peer->_host[host_len] = '\0';
  peer->_url[host_len + 7] = ':';
  if(peer->_port != 0) {
    snprintf(peer->_url + host_len + 8, 5, "%d", peer->_port);
  }
  return;
}

static void setPort(struct SiteToSiteCPeer * peer, uint16_t port) {
  peer->_port = port;
  if(peer->_url != NULL) {
    int i;
    for(i = strlen(peer->_url) -1; i >= 0; --i) { // look for the last ':' in the string
      if(peer->_url[i] == ':'){
        memset(peer->_url + i + 1, 0, 6); // zero the port area  - the new port can be shorter
        snprintf(peer->_url + i + 1, 6, "%d", peer->_port);
        break;
      }
    }
  }
}

static void initPeer(struct SiteToSiteCPeer * peer, cstream * injected_socket, const char * host, uint16_t port, const char * ifc) {
  peer->_stream = injected_socket;
  //peer->local_network_interface_= std::move(io::NetworkInterface(ifc, nullptr));
  peer->_host = NULL;
  peer->_url = NULL;
  peer->_port = 0;
  setHostName(peer, host);
  setPort(peer, port);

  if(peer->_stream == NULL) {
    peer->_owns_resource = True;
  }
}

static void freePeer(struct SiteToSiteCPeer * peer) {
  closePeer(peer);
  setHostName(peer, NULL);

  if(peer->_owns_resource == True && peer->_stream != NULL) {
    free_socket(peer->_stream);
    peer->_stream = NULL;
  }
}

#ifdef __cplusplus
}
#endif

#endif /* LIBMINIFI_INCLUDE_CSITETOSITE_CPEER_H_ */
