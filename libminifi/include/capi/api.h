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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define API_VERSION "0.01"
/****
 * ##################################################################
 *  BASE NIFI OPERATIONS
 * ##################################################################
 */


/**
 * NiFi Port struct
 */
typedef struct {
    char *pord_id;
}nifi_port;


/**
 * Nifi instance struct
 */
typedef struct {

  void *instance_ptr;

  nifi_port port;

} nifi_instance;


nifi_instance *create_instance(char *url, nifi_port *port);

void set_property(nifi_instance *, char *, char *);

void initialize_instance(nifi_instance *);

void free_instance(nifi_instance*);


/****
 * ##################################################################
 *  Processor OPERATIONS
 * ##################################################################
 */

typedef struct {
  void *processor_ptr;
} processor;

uint8_t run_processor(const processor *processor);


/****
 * ##################################################################
 *  FLOWFILE OPERATIONS
 * ##################################################################
 */

typedef struct{
  char *key;
  void *value;
  size_t value_size;
} attribute;

/**
 * State of a flow file
 *
 */
typedef struct {
  uint64_t size; /**< Size in bytes of the data corresponding to this flow file */

  char * contentLocation; /**< Filesystem location of this object */

  void *attributes; /**< Hash map of attributes */


} flow_file_record;



typedef struct  {
    void *plan;
} flow;


flow *create_flow(nifi_instance *, const char *);

void free_flow(flow *);

flow_file_record *get_next_flow_file(nifi_instance *, flow *);

size_t get_flow_files(nifi_instance *, flow *, flow_file_record **, size_t );


/**
 * Creates a flow file object.
 * Will obtain the size of file
 */
flow_file_record* create_flowfile(const char *file);

void free_flowfile(flow_file_record*);

uint8_t add_attribute(flow_file_record*, char *key, void *value, size_t size);

void *get_attribute(flow_file_record*, char *key);

uint8_t remove_attribute(flow_file_record*, char *key);

/****
 * ##################################################################
 *  Remote NIFI OPERATIONS
 * ##################################################################
 */

void transmit_flowfile(flow_file_record *, nifi_instance *);


/****
 * ##################################################################
 *  Persistence Operations
 * ##################################################################
 */

void transmit_flowfile(flow_file_record *, nifi_instance *);

#ifdef __cplusplus
}
#endif

#endif /* LIBMINIFI_INCLUDE_CAPI_NANOFI_H_ */
