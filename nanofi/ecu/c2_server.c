/*
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

/**
 * The flow of C2 operations looks as follows
 * |***********|                |**************| c2 operations  |****************|
 * |           | c2 operations  |              |--------------->|                |
 * | c2 client |--------------->| c2 server    |<---------------| c2 agent / ecu |
 * |           |                |              |  c2 heartbeat  |                |
 * *************                ****************                ******************
 *
 *
 * This is meant to be used as a c2 server to listen
 * for heartbeats from c2 agents.
 * It keeps track of c2 agents by the agent's uuid
 * and when c2 command is received from a c2 client,
 * forwards that command to the corresponding c2 agent
 * in response to the heartbeat
 *
 * This is in no way meant to be used in production.
 * This is just a skeleton/dummy c2 server used to
 * test c2 agents functionality.
 */

#include <nanofi/coap_server.h>
#include <coap/c2protocol.h>
#include <coap/c2structs.h>
#include <coap/c2payload.h>
#include <coap/c2agent.h>
#include <c2_api/c2api.h>
#include "api/ecu.h"
#include "utlist.h"

#include <signal.h>


volatile sig_atomic_t stop_c2 = 0;

void c2_signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        stop_c2 = 1;
    }
}

void setup_c2_signal_action() {
#ifdef _WIN32
    signal(SIGINT, c2_signal_handler);
    signal(SIGTERM, c2_signal_handler);
#else
    struct sigaction action;
    memset(&action, 0, sizeof(sigaction));
    action.sa_handler = c2_signal_handler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
#endif
}

typedef struct c2_server_responses {
    char uuid[37]; //key
    c2_server_response_t * response;
    UT_hash_handle hh;
} c2_server_responses_t;

typedef struct agents {
    char uuid[37];
    struct agents * next;
} agents_t;

int little_endian = 0;
c2_server_responses_t * responses = NULL;
agents_t * ags = NULL;

int find_agent(char * uuid_str) {
    agents_t * el;
    LL_FOREACH(ags, el) {
        if (memcmp(el->uuid, uuid_str, strlen(uuid_str)) == 0) {
            return 1;
        }
    }
    el = (agents_t *)malloc(sizeof(agents_t));
    strcpy(el->uuid, uuid_str);
    LL_APPEND(ags, el);
    return 0;
}

void free_agents(agents_t * ags) {
    agents_t *el, *tmp;
    LL_FOREACH_SAFE(ags, el, tmp) {
        free(el);
    }
}

void handle_c2_post_request(coap_context_t * ctx, struct coap_resource_t * resource, coap_session_t * session, coap_pdu_t * pdu,
        coap_binary_t * token, coap_string_t * query, coap_pdu_t * response) {
    //This is a handle for accumulating c2responses from a c2 client.
    //The accumulated responses are then sent out as part of heartbeat response from c2 agents
    //This simulates sending c2 operations to c2 specific agents
    uint8_t * data;
    size_t len;
    coap_get_data(pdu, &len, &data);
    coap_message msg;
    msg.data = data;
    msg.length = len;

    c2_server_response_t * c2_serv_resp = NULL;
    if (len > 0) {
        c2_serv_resp = decode_c2_server_response(&msg, little_endian);
    }
    if (c2_serv_resp) {
        const char * uuid = c2_serv_resp->ident;
        c2_server_responses_t * el = NULL;
        HASH_FIND_STR(responses, uuid, el);
        if (el)  {
            LL_APPEND(el->response, c2_serv_resp);
        }
        else {
            el = (struct c2_server_responses *) malloc(sizeof(struct c2_server_responses));
            memset(el, 0, sizeof(struct c2_server_responses));
            strcpy(el->uuid, uuid);
            LL_APPEND(el->response, c2_serv_resp);
            HASH_ADD_STR(responses, uuid, el);
        }
    }
}

void handle_acknowledge_post_request(coap_context_t * ctx, struct coap_resource_t * resource, coap_session_t * session, coap_pdu_t * pdu,
                                   coap_binary_t * token, coap_string_t * query, coap_pdu_t * response) {
    printf("acknowledge received from client\n");
}

typedef struct payload {
    char * data;
    size_t length;
} payload_t;

void free_session_app_data(coap_session_t * session) {
    coap_string_t * tmp = (coap_string_t *)(session->app);
    coap_delete_string(tmp);
    session->app = NULL;
}

