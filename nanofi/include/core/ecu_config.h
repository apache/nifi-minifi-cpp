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

#ifndef NANOFI_INCLUDE_CORE_ECU_CONFIG_H_
#define NANOFI_INCLUDE_CORE_ECU_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "uthash.h"
#include <core/yaml_config.h>
#include <core/cstructs.h>

typedef struct storage_config {
  char * storage_path;
  char * sync_mode;
  int storage_type;
  size_t chunk_size; // defaults to 4096
  size_t max_chunks_up; // defaults to 64
} storage_config;

typedef struct ecu_config {
  char * name; // key
  char * stream_name;
  properties_t * input_props;
  properties_t * output_props;
  UT_hash_handle hh;
} ecu_config;

ecu_config * parse_ecu_configuration(config_yaml_node_t * node);

storage_config * parse_storage_configuration(config_yaml_node_t * node);

ecu_config * find_ecu_configuration(const char * name);

int validate_ecu_configuration(ecu_config * config);

void free_ecu_configuration(ecu_config * conf);
void free_all_ecu_configuration(ecu_config * config);
void free_ecu_config_by_name(const char * name, ecu_config * config);
void free_storage_config(storage_config * strg_conf);

#ifdef __cplusplus
}
#endif
#endif /* NANOFI_INCLUDE_CORE_ECU_CONFIG_H_ */
