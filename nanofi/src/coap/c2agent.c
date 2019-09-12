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

#if defined(__APPLE__) && defined(__MACH__)
#define MAC
#include <sys/sysctl.h>
#elif defined(WIN32)
#pragma comment(lib, "ws2_32.lib")
#define _WINSOCKAPI_
#include <winsock.h>
#include <ws2tcpip.h>
#include <sysinfoapi.h>
#include <windows.h>
#else
#include <sys/sysinfo.h>
#endif
#include <core/synchutils.h>
#include <core/log.h>
#include <core/string_utils.h>

#ifndef WIN32
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#endif

#include "coap/c2structs.h"
#include "coap/c2agent.h"
#include "coap/c2protocol.h"
#include "coap/coapprotocol.h"

#include "nanofi/coap_message.h"

#include <errno.h>
#include "utlist.h"
#include <core/cuuid.h>

uint64_t get_physical_memory_size() {
#ifdef MAC
    uint64_t memsize = 0;
    size_t len = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &len, NULL, 0) == 0) {
        return memsize;
    }
#elif defined(WIN32)
    uint64_t totalMemoryKB;
    if (GetPhysicallyInstalledSystemMemory(&totalMemoryKB)) {
        return (totalMemoryKB * 1024);
    }
#else
    struct sysinfo sys_info;
    if (sysinfo(&sys_info) == 0) {
        return sys_info.totalram;
    }
#endif
    return 0;
}

uint16_t get_num_vcores() {
#ifdef MAC
    uint16_t num_cpus = 0;
    size_t len = sizeof(num_cpus);
    if (sysctlbyname("hw.logicalcpu", &num_cpus, &len, NULL, 0) == 0) {
        return num_cpus;
    }
    return 0;
#elif defined(WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (uint16_t)sysinfo.dwNumberOfProcessors;
#else
    long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpus < 0) {
        return 0;
    }
    return (uint16_t)ncpus;
#endif
}

int get_host_name(char * buffer, size_t len) {
    if (len == 0 || !buffer) return -1;
#ifdef WIN32
    WSADATA wsadata;
    int err = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (err != 0) {
        return -1;
    }
    if (gethostname(buffer, len) != 0) {
        WSACleanup();
        return -1;
    }
    WSACleanup();
    return 0;
#else
    if (gethostname(buffer, len) < 0) {
        return -1;
    }
    return 0;
#endif
}

char * get_ipaddr_from_hostname(const char * hostname) {
    size_t len = 45;
#ifdef WIN32
    WSADATA wsadata;
    int err = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (err != 0) {
        return NULL;
    }
#endif
    struct addrinfo hints, *infoptr;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;

    int result = getaddrinfo(hostname, NULL, &hints, &infoptr);
    if (result) {
        logc(err, "error getting addr info getaddrinfo: %s", gai_strerror(result));
        return NULL;
    }

    char * host = (char *)malloc((len+1) * sizeof(char));
    struct addrinfo *p;
    for (p = infoptr; p != NULL; p = p->ai_next) {
        if (getnameinfo(p->ai_addr, p->ai_addrlen, host, len, NULL, 0, NI_NUMERICHOST) == 0) {
            freeaddrinfo(infoptr);
            return host;
        }
    }

    freeaddrinfo(infoptr);
    free(host);
#ifdef WIN32
    WSACleanup();
#endif
    return NULL;
}

const char * get_machine_architecture() {
#ifdef WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    DWORD parch = sysinfo.wProcessorArchitecture;
    char * buff = (char *)malloc(8 * sizeof(char));
    memset(buff, 0, 8);
    switch (parch) {
    case PROCESSOR_ARCHITECTURE_AMD64:
        strcpy(buff, "x64");
        break;
    case PROCESSOR_ARCHITECTURE_ARM:
        strcpy(buff, "arm");
        break;
    case PROCESSOR_ARCHITECTURE_IA64:
        strcpy(buff, "ia64");
        break;
    case PROCESSOR_ARCHITECTURE_INTEL:
        strcpy(buff, "x86");
        break;
    default:
        strcpy(buff, "unknown");
        break;
    }
    return buff;
#else
    struct utsname name;
    if (uname(&name) != 0) {
    	return NULL;
    }
    char * machine_name = NULL;
    copystr(name.machine, &machine_name);
    return machine_name;
#endif
}

