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
#include "coap_connection.h"

CoapPDU *create_connection(uint8_t type, const char * const server, const char * const endpoint, int port, const CoapMessage * const message) {
  CoapPDU *pdu = (CoapPDU*) malloc(sizeof(CoapPDU));

  pdu->ctx = NULL;
  pdu->session = NULL;

  coap_uri_t uri;
  uri.host.s = (uint8_t*) server;  // may be a loss in resolution, but hostnames should be char *
  uri.host.length = strlen(server);
  uri.path.s = (uint8_t*) endpoint;  // ^ same as above for paths.
  uri.path.length = strlen(endpoint);
  uri.port = port;
  uri.scheme = COAP_URI_SCHEME_COAP;

  fd_set readfds;
  coap_pdu_t* request;
  unsigned char get_method = 1;

  int res = resolve_address(&uri.host, &pdu->dst_addr.addr.sa);
  if (res < 0) {
    return NULL;
  }
  pdu->dst_addr.size = res;
  pdu->dst_addr.addr.sin.sin_port = htons(uri.port);

  void *addrptr = NULL;
  char port_str[NI_MAXSERV] = "0";

  switch (pdu->dst_addr.addr.sa.sa_family) {
    case AF_INET:
      addrptr = &pdu->dst_addr.addr.sin.sin_addr;
      if (!create_session(&pdu->ctx, &pdu->session, 0x00, port_str, &pdu->dst_addr)) {
        break;
      } else {
        return NULL;
      }
    case AF_INET6:
      addrptr = &pdu->dst_addr.addr.sin6.sin6_addr;
      if (!create_session(&pdu->ctx, &pdu->session, 0x00, port_str, &pdu->dst_addr)) {
        break;
      } else {
        return NULL;
      }
    default:
      ;
  }

  // we want to register handlers in the event that an error occurs or nack is returned
  // from the library
  coap_register_event_handler(pdu->ctx, coap_event);
  coap_register_nack_handler(pdu->ctx, no_acknowledgement);

  coap_context_set_keepalive(pdu->ctx, 1);

  coap_str_const_t pld;
  pld.length = message->size_;
  pld.s = message->data_;

  coap_register_option(pdu->ctx, COAP_OPTION_BLOCK2);

  // set the response handler
  coap_register_response_handler(pdu->ctx, response_handler);

  pdu->session->max_retransmit = 1;
  pdu->optlist = NULL;

  // add the URI option to the options list
  coap_insert_optlist(&pdu->optlist, coap_new_optlist(COAP_OPTION_URI_PATH, uri.path.length, uri.path.s));

  // next, create the PDU.
  if (!(request = create_request(pdu->ctx, pdu->session, &pdu->optlist, type, &pld)))
    return NULL;

  // send the PDU using the session.
  coap_send(pdu->session, request);
  return pdu;
}

int8_t send_pdu(const CoapPDU * const pdu) {
  uint64_t wait_ms = 1 * 200;
  // run once will attempt to send the first time
  int runResponse = coap_run_once(pdu->ctx, wait_ms);
  // if no data is received, we will attempt re-transmission
  // until the number of attempts has been reached.
  while (!coap_can_exit(pdu->ctx)) {
    runResponse = coap_run_once(pdu->ctx, wait_ms);
  }
  if (runResponse < 0)
    return -1;
  else
    return 0;
}

int8_t free_pdu(CoapPDU * pdu) {
  if (NULL == pdu) {
    return -1;
  }
  coap_delete_optlist(pdu->optlist);
  coap_session_release(pdu->session);
  coap_free_context(pdu->ctx);
  free(pdu);
  return 0;
}

