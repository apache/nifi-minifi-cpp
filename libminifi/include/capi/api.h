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
#ifndef LIBMINIFI_INCLUDE_CAPI_NANOFI_H_
#define LIBMINIFI_INCLUDE_CAPI_NANOFI_H_

#include <stddef.h>
#include <stdint.h>

#include "cstructs.h"
#include "processors.h"

int initialize_api();

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Updates with every release
 */
#define API_VERSION "0.02"

void enable_logging();

/****
 * ##################################################################
 *  BASE NIFI OPERATIONS
 * ##################################################################
 */

nifi_instance *create_instance(const char *url, nifi_port *port);

void initialize_instance(nifi_instance *);

void free_instance(nifi_instance*);

/****
 * ##################################################################
 *  C2 OPERATIONS
 * ##################################################################
 */


typedef int c2_update_callback(char *);

typedef int c2_stop_callback(char *);

typedef int c2_start_callback(char *);

void enable_async_c2(nifi_instance *, C2_Server *, c2_stop_callback *, c2_start_callback *, c2_update_callback *);


uint8_t run_processor(const processor *processor);

flow *create_new_flow(nifi_instance *);

flow *create_flow(nifi_instance *, const char *);

flow *create_getfile(nifi_instance *instance, flow *parent, GetFileConfig *c);

processor *add_processor(flow *, const char *);

processor *add_processor_with_linkage(flow *flow, const char *processor_name);

processor *add_python_processor(flow *, void (*ontrigger_callback)(processor_session *session));

/**
* Register your callback to received flow files that the flow failed to process
* The flow file is deleted after the callback is executed, make sure to copy all the data you need!
* The first callback should be registered before the flow is used. Can be changed later during runtime.
*/
int add_failure_callback(flow *flow, void (*onerror_callback)(const flow_file_record*));

int set_property(processor *, const char *, const char *);

int set_instance_property(nifi_instance *instance, const char*, const char *);

int free_flow(flow *);

flow_file_record *get_next_flow_file(nifi_instance *, flow *);

size_t get_flow_files(nifi_instance *, flow *, flow_file_record **, size_t);

flow_file_record *get(nifi_instance *,flow *, processor_session *);

int transfer(processor_session* session, flow *flow, const char *rel);

/**
 * Creates a flow file object.
 * Will obtain the size of file
 */
flow_file_record* create_flowfile(const char *file, const size_t len);

flow_file_record* create_ff_object(const char *file, const size_t len, const uint64_t size);

flow_file_record* create_ff_object_na(const char *file, const size_t len, const uint64_t size);

void free_flowfile(flow_file_record*);

uint8_t add_attribute(flow_file_record*, const char *key, void *value, size_t size);

void update_attribute(flow_file_record*, const char *key, void *value, size_t size);

uint8_t get_attribute(flow_file_record *ff, attribute *caller_attribute);

int get_attribute_qty(const flow_file_record* ff);

int get_all_attributes(const flow_file_record* ff, attribute_set *target);

uint8_t remove_attribute(flow_file_record*, char *key);

/****
 * ##################################################################
 *  Remote NIFI OPERATIONS
 * ##################################################################
 */

int transmit_flowfile(flow_file_record *, nifi_instance *);

/****
 * ##################################################################
 *  Persistence Operations
 * ##################################################################
 */


#ifdef __cplusplus
}
#endif

#endif /* LIBMINIFI_INCLUDE_CAPI_NANOFI_H_ */