c2heartbeat_t prepare_c2_heartbeat(const char * agent_id, uint64_t start_time) {
    c2heartbeat_t c2_heartbeat;
    memset(&c2_heartbeat, 0, sizeof(c2heartbeat_t));

    if (!agent_id) {
        c2_heartbeat.is_error = 1;
        return c2_heartbeat;
    }

    strcpy(c2_heartbeat.agent_info.ident, agent_id);
    c2_heartbeat.agent_info.uptime = time(NULL) - start_time;

    //TODO get these from c2 configuration
    copystr("default", &c2_heartbeat.agent_info.agent_class);
    copystr("identifier", &c2_heartbeat.device_info.ident);
    copystr("device_id", &c2_heartbeat.device_info.network_info.device_id);

    //device_info.network_info
    size_t len = sizeof(c2_heartbeat.device_info.network_info.host_name);
    memset(c2_heartbeat.device_info.network_info.host_name, 0, len);
    if (get_host_name(c2_heartbeat.device_info.network_info.host_name, len) < 0) {
        strcpy(c2_heartbeat.device_info.network_info.host_name, "unknown");
    }
    char * ipaddr = get_ipaddr_from_hostname(c2_heartbeat.device_info.network_info.host_name);
    if (ipaddr) {
        strcpy(c2_heartbeat.device_info.network_info.ip_address, ipaddr);
        free(ipaddr);
    } else {
        strcpy(c2_heartbeat.device_info.network_info.ip_address, "unknown");
    }

    //device_info.system_info
    const char * machine_arch = get_machine_architecture();
    if (machine_arch == NULL) {
        copystr("unknown", &c2_heartbeat.device_info.system_info.machine_arch);
    } else {
    	c2_heartbeat.device_info.system_info.machine_arch = machine_arch;
    }

    c2_heartbeat.device_info.system_info.physical_mem = get_physical_memory_size();
    c2_heartbeat.device_info.system_info.v_cores = get_num_vcores();

    return c2_heartbeat;
}

void prepare_agent_manifest(c2context_t * c2_ctx, c2heartbeat_t * hb) {
    size_t num_ecus = 0;
    num_ecus = HASH_COUNT(c2_ctx->ecus);
    hb->has_ag_manifest = 1;
    agent_manifest ag_manifest;
    memset(&ag_manifest, 0, sizeof(agent_manifest));

    CIDGenerator gen;
    gen.implementation_ = CUUID_DEFAULT_IMPL;
    generate_uuid(&gen, ag_manifest.manifest_id);

    strcpy(ag_manifest.agent_type, "nanofi-c");
    strcpy(ag_manifest.version, "0.0.1");

    ag_manifest.num_ecus = num_ecus;
    ag_manifest.ecus = (ecuinfo *)malloc(num_ecus * sizeof(ecuinfo));
    memset(ag_manifest.ecus, 0, (num_ecus * sizeof(ecuinfo)));

    ecu_entry_t * el, *tmp;
    int i = 0;
    HASH_ITER(hh, c2_ctx->ecus, el, tmp) {
        strcpy(ag_manifest.ecus[i].uuid, el->uuid);
        ag_manifest.ecus[i].name = (const char *)malloc(strlen(el->ecu->name) + 1);
        strcpy((char *)ag_manifest.ecus[i].name, el->ecu->name);
        char * input;
        get_input_name(el->ecu, &input);
        if (input && strlen(input) > 0) {
            size_t ip_len = strlen(input);
            ag_manifest.ecus[i].input = (const char *)malloc(ip_len + 1);
            strcpy((char *)ag_manifest.ecus[i].input, input);
            free(input);
        }
        ag_manifest.ecus[i].ip_args = get_input_args(el->ecu);

        char * output;
        get_output_name(el->ecu, &output);
        if (output && strlen(output) > 0) {
            size_t op_len = strlen(output);
            ag_manifest.ecus[i].output = (const char *)malloc(op_len + 1);
            strcpy((char *)ag_manifest.ecus[i].output, output);
            free(output);
        }
        ag_manifest.ecus[i].op_args = get_output_args(el->ecu);
    }
    hb->ag_manifest = ag_manifest;
    hb->ag_manifest.io = get_io_manifest();
}

c2_response_t * prepare_c2_response(const char * operation_id) {
    if (!operation_id || strlen(operation_id) == 0) {
        return NULL;
    }
    c2_response_t * c2_resp = (c2_response_t *)malloc(sizeof(c2_response_t));
    memset(c2_resp, 0, sizeof(c2_response_t));
    copystr(operation_id, &c2_resp->ident);
    c2_resp->operation = ACKNOWLEDGE;
    return c2_resp;
}

typedef struct {
    char * ip_name;
    char * op_name;
    properties_t * ip_props;
    properties_t * op_props;
} io_properties;

void parse_operation_args(properties_t * entry, const char ** type, const char ** name, const char ** key, const char ** value) {
    char ** tokens = parse_tokens(entry->key, strlen(entry->key), 3, ".");
    *type = tokens[0];
    *name = tokens[1];
    *key = tokens[2];
    copystr(entry->value, (char **)value);
    free(tokens);
}

