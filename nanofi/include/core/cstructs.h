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

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#endif
#ifndef DEPRECATED
#ifdef _MSC_VER
#define DEPRECATED(v,ev) __declspec(deprecated)
#elif defined(__GNUC__) | defined(__clang__)
#define DEPRECATED(v,ev) __attribute__((__deprecated__))
#else
#define DEPRECATED(v,ev)
#endif
#endif

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

typedef struct processor processor;

typedef struct standalone_processor standalone_processor;

typedef struct processor_session processor_session;

typedef struct processor_context processor_context;

/****
 * ##################################################################
 *  FLOWFILE OPERATIONS
 * ##################################################################
 */

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
  uint64_t size; /**< Size in bytes of the data corresponding to this flow file */

  void * in;

  void * crp;

  char * contentLocation; /**< Filesystem location of this object */

  void * attributes; /**< Hash map of attributes */

  void * ffp;

  uint8_t keepContent;

} flow_file_record;

typedef struct flow flow;

typedef enum FS {
  AS_IS,
  ROLLBACK
} FailureStrategy;

typedef void (ontrigger_callback)(processor_session*, processor_context *);
typedef void (onschedule_callback)(processor_context *);

typedef ontrigger_callback processor_logic;

typedef struct file_buffer {
  uint8_t * buffer;
  uint64_t file_len;
} file_buffer;

#if defined(_WIN32) && defined(_WIN64)
#define PRI_SOCKET "llu"
#elif defined(_WIN32)
#define PRI_SOCKET "u"
#else
#define PRI_SOCKET "d"
typedef int SOCKET;
#endif

typedef struct cstream {
  SOCKET socket_;
} cstream;

/****
 * ##################################################################
 *  STRING OPERATIONS
 * ##################################################################
 */

typedef struct token_node {
    char * data;
    struct token_node * next;
} token_node;

typedef struct token_list {
    struct token_node * head;
    struct token_node * tail;
    uint64_t size;
    uint64_t total_bytes;
    int has_non_delimited_token;
} token_list;

/****
 * ##################################################################
 *  FLOWFILE OPERATIONS
 * ##################################################################
 */

typedef struct flow_file_list {
    flow_file_record * ff_record;
    int complete;
    struct flow_file_list * next;
} flow_file_list;

typedef struct flow_file_info {
    struct flow_file_list * ff_list;
    uint64_t total_bytes;
} flow_file_info;

#endif /* LIBMINIFI_SRC_CAPI_CSTRUCTS_H_ */
