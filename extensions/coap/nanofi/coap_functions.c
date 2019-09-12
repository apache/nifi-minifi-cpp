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
  //coap_set_log_level(LOG_DEBUG);
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

#ifdef WIN32
  WSADATA wsadata;
  int err = WSAStartup(MAKEWORD(2, 2), &wsadata);
  if (err != 0) {
      return -1;
  }
#endif

  int getaddrres = getaddrinfo(node, port, &hints, &result);
  if (getaddrres != 0) {
      perror("getaddrinfo");
#ifdef WIN32
      WSACleanup();
#endif
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
#ifdef WIN32
        WSACleanup();
#endif
        return 0;
      }
    }
  }

  freeaddrinfo(result);
#ifdef WIN32
  WSACleanup();
#endif
  return -2;
}

struct coap_pdu_t * create_coap_pdu(struct coap_session_t *session, unsigned char code, coap_optlist_t ** optlist) {
    coap_pdu_t *pdu;

    if (!(pdu = coap_new_pdu(session)))
      return NULL;

    pdu->type = COAP_MESSAGE_CON;
    pdu->tid = coap_new_message_id(session);
    pdu->code = code;
    if (optlist) {
      coap_add_optlist_pdu(pdu, optlist);
    }
    return pdu;
}

void create_blockwise_request(struct coap_pdu_t * pdu, struct coap_context_t * ctx, struct coap_session_t *session, coap_str_const_t *ptr, unsigned int block_num, int szx) {
    if (!ctx->app) {
        coap_set_app_data(ctx, (void *)coap_new_str_const(ptr->s, ptr->length));
    }

    //determine the encode option sizes for COAP_OPTION_BLOCK1 and COAP_OPTION_SIZE1
    //We can safely assume .szx = 6 to find the encode size of block1 option
    //The only variable part is the block number bits in the block1 option.
    //We do not set the block number to some assumed value. the block number
    //will be calculated dynamically in the process of blockwise transfer request
    //response transactions

    coap_block_t block1 = {.m = 1, .num = block_num, .szx = 6};
    unsigned char blk_opt[4];
    unsigned char sz1_opt[4];
    unsigned int sz1_opt_len = coap_encode_var_safe(sz1_opt, sizeof(sz1_opt), ptr->length);
    if (szx < 0) {
        unsigned int blk_opt_len = coap_encode_var_safe(blk_opt, sizeof(blk_opt),
                                                         (block1.num << 4)|(block1.m << 3)|block1.szx);
        size_t blk_encode_size = coap_opt_encode_size(COAP_OPTION_BLOCK1 - pdu->max_delta, blk_opt_len);
        size_t sz1_encode_size = coap_opt_encode_size(COAP_OPTION_SIZE1 - COAP_OPTION_BLOCK1, sz1_opt_len);
        size_t avail = pdu->max_size - pdu->used_size - 4 - blk_encode_size - sz1_encode_size;
        szx = coap_flsll((long long)avail) - 5;
    }
    block1.szx = szx;
    block1.m = ((block1.num + 1) << (block1.szx + 4)) < ptr->length;
    coap_add_option(pdu,
                    COAP_OPTION_BLOCK1,
                    coap_encode_var_safe(blk_opt, sizeof(blk_opt),
                            (block1.num << 4) | (block1.m << 3) | block1.szx),
                    blk_opt);
    coap_add_option(pdu, COAP_OPTION_SIZE1, sz1_opt_len, sz1_opt);
    coap_add_block(pdu, ptr->length, ptr->s, block1.num, block1.szx);
}