io_properties prepare_io_properties(properties_t * opargs) {
    io_properties io_props;
    memset(&io_props, 0, sizeof(io_properties));
    properties_t * el, *tmp;
    HASH_ITER(hh, opargs, el, tmp) {
        const char * type = NULL;
        const char * name = NULL;
        const char * key = NULL;
        const char * value = NULL;
        parse_operation_args(el, &type, &name, &key, &value);
        if (type && name && key && value) {
            properties_t * prop = (properties_t *)malloc(sizeof(properties_t));
            prop->key = key;
            prop->value = value;
            if (strcmp(type, "input") == 0) {
                if (!io_props.ip_name) io_props.ip_name = name;
                else free((void *)name);
                HASH_ADD_KEYPTR(hh, io_props.ip_props, prop->key, strlen(prop->key), prop);
            } else if (strcmp(type, "output") == 0) {
                if (!io_props.op_name) io_props.op_name = name;
                else free((void *)name);
                HASH_ADD_KEYPTR(hh, io_props.op_props, prop->key, strlen(prop->key), prop);
            } else {
                free((void *)name);
                free((void *)key);
                free((void *)value);
                free((void *)prop);
            }
            free((void *)type);
        }
    }
    return io_props;
}

typedef struct {
    io_type_t ip_type;
    io_type_t op_type;
    properties_t * ip_props;
    properties_t * op_props;
} io_params;

io_params get_io_params(properties_t * args) {
    io_properties io = prepare_io_properties(args);
    io_type_t ip_type = get_io_type(io.ip_name);
    io_type_t op_type = get_io_type(io.op_name);
    free(io.ip_name);
    free(io.op_name);
    io_params ioparams;
    ioparams.ip_type = ip_type;
    ioparams.op_type = op_type;
    ioparams.ip_props = io.ip_props;
    ioparams.op_props = io.op_props;
    return ioparams;
}

void start(c2context_t * c2, ecu_context_t * ecu, c2_server_response_t * resp) {
    if (c2->on_start) {
        io_params io = get_io_params(resp->args);
        c2->on_start(ecu, io.ip_type, io.op_type, io.ip_props, io.op_props);
        free_properties(io.ip_props);
        free_properties(io.op_props);
    }
}

void stop(c2context_t * c2, ecu_context_t * ecu) {
    if (c2->on_stop) {
        c2->on_stop(ecu);
    }
}

void update(c2context_t * c2, ecu_context_t * ecu, c2_server_response_t * resp) {
    if (c2->on_update) {
        io_params io = get_io_params(resp->args);
        c2->on_update(ecu, io.ip_type, io.op_type, io.ip_props, io.op_props);
        free_properties(io.ip_props);
        free_properties(io.op_props);
    }
}

ecu_entry_t * find_ecu(ecu_entry_t * ecus, const char * uuid) {
    ecu_entry_t * entry = NULL;
    HASH_FIND_STR(ecus, uuid, entry);
    return entry;
}

void handle_c2_server_response(c2context_t * c2, c2_server_response_t * resp) {
    if (!resp) return;

    acquire_lock(&c2->ecus_lock);
    /*
     * In the first release, we are going to have only one ecu
     * per c2 agent. Later when we support multiple ecus
     * the below code can be uncommented
     */
    /*ecu_entry_t * entry = find_ecu(c2->ecus, resp->operand);
    if (!entry) {
        pthread_mutex_unlock(&c2->ecus_lock);
        return;
    }*/
    ecu_entry_t * entry = c2->ecus;
    switch (resp->operation) {
    case START: {
        start(c2, entry->ecu, resp);
        break;
    }
    case STOP: {
        stop(c2, entry->ecu);
        break;
    }
    case UPDATE: {
        update(c2, entry->ecu, resp);
        break;
    }
    //TODO implement RESTART
    default:
        break;
    }
    release_lock(&c2->ecus_lock);
}

void free_c2_responses(c2_response_t * resps) {
    while (resps) {
        c2_response_t * tmp = resps;
        resps = resps->next;
        free((void *)tmp->ident);
        free(tmp);
    }
}

void free_c2_server_responses(c2_server_response_t * resps) {
    while (resps) {
        c2_server_response_t * tmp = resps;
        resps = resps->next;
        free(tmp->ident);
        free(tmp->operand);
        free_properties(tmp->args);
        free(tmp);
    }
}

