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

#ifndef NIFI_MINIFI_CPP_C2STRUCTS_H
#define NIFI_MINIFI_CPP_C2STRUCTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <core/synchutils.h>
#include <core/cstructs.h>
#include <stdint.h>
#include <limits.h>
#include <stdint.h>
#include <uthash.h>

typedef struct systeminfo {
    char * machine_arch;
    uint64_t physical_mem;
    uint16_t v_cores;
} systeminfo;

typedef struct networkinfo {
    char * device_id;
    char host_name[256];
    char ip_address[46];
} networkinfo;

typedef struct buildinfo {
    char * version;
    char * revision;
    uint64_t timestamp;
    char * target_arch;
    char * compiler;
    char * compiler_flags;
} buildinfo;

typedef struct agentmanifest {
    char * ident;
    char * agent_type;
    char * version;
    struct buildinfo build_info;
} agentmanifest;

typedef struct agentinfo {
    char ident[37]; //uuid of the agent
    char * agent_class;
    uint64_t uptime;
} agentinfo;

typedef struct deviceinfo {
    char * ident;
    struct systeminfo system_info;
    struct networkinfo network_info;
} deviceinfo;

typedef struct {
    char ** inputs;
    size_t ip_len;
    char ** outputs;
    size_t op_len;
} io_capabilities;

typedef struct {
    char * name;
    size_t num_params;
    char ** params;
} ioparams;

typedef struct {
    const char * property_name;
    const char * display_name;
    const char * description;
    uint8_t required:1;
    uint8_t sensitive:1;
    uint8_t dynamic:1;
    const char * validator;
} property_descriptor;

typedef struct {
    const char * name;
    uint8_t num_props;
    property_descriptor * prop_descrs;
} io_descriptor;

typedef struct {
    size_t num_ips;
    io_descriptor * input_descrs;
    size_t num_ops;
    io_descriptor * output_descrs;
} io_manifest;

typedef struct {
    char uuid[37];
    const char * name;
    const char * input;
    properties_t * ip_args;
    const char * output;
    properties_t * op_args;
} ecuinfo;

typedef struct agent_manifest {
    char manifest_id[37];
    char agent_type[9]; //"nanofi"
    char version[6]; //"0.0.1"
    size_t num_ecus;
    ecuinfo * ecus;
    io_manifest io;
} agent_manifest;

typedef struct c2heartbeat {
    int is_error;
    deviceinfo device_info;
    agentinfo agent_info;
    int has_ag_manifest;
    agent_manifest ag_manifest;
} c2heartbeat_t;

typedef struct encoded_data {
    unsigned char * buff;
    size_t length;
    size_t written;
    struct encoded_data * next;
} encoded_data;

typedef enum c2operation {
    INVALID = -1,
    ACKNOWLEDGE,
    HEARTBEAT,
    CLEAR,
    DESCRIBE,
    RESTART,
    START,
    UPDATE,
    STOP
} c2operation;

//This is the c2 heartbeat response sent by c2 server
//to the nanofi agent in response to heartbeat
typedef struct c2_server_response {
    char * ident;
    c2operation operation;
    char * operand;
    properties_t * args;
    struct c2_server_response * next;
} c2_server_response_t;

//This is the acknowledgment sent by nanofi agent to the
//c2 server in response to c2 requested operation
typedef struct c2_response {
    char * ident;
    c2operation operation;
    struct c2_response * next;
} c2_response_t;

typedef struct c2_message_ctx {
    //responses to heartbeat from c2 server
    c2_server_response_t * c2_serv_resps;
    lock_t serv_resp_lock;
    conditionvariable_t serv_resp_cond;
    conditionvariable_attr_t serv_resp_cond_attr;

    //responses to c2 server
    c2_response_t * c2_resps;
    lock_t resp_lock;
	conditionvariable_t resp_cond;
	conditionvariable_attr_t resp_cond_attr;
} c2_message_ctx_t;

#ifdef __cplusplus
}
#endif

#endif //NIFI_MINIFI_CPP_C2STRUCTS_H
