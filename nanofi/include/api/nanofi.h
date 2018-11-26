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

#include "core/cstructs.h"
#include "core/processors.h"

int initialize_api();

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Updates with every release. Functions used here constitute the public API of NanoFi.
 *
 * Changes here will follow semver
 */
#define API_VERSION "0.02"

#define SUCCESS_RELATIONSHIP "success"
#define FAILURE_RELATIONSHIP "failure"

void enable_logging();

void set_terminate_callback(void (*terminate_callback)());

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

processor *add_python_processor(flow *, void (*ontrigger_callback)(processor_session *session));

standalone_processor *create_processor(const char *);

void free_standalone_processor(standalone_processor*);

/**
* Register your callback to received flow files that the flow failed to process
* The flow file ownership is transferred to the caller!
* The first callback should be registered before the flow is used. Can be changed later during runtime.
*/
int add_failure_callback(flow *flow, void (*onerror_callback)(flow_file_record*));


/**
* Set failure strategy. Please use the enum defined in cstructs.h
* Return values: 0 (success), -1 (strategy cannot be set - no failure callback added?)
* Can be changed runtime.
* The default strategy is AS IS.
*/
int set_failure_strategy(flow *flow, FailureStrategy strategy);

int set_property(processor *, const char *, const char *);

int set_standalone_property(standalone_processor*, const char*, const char *);

int set_instance_property(nifi_instance *instance, const char*, const char *);

char * get_property(const processor_context *  context, const char * name);

int free_flow(flow *);

flow_file_record *get_next_flow_file(nifi_instance *, flow *);

size_t get_flow_files(nifi_instance *, flow *, flow_file_record **, size_t);

flow_file_record *get(nifi_instance *,flow *, processor_session *);

flow_file_record *invoke(standalone_processor* proc);

flow_file_record *invoke_ff(standalone_processor* proc, const flow_file_record *input_ff);

flow_file_record *invoke_file(standalone_processor* proc, const char* path);

flow_file_record *invoke_chunck(standalone_processor* proc, uint8_t* buf, uint64_t);

int transfer(processor_session* session, flow *flow, const char *rel);

/**
 * Creates a flow file object.
 * Will obtain the size of file
 */
flow_file_record* create_flowfile(const char *file, const size_t len);

flow_file_record* create_ff_object(const char *file, const size_t len, const uint64_t size);

flow_file_record* create_ff_object_na(const char *file, const size_t len, const uint64_t size);

/**
 * Get incoming flow file. To be used in processor logic callbacks.
 */
flow_file_record* get_flowfile(processor_session* session, processor_context* context);

void free_flowfile(flow_file_record*);

uint8_t add_attribute(flow_file_record*, const char *key, void *value, size_t size);

void update_attribute(flow_file_record*, const char *key, void *value, size_t size);

uint8_t get_attribute(const flow_file_record *ff, attribute *caller_attribute);

int get_attribute_qty(const flow_file_record* ff);

int get_all_attributes(const flow_file_record* ff, attribute_set *target);

/**
 * reads the content of a flow file
 * @param target reference in which will set the result
 * @param size max number of bytes to read (use flow_file_record->size to get the whole content)
 * @return resulting read size (<=size)
 **/
int get_content(const flow_file_record* ff, uint8_t* target, int size);

uint8_t remove_attribute(flow_file_record*, char *key);

/****
 * ##################################################################
 *  Remote NIFI OPERATIONS
 * ##################################################################
 */

int transmit_flowfile(flow_file_record *, nifi_instance *);

int add_custom_processor(const char * name, processor_logic* logic);

int delete_custom_processor(const char * name);

int transfer_to_relationship(flow_file_record * ffr, processor_session * ps, const char * relationship);

/****
 * ##################################################################
 *  Persistence Operations
 * ##################################################################
 */


#ifdef __cplusplus
}
#endif

#endif /* LIBMINIFI_INCLUDE_CAPI_NANOFI_H_ */