void free_c2_message_context(c2_message_ctx_t * ctx) {
    acquire_lock(&ctx->serv_resp_lock);
    free_c2_server_responses(ctx->c2_serv_resps);
    destroy_cvattr(&ctx->serv_resp_cond_attr);
    destroy_cv(&ctx->serv_resp_cond);
    destroy_lock(&ctx->serv_resp_lock);

    acquire_lock(&ctx->resp_lock);
    free_c2_responses(ctx->c2_resps);
    destroy_cvattr(&ctx->resp_cond_attr);
    destroy_cv(&ctx->resp_cond);
    destroy_lock(&ctx->resp_lock);
    free(ctx);
}

void free_coap_msg(coap_message msg) {
    free(msg.data);
}

void free_c2_coap_messages(coap_messages * msgs) {
    if (msgs) {
        struct coap_messages * el, *tmp;
        HASH_ITER(hh, msgs, el, tmp) {
            HASH_DEL(msgs, el);
            free_coap_msg(el->coap_msg);
            free(el);
        }
    }
}

c2_message_ctx_t * create_c2_message_context() {
    c2_message_ctx_t * msg_ctx = (c2_message_ctx_t *)malloc(sizeof(c2_message_ctx_t));
    memset(msg_ctx, 0, sizeof(c2_message_ctx_t));
    initialize_lock(&msg_ctx->resp_lock);
    initialize_lock(&msg_ctx->serv_resp_lock);
#if defined(__APPLE__) || defined(WIN32)
    initialize_cv(&msg_ctx->resp_cond, NULL);
    initialize_cv(&msg_ctx->serv_resp_cond, NULL);
#else
    initialize_cvattr(&msg_ctx->resp_cond_attr);
    initialize_cvattr(&msg_ctx->serv_resp_cond_attr);
    initialize_cv(&msg_ctx->resp_cond, &msg_ctx->resp_cond_attr);
    initialize_cv(&msg_ctx->serv_resp_cond, &msg_ctx->serv_resp_cond_attr);
    condition_attr_set_clock(&msg_ctx->resp_cond_attr, CLOCK_MONOTONIC);
    condition_attr_set_clock(&msg_ctx->serv_resp_cond_attr, CLOCK_MONOTONIC);
#endif
    return msg_ctx;
}

void enqueue_c2_serv_response(c2context_t * c2, c2_server_response_t * serv_resp) {
    if (!c2 || !serv_resp) return;

    acquire_lock(&c2->c2_msg_ctx->serv_resp_lock);
    LL_APPEND(c2->c2_msg_ctx->c2_serv_resps, serv_resp);
    condition_variable_signal(&c2->c2_msg_ctx->serv_resp_cond);
    release_lock(&c2->c2_msg_ctx->serv_resp_lock);
}

c2_server_response_t * dequeue_c2_serv_response(c2context_t * c2) {
    if (!c2) return NULL;

    acquire_lock(&c2->c2_msg_ctx->serv_resp_lock);

    while (!c2->c2_msg_ctx->c2_serv_resps) {
        int ret = condition_variable_timedwait(&c2->c2_msg_ctx->serv_resp_cond, &c2->c2_msg_ctx->serv_resp_lock, 200);
        if (ret == ETIMEDOUT) {
            release_lock(&c2->c2_msg_ctx->serv_resp_lock);
            return NULL;
        }
    }

    c2_server_response_t * head = c2->c2_msg_ctx->c2_serv_resps;
    c2->c2_msg_ctx->c2_serv_resps = c2->c2_msg_ctx->c2_serv_resps->next;
    head->next = NULL;
    release_lock(&c2->c2_msg_ctx->serv_resp_lock);
    return head;
}

void enqueue_c2_resp(c2context_t * c2, c2_response_t * resp) {
    if (!c2 || !resp) return;

    acquire_lock(&c2->c2_msg_ctx->resp_lock);
    LL_APPEND(c2->c2_msg_ctx->c2_resps, resp);
    condition_variable_signal(&c2->c2_msg_ctx->resp_cond);
    release_lock(&c2->c2_msg_ctx->resp_lock);
}

c2_response_t * dequeue_c2_resp(c2context_t * c2) {
    if (!c2) return NULL;

    acquire_lock(&c2->c2_msg_ctx->resp_lock);

    while (!c2->c2_msg_ctx->c2_resps) {
        int ret = condition_variable_timedwait(&c2->c2_msg_ctx->resp_cond, &c2->c2_msg_ctx->resp_lock, 200);
        if (ret == ETIMEDOUT) {
            release_lock(&c2->c2_msg_ctx->resp_lock);
            return NULL;
        }
    }

    c2_response_t * head = c2->c2_msg_ctx->c2_resps;
    c2->c2_msg_ctx->c2_resps = c2->c2_msg_ctx->c2_resps->next;
    head->next = NULL;
    release_lock(&c2->c2_msg_ctx->resp_lock);
    return head;
}