void handle_heartbeat_post_request(coap_context_t * ctx, struct coap_resource_t * resource, coap_session_t * session, coap_pdu_t * pdu,
                               coap_binary_t * token, coap_string_t * query, coap_pdu_t * response) {
    uint8_t * data;
    size_t len;

    c2_payload_map_t * c2_payload = NULL;
    coap_block_t block1;
    memset(&block1, 0, sizeof(block1));
    if (coap_get_block(pdu, COAP_OPTION_BLOCK1, &block1)) {
        char * app_data = NULL;
        if (!session->app) {
            coap_opt_iterator_t opt_iter;
            coap_opt_t * option;
            if ((option = coap_check_option(pdu, COAP_OPTION_SIZE1, &opt_iter)) == NULL) {
                printf("block option specified, but size option not available\n");
                return;
            }
            unsigned char buf[4];
            uint32_t size1 = coap_decode_var_bytes(coap_opt_value(option), coap_opt_length(option));
            coap_string_t * pld = coap_new_string(size1);
            pld->length = size1;
            session->app = (void *)pld;
        }
        coap_string_t * payld = (coap_string_t *)session->app;
        size_t offset = (block1.num << (block1.szx + 4));
        coap_get_data(pdu, &len, &data);
        memcpy(payld->s + offset, data, len);

        if (block1.m) {
            response->code = COAP_RESPONSE_CODE(231);
            unsigned char buff[4];
            coap_add_option(response,
                            COAP_OPTION_BLOCK1,
                            coap_encode_var_safe(buff, sizeof(buff),
                                                 ((block1.num << 4) |
                                                 (block1.m << 3) |
                                                  block1.szx)),
                            buff);
            return;
        }
        c2_payload = deserialize_payload(payld->s, payld->length);
    } else {
        coap_get_data(pdu, &len, &data);
        c2_payload = deserialize_payload(data, len);
    }

    response->code = COAP_RESPONSE_CODE(204);
    c2heartbeat_t * heartbeat = extract_c2_heartbeat(c2_payload);
    print_c2_payload(c2_payload);
    free_c2_payload(c2_payload);
    if (!heartbeat) {
        free_session_app_data(session);
        return;
    }
    char * uuid = heartbeat->agent_info.ident;
    printf("heartbeat received from client %s\n", uuid);

    //To simulate agent registration
    if (!find_agent(uuid)) {
        //send a response with payload "register"
        const char * payload = "register";
        size_t length = strlen(payload);
        response->code = COAP_RESPONSE_400;
        coap_add_data(response, length, payload);
        free_c2heartbeat(heartbeat);
        free(heartbeat);
        free_session_app_data(session);
        return;
    }

    //Look up any stored c2 operations and send
    //them to the agent in response to this heartbeat

    c2_server_responses_t * el = NULL;
    HASH_FIND_STR(responses, uuid, el);
    if (!el)  {
        free_c2heartbeat(heartbeat);
        free(heartbeat);
        free_session_app_data(session);
        return;
    }

    c2_payload_map_t * c2payload = c2_payload_server_response(el->response);
    HASH_DEL(responses, el);
    free_c2_server_responses(el->response);
    free(el);

    char * payload;
    size_t length;
    if (!serialize_payload(c2payload, &payload, &length)) {
        free_c2_payload(c2payload);
        free_c2heartbeat(heartbeat);
        free(heartbeat);
        return;
    }
    coap_add_data_blocked_response(resource, session, pdu, response, token,
                                   COAP_MEDIATYPE_APPLICATION_CBOR, 0x2ffff,
                                   length,
                                   (const uint8_t *)payload);
    free(payload);
    free_c2heartbeat(heartbeat);
    free(heartbeat);
    free_c2_payload(c2payload);
    free_session_app_data(session);
}

void start_coap_server(CoapEndpoint * endpoint) {
    while (!stop_c2) {
        if (coap_run_once(((CoapEndpoint*)endpoint)->server->ctx, 100) < 0) {
            return;
        }
    }
}

int main(int argc, char ** argv) {

    if (argc < 3) {
        printf("Usage:./c2_server <listen ip> <port>\n");
        return 0;
    }
    setup_c2_signal_action();
    little_endian = is_little_endian();
    CoapServerContext * coapctx = create_server(argv[1], argv[2]);
    CoapEndpoint * hb_endpoint = create_endpoint(coapctx, "heartbeat", COAP_REQUEST_POST, handle_heartbeat_post_request);
    CoapEndpoint * ack_endpoint = create_endpoint(coapctx, "acknowledge", COAP_REQUEST_POST, handle_acknowledge_post_request);
    CoapEndpoint * c2op_endpoint = create_endpoint(coapctx, "c2operation", COAP_REQUEST_POST, handle_c2_post_request);
    if (!hb_endpoint || !ack_endpoint || !c2op_endpoint) {
        free_server(coapctx);
        return 1;
    }
    start_coap_server(hb_endpoint);
    free_endpoint(hb_endpoint);
    free_endpoint(ack_endpoint);
    free_endpoint(c2op_endpoint);
    free_app_data(coapctx->ctx);
    free_server(coapctx);
    free_agents(ags);
}
