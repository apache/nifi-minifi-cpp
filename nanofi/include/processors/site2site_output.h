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

#ifndef NIFI_MINIFI_CPP_SITE2SITE_INPUT_H
#define NIFI_MINIFI_CPP_SITE2SITE_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sitetosite/CRawSocketProtocol.h>
#include <core/message_queue.h>
#include <core/synchutils.h>
#include <ecu_api/ecuapi.h>

typedef struct site2site_output_context {
    uint64_t tcp_port;
    char port_uuid[37];
    char * host_name;
    properties_t * output_properties;
    message_queue_t * msg_queue;
    struct CRawSiteToSiteClient * client;
    lock_t client_mutex;
    int stop;
    lock_t stop_mutex;
    conditionvariable_t stop_cond;
} site2site_output_context_t;

void initialize_s2s_output(site2site_output_context_t * ctx);
void start_s2s_output(site2site_output_context_t * ctx);
void free_s2s_output_context(site2site_output_context_t * ctx);
task_state_t site2site_writer_processor(void * args, void * state);
int validate_s2s_properties(site2site_output_context_t * ctx);
void free_s2s_output_properties(site2site_output_context_t * ctx);
void wait_s2s_output_stop(site2site_output_context_t * ctx);
void write_to_s2s(site2site_output_context_t * ctx, message_t * msg);
int set_s2s_output_property(site2site_output_context_t * ctx, const char * name, const char * value);
site2site_output_context_t * create_s2s_output_context();

static const property_descriptor stos_prop_desc[3] = {
        {"host_name", "Host Name", "The host name or Ip Address of the SitetoSite node", 1, 0, 0, "HostNameValidator"},
        {"tcp_port", "Raw TCP Port", "The raw tcp port of the SitetoSite node", 1, 0, 0, "PortValidator"},
        {"nifi_input_port_id", "NiFi input port UUID", "SitetoSite input port UUID", 1, 0, 0, ""}
};

static const io_descriptor sitetosite_output_desc[1] = {
        {"SITETOSITE", 3, stos_prop_desc},
};

#ifdef __cplusplus
}
#endif

#endif //NIFI_MINIFI_CPP_SITE2SITE_INPUT_H
