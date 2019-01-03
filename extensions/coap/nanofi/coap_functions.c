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
#include "coap_functions.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the API access. Not thread safe.
 */
void init_coap_api(void *rcvr, callback_pointers *ptrs) {
  global_ptrs.data_received = ptrs->data_received;
  global_ptrs.received_error = ptrs->received_error;
  receiver = rcvr;
}

int create_session(coap_context_t **ctx, coap_session_t **session, const char *node, const char *port, coap_address_t *dst_addr) {
  int getaddrres;
  struct addrinfo hints;
  coap_proto_t proto = COAP_PROTO_UDP;
  struct addrinfo *result, *interface_itr;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;  // ipv4 or ipv6
  hints.ai_socktype = COAP_PROTO_RELIABLE(proto) ? SOCK_STREAM : SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST | AI_NUMERICSERV | AI_ALL;

  if (node) {
    getaddrres = getaddrinfo(node, port, &hints, &result);
    if (getaddrres != 0) {
      perror("getaddrinfo");
      return -1;
    }
    int skip = 1, count = 0;
    for (interface_itr = result; interface_itr != NULL; interface_itr = interface_itr->ai_next) {
      coap_address_t addr;

      if (interface_itr->ai_addrlen <= sizeof(addr.addr)) {
        coap_address_init(&addr);
        addr.size = interface_itr->ai_addrlen;
        memcpy(&addr.addr, interface_itr->ai_addr, interface_itr->ai_addrlen);

        *ctx = coap_new_context(0x00);

        *session = coap_new_client_session(*ctx, &addr, dst_addr, proto);
        if (*ctx && *session) {
          freeaddrinfo(result);
          return 0;
        }
      }
    }
    freeaddrinfo(result);
    return -2;
  } else {
    *ctx = coap_new_context(0x00);

    *session = coap_new_client_session(*ctx, 0x00, dst_addr, proto);
    return 0;
  }

}

int create_endpoint_context(coap_context_t **ctx, const char *node, const char *port) {
  struct addrinfo hints;
  coap_proto_t proto = COAP_PROTO_UDP;
  struct addrinfo *result, *interface_itr;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;  // ipv4 or ipv6
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
  int getaddrres = getaddrinfo(node, port, &hints, &result);
  if (getaddrres != 0) {
    perror("getaddrinfo");
    return -1;
  }

  for (interface_itr = result; interface_itr != NULL; interface_itr = interface_itr->ai_next) {
    coap_address_t addr;

    if (interface_itr->ai_addrlen <= sizeof(addr.addr)) {
      coap_address_init(&addr);
      addr.size = interface_itr->ai_addrlen;
      memcpy(&addr.addr, interface_itr->ai_addr, interface_itr->ai_addrlen);

      *ctx = coap_new_context(0x00);

      coap_endpoint_t * ep_udp = coap_new_endpoint(*ctx, &addr, COAP_PROTO_UDP);

      if (*ctx && ep_udp) {
        freeaddrinfo(result);
        return 0;
      }
    }
  }

  freeaddrinfo(result);
  return -2;
}

struct coap_pdu_t *create_request(struct coap_context_t *ctx, struct coap_session_t *session, coap_optlist_t **optlist, unsigned char code, coap_str_const_t *ptr) {
  coap_pdu_t *pdu;

  if (!(pdu = coap_new_pdu(session)))
    return NULL;

  pdu->type = COAP_MESSAGE_CON;
  pdu->tid = coap_new_message_id(session);
  pdu->code = code;

  if (optlist) {
    coap_add_optlist_pdu(pdu, optlist);
  }

  int flags = 0;
  coap_add_data(pdu, ptr->length, ptr->s);
  return pdu;
}

int coap_event(struct coap_context_t *ctx, coap_event_t event, struct coap_session_t *session) {
  if (event == COAP_EVENT_SESSION_FAILED && global_ptrs.received_error) {
    global_ptrs.received_error(receiver, ctx, -1);
  }
  return 0;
}

void no_acknowledgement(struct coap_context_t *ctx, coap_session_t *session, coap_pdu_t *sent, coap_nack_reason_t reason, const coap_tid_t id) {
  if (global_ptrs.received_error) {
    global_ptrs.received_error(receiver, ctx, -1);
  }
}

void response_handler(struct coap_context_t *ctx, struct coap_session_t *session, coap_pdu_t *sent, coap_pdu_t *received, const coap_tid_t id) {
  unsigned char* data;
  size_t data_len;
  coap_opt_iterator_t opt_iter;
  coap_opt_t * block_opt = coap_check_option(received, COAP_OPTION_BLOCK1, &opt_iter);
  if (block_opt) {
    printf("Block option not currently supported");
  } else {
    if (!global_ptrs.data_received) {
      return;
    }

    if (COAP_RESPONSE_CLASS(received->code) == 2 || received->code == COAP_RESPONSE_400) {
      if (global_ptrs.data_received) {
        CoapMessage * const msg = create_coap_message(received);
        global_ptrs.data_received(receiver, ctx, msg);
      }
    } else {
      if (global_ptrs.received_error)
        global_ptrs.received_error(receiver, ctx, received->code);
    }
  }

}

int resolve_address(const struct coap_str_const_t *server, struct sockaddr *destination) {
  struct addrinfo *result, *iterative_obj;
  struct addrinfo hints;
  static char addrstr[256];
  int error, len = -1;

  memset(addrstr, 0, sizeof(addrstr));
  if (server->length)
    memcpy(addrstr, server->s, server->length);
  else
    memcpy(addrstr, "127.0.0.1", 9);

  memset((char *) &hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_family = AF_UNSPEC;

  error = getaddrinfo(addrstr, NULL, &hints, &result);

  if (error != 0) {
    perror("getaddrinfo");

    return error;
  }

  for (iterative_obj = result; iterative_obj != NULL; iterative_obj = iterative_obj->ai_next) {
    switch (iterative_obj->ai_family) {
      case AF_INET6:
      case AF_INET:
        len = iterative_obj->ai_addrlen;
        memcpy(destination, iterative_obj->ai_addr, len);
        freeaddrinfo(result);
        return len;
      default:
        ;
    }
  }

  freeaddrinfo(result);
  return len;
}

#ifdef __cplusplus
}
#endif

