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

#ifndef NIFI_MINIFI_CPP_ECUAPI_H
#define NIFI_MINIFI_CPP_ECUAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <core/threadpool.h>
#include <core/storage.h>
#include <core/ecu_config.h>
#include "uthash.h"
#include "io_api.h"

typedef struct input_context {
    io_type_t type;
    void * proc_ctx;
} input_context_t;

typedef struct output_context {
    io_type_t type;
    void * proc_ctx;
} output_context_t;

typedef struct ecu_context {
    char * name;
    char * strm_name;
    char uuid[37];
    input_context_t * input;
    output_context_t * output;
    struct io_context * io;
    storage_stream * stream;
    int started;
    lock_t ctx_lock;
    struct ecu_context * next;
} ecu_context_t;

typedef struct io_context {
    threadpool_t * thread_pool;
    ecu_context_t * ecus;
    storage_config * strg_conf;
    lock_t ctx_lock;
} io_context_t;

typedef int (*on_start_callback_t)(ecu_context_t * ecu_ctx, io_type_t input, io_type_t output, properties_t * input_props, properties_t * output_props);
typedef int (*on_stop_callback_t)(ecu_context_t * ecu_ctx);

typedef struct manual_input_context {
    message_t * message;
} manual_input_context_t;

ecu_context_t * create_ecu(io_context_t * io, const char * name, const char * strm_name, input_context_t *ip, output_context_t * op);
int set_ecu_input_property(ecu_context_t * ctx, const char * name, const char * value);
int set_ecu_output_property(ecu_context_t * ctx, const char * name, const char * value);
int start_ecu_async(ecu_context_t * ctx);
int stop_ecu_context(ecu_context_t * ctx);
void destroy_ecu(ecu_context_t * ctx);

int on_start(ecu_context_t * ecu_ctx, io_type_t input, io_type_t output, properties_t * input_props, properties_t * output_props);
int on_update(ecu_context_t * ecu_ctx, io_type_t input, io_type_t output, properties_t * input_props, properties_t * output_props);
int on_stop(ecu_context_t * ecu_ctx);
int on_clear(ecu_context_t * ecu_ctx);

properties_t * get_input_properties(ecu_context_t * ctx);
properties_t * get_output_properties(ecu_context_t * ctx);

manual_input_context_t * create_manual_input_context();
void ingest_input_data(ecu_context_t * ctx, const char * payload, size_t len, properties_t * attrs);
void ecu_push_output(ecu_context_t * ctx);
void free_manual_input_context(manual_input_context_t * ctx);
void ingest_and_push_out(ecu_context_t * ctx, const char * payload, size_t len, properties_t * attrs);

void get_input_name(ecu_context_t * ecu, char ** input);
void get_output_name(ecu_context_t * ecu, char ** output);
io_type_t get_io_type(const char * name);
properties_t * get_input_args(ecu_context_t * ecu);
properties_t * get_output_args(ecu_context_t * ecu);

//io_manifest get_io_manifest();

io_context_t * create_io_context();
void destroy_io_context(io_context_t * io);
void remove_ecu_iocontext(io_context_t * io, ecu_context_t * ecu);
input_context_t * create_input(io_type_t type);
void free_input(input_context_t * input);
int set_input_property(input_context_t * ip, const char * key, const char * value);
output_context_t * create_output(io_type_t type);
void free_output(output_context_t * output);
int set_output_property(output_context_t * op, const char * key, const char * value);

#ifdef __cplusplus
}
#endif
#endif //NIFI_MINIFI_CPP_ECUAPI_H
