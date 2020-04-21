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

#include <stdlib.h>
#include "utlist.h"
#include <core/ecu_config.h>
#include <core/log.h>
#include <chunkio/chunkio_compat.h>
#include <chunkio/chunkio.h>

int parse_ecu_properties(config_yaml_node_t * node, ecu_config * conf) {
  if (!node || !conf) return 0;

  config_yaml_node_t * out;
  HASH_FIND(hh, node, "name", strlen("name"), out);

  if (!out || !out->value || strlen(out->value) == 0) {
    logc(err, "ecu name not provided or empty");
    return 0;
  }
  conf->name = copystr(out->value);

  HASH_FIND(hh, node, "stream_name", strlen("stream_name"), out);
  if (!out || !out->value || strlen(out->value) == 0) {
    logc(err, "ecu stream name not provided or empty");
    return 0;
  }
  conf->stream_name = copystr(out->value);
  return 1;
}

int validate_config_property(config_yaml_node_t * element) {
  return element->key
      && strlen(element->key) > 0
      && element->value
      && strlen(element->value) > 0;
}

properties_t * parse_io_properties(config_yaml_node_t * node, const char * key) {
  if (!node || !key || strlen(key) == 0) return NULL;

  properties_t * props = NULL;
  struct config_yaml_node * io;
  HASH_FIND(hh, node, key, strlen(key), io);
  if (io) {
    struct config_yaml_node * el, *tmp;
    HASH_ITER(hh, io->map, el, tmp) {
      if (!validate_config_property(el)) {
        continue;
      }
      properties_t * prop = (properties_t *)malloc(sizeof(properties_t));
      prop->key = copystr(el->key);
      prop->value = copystr(el->value);
      HASH_ADD_KEYPTR(hh, props, prop->key, strlen(prop->key), prop);
    }
  }
  return props;
}

storage_config * validate_storage_configuration(config_yaml_node_t * node) {
  struct config_yaml_node * sp, *sm, *st, *cs, *mc;
  HASH_FIND(hh, node, "storage_type", strlen("storage_type"), st);

  if (!st || !st->value || !strlen(st->value)) {
    logc(err, "no storage type provided");
    return NULL;
  }

  storage_config * conf = (storage_config *)malloc(sizeof(storage_config));
  memset(conf, 0, sizeof(*conf));

  if (strcasecmp(st->value, "filesystem") == 0) {
    conf->storage_type = CIO_STORE_FS;
  } else if (strcasecmp(st->value, "memory") == 0) {
    conf->storage_type = CIO_STORE_MEM;
  } else {
    logc(err, "invalid storage type provided %s", st->value);
    free_storage_config(conf);
    free(conf);
    return NULL;
  }

  if (conf->storage_type == CIO_STORE_FS) {
    HASH_FIND(hh, node, "storage_path", strlen("storage_path"), sp);
    HASH_FIND(hh, node, "sync_mode", strlen("sync_mode"), sm);
    if (!sp || !sp->value || !strlen(sp->value)
        || !sm || !sm->value || !strlen(sm->value)) {
      free_storage_config(conf);
      free(conf);
      return NULL;
    }
    conf->storage_path = copystr(sp->value);
    conf->sync_mode = copystr(sm->value);
  }

  HASH_FIND(hh, node, "chunk_size", strlen("chunk_size"), cs);
  HASH_FIND(hh, node, "max_chunks_up", strlen("max_chunks_up"), mc);

  uint64_t chunk_size = 4096;
  if (!cs || !cs->value || !strlen(cs->value)) {
    conf->chunk_size = chunk_size;
  } else {
    str_to_uint(cs->value, &chunk_size);
    conf->max_chunks_up = chunk_size;
  }

  uint64_t max_chunks_up = 64;
  if (!mc || !mc->value || !strlen(mc->value)) {
    conf->max_chunks_up = max_chunks_up;
  } else {
    str_to_uint(mc->value, &max_chunks_up);
    conf->max_chunks_up = max_chunks_up;
  }
  return conf;
}

storage_config * parse_storage_configuration(config_yaml_node_t * node) {
  if (!node) return NULL;
  const char * storage = "storage";
  config_yaml_node_t * out;
  HASH_FIND(hh, node, storage, strlen(storage), out);
  if (!out || !out->map) return NULL;

  return validate_storage_configuration(out->map);
}

ecu_config * parse_ecu_configuration(config_yaml_node_t * node) {
  if (!node) return NULL;

  config_yaml_node_t * ecus;
  HASH_FIND(hh, node, "ecus", strlen("ecus"), ecus);
  if (!ecus) {
    logc(err, "no ecus configured");
    return NULL;
  }

  // a table of ecu configurations
  // indexed on ecu name string
  ecu_config * ecu_confs = NULL;

  config_yaml_node_t * ecu;
  LL_FOREACH(ecus->list, ecu) {
    if (!ecu->map) {
      logc(err, "unexpected configuration format for ecu node in yaml");
      return NULL;
    }
    ecu_config * ecu_conf = (ecu_config *)malloc(sizeof(ecu_config));
    if (!parse_ecu_properties(ecu->map, ecu_conf)) {
      logc(err, "failed to parse ecu properties");
      free_ecu_configuration(ecu_conf);
      free_ecu_configuration(ecu_confs);
      return NULL;
    }

    ecu_config * duplicate;
    HASH_FIND(hh, ecu_confs, ecu_conf->name, strlen(ecu_conf->name), duplicate);
    if (duplicate) {
      logc(warn, "skipping duplicate ecu {name: %s}", ecu_conf->name);
      free_ecu_configuration(ecu_conf);
      continue;
    }

    // input and output properties are optionally
    // provided in configuration file because
    // they can be obtained from c2
    ecu_conf->input_props = parse_io_properties(ecu->map, "input");
    ecu_conf->output_props = parse_io_properties(ecu->map, "output");

    HASH_ADD_KEYPTR(hh, ecu_confs, ecu_conf->name, strlen(ecu_conf->name), ecu_conf);
  }
  return ecu_confs;
}

void free_ecu_configuration(ecu_config * config) {
  if (!config) return;
  free(config->name);
  free(config->stream_name);
  free_properties(config->input_props);
  free_properties(config->output_props);
  free(config);
}

void free_all_ecu_configuration(ecu_config * config) {
  ecu_config *el, *tmp;
  HASH_ITER(hh, config, el, tmp) {
    HASH_DELETE(hh, config, el);
    free_ecu_configuration(el);
  }
}

void free_storage_config(storage_config * config) {
  if (!config) return;
  free(config->storage_path);
  free(config->sync_mode);
}
