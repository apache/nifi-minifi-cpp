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

#ifndef LIBMINIFI_SRC_CAPI_CSTRUCTS_H_
#define LIBMINIFI_SRC_CAPI_CSTRUCTS_H_

#include "uthash.h"
#include "utlist.h"
/**
 * NiFi Port struct
 */
typedef struct {
  char *port_id;
} nifi_port;

/**
 * Nifi instance struct
 */
typedef struct {
  // should go away as we transition from C++ Instances to C implementations
  void *instance_ptr;

  nifi_port port;

} nifi_instance;

/****
 * ##################################################################
 *  C2 OPERATIONS
 * ##################################################################
 */

enum C2_Server_Type {
  REST,
  MQTT
};

typedef struct {
  char *url;
  char *ack_url;
  char *identifier;
  char *topic;
  enum C2_Server_Type type;
} C2_Server;

/****
 * ##################################################################
 *  Processor OPERATIONS
 * ##################################################################
 */

typedef struct {
  // should go away as we transition from C++ Instances to C implementations
  void *processor_ptr;
} processor;

typedef struct {
  // should go away as we transition from C++ Instances to C implementations
  void *session;
} processor_session;

/****
 * ##################################################################
 *  FLOWFILE OPERATIONS
 * ##################################################################
 */

typedef struct{
  char *key;
  char *value;
  UT_hash_handle handle;
} key_value;

typedef struct {
  const char *key;
  void *value;
  size_t value_size;
} attribute;

typedef struct {
  attribute * attributes;
  size_t size;
} attribute_set;

/**
 * State of a flow file
 *
 */
typedef struct {
  // structural definitions for moving way from C++
  unsigned modified:1;
  unsigned flushed:1;
  unsigned inmem:1;
  uint64_t id;
  uint64_t entry_date;
  uint64_t lineage_start;
  uint64_t penalty_ms;
  key_value *attribute_map;
  // structural definitions for moving way from C++

  uint64_t size; /**< Size in bytes of the data corresponding to this flow file */

  void * in;

  void * crp;

  char * contentLocation; /**< Filesystem location of this object */

  void *attributes; /**< Hash map of attributes */

  void *ffp;// should go away as we transition from C++ Instances to C implementations

} flow_file_record;

typedef struct {
  void *plan;
} flow;

typedef enum FS {
  AS_IS,
  ROLLBACK
} FailureStrategy;

#endif /* LIBMINIFI_SRC_CAPI_CSTRUCTS_H_ */
