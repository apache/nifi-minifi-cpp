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
 * This is meant to be used as a c2 client to test
 * c2 operations on ecus controlled via c2 agent.
 * It sends c2 operations specified in the configuration
 * file to a c2 server that is listening for c2 heartbeats
 * from a c2 agent
 */

#ifdef __cplusplus
extern "C" {
#endif
#include <coap/c2structs.h>
#include <coap/c2protocol.h>
#include <coap/c2agent.h>
#include <c2_api/c2api.h>
#include <api/ecu.h>
#include <core/string_utils.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "utlist.h"
#include "uthash.h"

c2operation c2_operation_from_string(const char * operation) {
    if (strcmp(operation, "START") == 0) {
        return START;
    }

    if (strcmp(operation, "STOP") == 0) {
        return STOP;
    }

    if (strcmp(operation, "RESTART") == 0) {
        return RESTART;
    }

    if (strcmp(operation, "UPDATE") == 0) {
        return UPDATE;
    }

    return -1;
}

/**
 * Example input params in input params file:
 * input.file.file_path=./e1.txt
 * input.file.delimiter=;
 * input.file.tail_frequency_ms=1000
 *
 * Example output params in output params file:
 * output.sitetosite.nifi_port_uuid=9545fe6a-016e-1000-dbf1-8e636aee4e1f
 * output.sitetosite.tcp_port=10000
 * output.sitetosite.host_name=<ip address> or <host name>
 *
 * Example config_file params:
 * host=<c2 server hostname or ip address>
 * port=<c2 server port>
 *
 * The purpose of this program is to serve
 * as a C2 client. This client sends c2 operations
 * based on the inputs from configuration files
 * to the C2 server. C2 server forwards the C2 operations
 * as a response to the heartbeat from C2 agents.
 */
void print_usage() {
    printf("usage: ./c2_client <config_file> <c2operation [START | STOP | UPDATE | RESTART]> <c2agent uuid> <ecu uuid> <input_params_file> <output_params_file>\n");
}

int main(int argc, char ** argv) {
    if (argc < 5) {
        print_usage();
        return 1;
    }

    const char * operation = argv[2];
    const char * uuid = argv[3];

    c2operation op = c2_operation_from_string(operation);

    if (op < 0 || (argc > 5 && argc < 7 && op != STOP)) {
        printf("Invalid input.\n");
        print_usage();
        return 1;
    }

    const char * config_file = argv[1];

    char * input_file = NULL;
    char * output_file = NULL;
    if (op == START || op == UPDATE || op == RESTART) {
        input_file = argv[5];
        if (argc > 5) {
            output_file = argv[6];
        }
    }

    properties_t * config = read_configuration_file(config_file);

    properties_t * host = NULL;
    HASH_FIND_STR(config, "host", host);
    properties_t * port = NULL;
    HASH_FIND_STR(config, "port", port);

    if (!host || !port) {
        free_properties(config);
        return 1;
    }
    char * host_name = host->value;
    printf("host = %s\n", host_name);
    char * port_str = port->value;
    printf("port = %s\n", port_str);
    size_t port_num = (uint64_t)strtoul(port_str, NULL, 10);

    properties_t * input_params = NULL;
    if (input_file) {
        input_params = read_configuration_file(input_file);
    }

    properties_t * output_params = NULL;
    if (output_file) {
        output_params = read_configuration_file(output_file);
    }

    c2context_t * c2 = create_c2_agent(host_name, port_str);
    c2->is_little_endian = is_little_endian();
    initialize_coap(c2);

    c2_server_response_t * resp = (c2_server_response_t *)malloc(sizeof(c2_server_response_t));
    memset(resp, 0, sizeof(c2_server_response_t));
    copystr(uuid, &resp->ident);

    char * operand_str = argv[4];
    copystr(operand_str, &resp->operand);

    resp->operation = op;

    if (input_params) {
        properties_t * el, *tmp;
        HASH_ITER(hh, input_params, el, tmp) {
            properties_t * op_args = (properties_t *)malloc(sizeof(properties_t));
            memset(op_args, 0, sizeof(properties_t));
            copystr(el->key, &op_args->key);
            copystr(el->value, &op_args->value);
            HASH_ADD_KEYPTR(hh, resp->args, op_args->key, strlen(op_args->key), op_args);
        }
    }

    if (output_params) {
        properties_t * el, *tmp;
        HASH_ITER(hh, output_params, el, tmp) {
            properties_t * op_args = (properties_t *)malloc(sizeof(properties_t));
            memset(op_args, 0, sizeof(properties_t));
            copystr(el->key, &op_args->key);
            copystr(el->value, &op_args->value);
            HASH_ADD_KEYPTR(hh, resp->args, op_args->key, strlen(op_args->key), op_args);
        }
    }

    char * payload;
    size_t length;
    c2_payload_map_t * c2_payload = c2_payload_server_response(resp);
    serialize_payload(c2_payload, &payload, &length);
    free_c2_payload(c2_payload);
    free_c2_server_responses(resp);
#ifdef WIN32
    WSADATA wsadata;
    int err = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (err != 0) {
        return NULL;
    }
#endif
    CoapMessage coap_msg;
    coap_msg.data_ = (uint8_t*)payload;
    coap_msg.size_ = length;
    send_payload(c2, "c2operation", &coap_msg);

    free_properties(config);
    free_properties(output_params);
    free_properties(input_params);
    free(coap_msg.data_);
    destroy_c2_context(c2);

#ifdef WIN32
    WSACleanup();
#endif
}

#ifdef __cplusplus
}
#endif