struct coap_pdu_t *create_request(struct coap_context_t *ctx, struct coap_session_t *session, coap_optlist_t **optlist, unsigned char code, coap_str_const_t *ptr) {
  coap_pdu_t *pdu = create_coap_pdu(session, code, optlist);
  if (!pdu) {
      return NULL;
  }
  if (ptr->length > (pdu->max_size - pdu->used_size - 4)) {
      create_blockwise_request(pdu, ctx, session, ptr,  0, -1);
      return pdu;
  }
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

void get_pdu_optlist(coap_pdu_t * pdu, coap_optlist_t ** optlist) {
    coap_opt_t *option;
    coap_opt_iterator_t opt_iter;
    coap_option_iterator_init(pdu, &opt_iter, COAP_OPT_ALL);
    while ((option = coap_option_next(&opt_iter))) {
        //while iterating create a coap_optlist_t
        if (opt_iter.type != COAP_OPTION_BLOCK1 && opt_iter.type != COAP_OPTION_SIZE1)
            coap_insert_optlist(optlist, coap_new_optlist(opt_iter.type, coap_opt_length(option), coap_opt_value(option)));
    }
}

void handle_block_response(coap_opt_t * block_opt, struct coap_context_t *ctx, struct coap_session_t *session, coap_pdu_t *sent) {
    unsigned int szx = COAP_OPT_BLOCK_SZX(block_opt);
    unsigned int num = coap_opt_block_num(block_opt);
    coap_block_t block;
    block.num = num;
    block.szx = szx;
    //We are handling a response to POST request. Server never uses BLOCK1 option in the response
    //unless client initiated BLOCK1 blockwise transfer request. Therefore, it is safe to consider
    //that the recently sent pdu within this session has the BLOCK1 option
    coap_opt_iterator_t opt_iter;
    block_opt = coap_check_option(sent, COAP_OPTION_BLOCK1, &opt_iter);
    if (szx != COAP_OPT_BLOCK_SZX(block_opt)) {
        block.num = coap_opt_block_num(block_opt);
        block.szx = COAP_OPT_BLOCK_SZX(block_opt);
        //Server negotiated a different block size
        //client will follow the server, but we have to adjust block number
        unsigned int bytes_sent = ((block.num + 1) << (block.szx + 4));
        if (bytes_sent % (1 << (szx + 4)) == 0) {
            num = block.num = (bytes_sent >> (szx + 4)) - 1;
            block.szx = szx;
            coap_log(LOG_DEBUG,
                    "new Block1 size is %u, block number %u completed\n",
                    (1 << (block.szx + 4)), block.num);
        } else {
            coap_log(LOG_DEBUG, "ignoring request to increase Block1 size, "
                    "next block is not aligned on requested block size boundary. "
                    "(%u x %u mod %u = %u != 0)\n",
                    block.num + 1, (1 << (block.szx + 4)), (1 << (szx + 4)),
                    bytes_sent % (1 << (szx + 4)));
        }
    }

    coap_str_const_t * payload = (coap_str_const_t *)coap_get_app_data(ctx);
    if (payload->length <= (block.num + 1) * (1 << (block.szx + 4))) {
        coap_log(LOG_DEBUG, "blockwise transfer completed\n");
        return;
    }

    coap_optlist_t * optlist = NULL;
    get_pdu_optlist(sent, &optlist);
    coap_pdu_t * pdu = create_coap_pdu(session, sent->code, &optlist);
    coap_delete_optlist(optlist);
    create_blockwise_request(pdu, ctx, session, payload, block.num + 1, block.szx);
    coap_send(session, pdu);
}

void response_handler(struct coap_context_t *ctx, struct coap_session_t *session, coap_pdu_t *sent, coap_pdu_t *received, const coap_tid_t id) {

    if (COAP_RESPONSE_CLASS(received->code) == 2 || received->code == COAP_RESPONSE_400) {
        coap_opt_iterator_t opt_iter;
        coap_opt_t * block_opt = coap_check_option(received, COAP_OPTION_BLOCK1, &opt_iter);

        if (block_opt) {
            handle_block_response(block_opt, ctx, session, sent);
        } else {
            if (!global_ptrs.data_received) return;
            CoapMessage * const msg = create_coap_message(received);
            global_ptrs.data_received(receiver, ctx, msg);
        }
    } else {
        if (global_ptrs.received_error)
            global_ptrs.received_error(receiver, ctx, received->code);
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

void free_app_data(struct coap_context_t * ctx) {
    if (ctx) {
        coap_str_const_t * s = (coap_str_const_t *)coap_get_app_data(ctx);
        coap_delete_str_const(s);
    }
}

#ifdef __cplusplus
}
#endif

