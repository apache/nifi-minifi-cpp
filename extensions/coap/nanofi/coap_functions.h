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
#ifndef EXTENSIONS_COAP_NANOFI_COAP_FUNCTIONS_H_
#define EXTENSIONS_COAP_NANOFI_COAP_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif


typedef unsigned char method_t;

#include "coap2/coap.h"
#include "coap2/uri.h"
#include "coap2/address.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include "coap_message.h"


typedef struct {
  void (*data_received)(void *receiver_context, struct coap_context_t *ctx, CoapMessage *const);
  void (*received_error)(void *receiver_context, struct coap_context_t *ctx, unsigned int code);
} callback_pointers;

// defines the context specific data for the data receiver
static void *receiver;
static callback_pointers global_ptrs;

/**
 * Initialize the API access. Not thread safe.
 */
void init_coap_api(void *rcvr, callback_pointers *ptrs);

/**
 * Creates a CoAP session. Provide it double pointer to the context and session to instantiate those structs.
 * @param ctx coap context
 * @param session coap session
 * @param node device node name
 * @param dst_addr destination address
 * @return 0 if success -1 otherwise.
 */
int create_session(coap_context_t **ctx, coap_session_t **session, const char *node, const char *port, coap_address_t *dst_addr);

/**
 * Creates an endpoint context
 * @param ctx pointer to a context
 * @param node device node name
 * @param port device port
 */
int create_endpoint_context(coap_context_t **ctx, const char *node, const char *port);

/**
 * Creates a request object with an already allocated context and session. The option list is sent in as an arry ptr.
 * @param ctx coap context
 * @param session coap session
 * @param optlist option list array
 * @param code coap request code
 * @param ptr ptr to the coap payload
 * @returns pointer to a newly formed PDU ( protocol data unit )
 */
struct coap_pdu_t *create_request(struct coap_context_t *ctx, struct coap_session_t *session, coap_optlist_t **optlist, unsigned char code, coap_str_const_t *ptr);

/**
 * Function can be used to receive coap events
 * @param ctx context that performed event
 * @param event coap event launched
 * @param session session that performed event.
 * @return 0 as a success ( events in this library aren't noteworthy to PDU )
 */
int coap_event(struct coap_context_t *ctx, coap_event_t event, struct coap_session_t *session);

/**
 * Function can be used when a noack is received from the library
 * @param ctx context that performed event
 * @param session session that performed event.
 * @parma sent pdu that was sent
 * @param reason reason PDU was not acked
 * @param event coap event launched
 * @param id id of ack
 */
void no_acknowledgement(struct coap_context_t *ctx, coap_session_t *session, coap_pdu_t *sent, coap_nack_reason_t reason, const coap_tid_t id);

/**
 * Responser handler is the function launched when data is received
 * @param ctx context
 * @param session coap session
 * @param sent PDU that was sent
 * @param received PDU that was received
 * @param id id of ack
 */
void response_handler(struct coap_context_t *ctx, struct coap_session_t *session, coap_pdu_t *sent, coap_pdu_t *received, const coap_tid_t id);

/**
 * Resolves the destination address of the server and places that into dst
 * @param server server to connect to
 * @param dst destination pointer
 * @return 0 if sucess -1 otherwise
 */
int resolve_address(const struct coap_str_const_t *server, struct sockaddr *destination);

#ifdef __cplusplus
}
#endif

#endif /* EXTENSIONS_COAP_NANOFI_COAP_FUNCTIONS_H_ */

