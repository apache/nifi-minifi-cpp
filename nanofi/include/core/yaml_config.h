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

#ifndef NANOFI_INCLUDE_CORE_YAML_CONFIG_H_
#define NANOFI_INCLUDE_CORE_YAML_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "uthash.h"
#include "utlist.h"

typedef enum yaml_type {
  MAP,
  LIST,
  SCALAR
} yaml_type_t;

typedef struct config_yaml_node {
  char * key;
  char * value;
  struct config_yaml_node * list;
  struct config_yaml_node * map;
  yaml_type_t node_type;
  yaml_type_t value_type;

  struct config_yaml_node * next;
  UT_hash_handle hh;
} config_yaml_node_t;

config_yaml_node_t * parse_yaml_configuration(const char * config_file);

void free_config_yaml(config_yaml_node_t * node);

#ifdef __cplusplus
}
#endif
#endif /* NANOFI_INCLUDE_CORE_YAML_CONFIG_H_ */
