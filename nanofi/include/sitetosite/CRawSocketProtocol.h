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
#ifndef __CSITE2SITE_CLIENT_PROTOCOL_H__
#define __CSITE2SITE_CLIENT_PROTOCOL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <uuid/uuid.h>

#include "api/nanofi.h"
#include "CSiteToSite.h"
#include "CPeer.h"

#include "uthash.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DESCRIPTION_BUFFER_SIZE 2048

struct CRawSiteToSiteClient;

int readResponse(struct CRawSiteToSiteClient* client, RespondCode *code);

int writeResponse(struct CRawSiteToSiteClient* client, RespondCode code, const char * message);

int readRequestType(struct CRawSiteToSiteClient* client, RequestType *type);

int writeRequestType(struct CRawSiteToSiteClient* client, RequestType type);

void tearDown(struct CRawSiteToSiteClient* client);

int initiateResourceNegotiation(struct CRawSiteToSiteClient* client);

int initiateCodecResourceNegotiation(struct CRawSiteToSiteClient* client);

int negotiateCodec(struct CRawSiteToSiteClient* client);

int establish(struct CRawSiteToSiteClient* client);

void addTransaction(struct CRawSiteToSiteClient * client, CTransaction * transaction);

CTransaction * findTransaction(const struct CRawSiteToSiteClient * client, const char * id);

void deleteTransaction(struct CRawSiteToSiteClient * client, const char * id);

void clearTransactions(struct CRawSiteToSiteClient * client);

int handShake(struct CRawSiteToSiteClient * client);

int bootstrap(struct CRawSiteToSiteClient * client);

int complete(struct CRawSiteToSiteClient * client, const char * transactionID);

int confirm(struct CRawSiteToSiteClient * client, const char * transactionID);

int transmitPayload(struct CRawSiteToSiteClient * client, const char * payload, const attribute_set * attributes);

int16_t sendPacket(struct CRawSiteToSiteClient * client, const char * transactionID, CDataPacket *packet, flow_file_record * ff);

CTransaction* createTransaction(struct CRawSiteToSiteClient * client, TransferDirection direction);

static const char * getResourceName(const struct CRawSiteToSiteClient * c) {
  return "SocketFlowFileProtocol";
}

static const char * getCodecResourceName(const struct CRawSiteToSiteClient * c) {
  return "StandardFlowFileCodec";
}

static RespondCodeContext *getRespondCodeContext(RespondCode code) {
  unsigned int i;
  for ( i = 0; i < sizeof(respondCodeContext) / sizeof(RespondCodeContext); i++) {
    if (respondCodeContext[i].code == code) {
      return &respondCodeContext[i];
    }
  }
  return NULL;
}

// RawSiteToSiteClient Class
struct CRawSiteToSiteClient {
  // Batch Count
  uint64_t _batch_count;
  // Batch Size
  uint64_t _batch_size;
  // Batch Duration in msec
  uint64_t _batch_duration;
  // Timeout in msec
  uint64_t _timeout;

  // commsIdentifier
  char _commsIdentifier[37];

  // Peer State
  PeerState _peer_state;

  // portIDStr
  char _port_id_str[37];

  char _description_buffer[DESCRIPTION_BUFFER_SIZE]; //should be big enough

  // Peer Connection
  struct SiteToSiteCPeer* _peer;

  // Indicatates if _peer is owned by the client
  enum Bool _owns_resource;


  CTransaction * _known_transactions;

  // BATCH_SEND_NANOS
  uint64_t _batchSendNanos;

  /***
   * versioning
   */
  uint32_t _supportedVersion[5];
  uint32_t _currentVersion;
  int _currentVersionIndex;
  uint32_t _supportedCodecVersion[1];
  uint32_t _currentCodecVersion;
  int _currentCodecVersionIndex;
};

static const char * getPortId(const struct CRawSiteToSiteClient * client) {
  return client->_port_id_str;
}

static void setPortId(struct CRawSiteToSiteClient * client, const char * id) {
  strncpy(client->_port_id_str, id, 37);
  client->_port_id_str[36] = '\0';
  int i;
  for(i = 0; i < 37; i++){
    client->_port_id_str[i] = tolower(client->_port_id_str[i]);
  }
}

static void setBatchSize(struct CRawSiteToSiteClient *client, uint64_t size) {
  client->_batch_size = size;
}

static void setBatchCount(struct CRawSiteToSiteClient *client, uint64_t count) {
  client->_batch_count = count;
}

static void setBatchDuration(struct CRawSiteToSiteClient *client, uint64_t duration) {
  client->_batch_duration = duration;
}

static uint64_t getTimeOut(const struct CRawSiteToSiteClient *client) {
  return client->_timeout;
}

static void initRawClient(struct CRawSiteToSiteClient *client, struct SiteToSiteCPeer * peer) {
  client->_owns_resource = False;
  client->_peer = peer;
  client->_peer_state = IDLE;
  client->_batch_size = 0;
  client->_batch_count = 0;
  client->_batch_duration = 0;
  client->_batchSendNanos = 5000000000;  // 5 seconds
  client->_timeout = 30000;  // 30 seconds
  client->_supportedVersion[0] = 5;
  client->_supportedVersion[1] = 4;
  client->_supportedVersion[2] = 3;
  client->_supportedVersion[3] = 2;
  client->_supportedVersion[4] = 1;
  client->_currentVersion = client->_supportedVersion[0];
  client->_currentVersionIndex = 0;
  client->_supportedCodecVersion[0] = 1;
  client->_currentCodecVersion = client->_supportedCodecVersion[0];
  client->_currentCodecVersionIndex = 0;
  client->_known_transactions = NULL;
  memset(client->_description_buffer, 0, DESCRIPTION_BUFFER_SIZE);
}

static struct CRawSiteToSiteClient* createClient(const char * host, uint16_t port, const char * nifi_port) {
  struct SiteToSiteCPeer * peer = (struct SiteToSiteCPeer *)malloc(sizeof(struct SiteToSiteCPeer));
  initPeer(peer, NULL, host, port, "");
  struct CRawSiteToSiteClient* client = (struct CRawSiteToSiteClient*)malloc(sizeof(struct CRawSiteToSiteClient));
  initRawClient(client, peer);
  client->_owns_resource = True;
  setPortId(client, nifi_port);
  return client;
}

static void destroyClient(struct CRawSiteToSiteClient * client){
  tearDown(client);
  if(client->_owns_resource == True) {
    freePeer(client->_peer);
  }
}

#ifdef __cplusplus
}
#endif

#endif
