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

#include "yaml.h"
#include <stdio.h>
#include <core/yaml_config.h>

config_yaml_node_t * allocate_yaml_node() {
  config_yaml_node_t * node = (config_yaml_node_t *)malloc(sizeof(config_yaml_node_t));
  memset(node, 0, sizeof(config_yaml_node_t));
  return node;
}

void parse_yaml_scalar(yaml_document_t * document, yaml_node_t * yml_node, config_yaml_node_t ** node) {
  if (!(*node)) {
    config_yaml_node_t * tmp = allocate_yaml_node();
    tmp->key = strdup((char *)(yml_node->data.scalar.value));
    *node = tmp;
  } else {
     (*node)->value = strdup((char *)(yml_node->data.scalar.value));
  }
}

int parse_yaml(yaml_document_t * document, yaml_node_t * yml_node, config_yaml_node_t ** node) {
  if (!yml_node) return 0;

  yaml_node_t * next_node;
  switch (yml_node->type) {
  case YAML_SCALAR_NODE: {
    parse_yaml_scalar(document, yml_node, node);
    break;
  }
  case YAML_MAPPING_NODE: {
    yaml_node_pair_t * node_pair;
    config_yaml_node_t * sub_node = NULL;
    for (node_pair = yml_node->data.mapping.pairs.start; node_pair < yml_node->data.mapping.pairs.top; node_pair++) {
      config_yaml_node_t * pair = NULL;
      next_node = yaml_document_get_node(document, node_pair->key);
      if (!next_node || !parse_yaml(document, next_node, &pair))
        return 0;

      next_node = yaml_document_get_node(document, node_pair->value);
      if (!next_node || !parse_yaml(document, next_node, &pair))
        return 0;

      HASH_ADD_KEYPTR(hh, sub_node, pair->key, strlen(pair->key), pair);
    }

    if (!(*node)) {
      *node = sub_node;
    } else {
      (*node)->map = sub_node;
    }
    break;
  }
  case YAML_SEQUENCE_NODE: {
    yaml_node_item_t *i_node;
    config_yaml_node_t * sub_node = NULL;
    for (i_node = yml_node->data.sequence.items.start; i_node < yml_node->data.sequence.items.top; i_node++) {
      next_node = yaml_document_get_node(document, *i_node);
      if (next_node) {
        config_yaml_node_t * n = allocate_yaml_node();
        if (!parse_yaml(document, next_node, &n))
          return 0;
        LL_APPEND(sub_node, n);
      }
    }
    if (!*node) {
      *node = sub_node;
    } else {
      (*node)->list = sub_node;
    }
    break;
  }
  default:
    break;
  }
  return 1;
}

void free_config_yaml_map(config_yaml_node_t * node);

void free_config_yaml_list(config_yaml_node_t * node) {
  config_yaml_node_t * el, *tmp;
  LL_FOREACH_SAFE(node, el, tmp) {
    free_config_yaml_list(el->list);
    free_config_yaml_map(el->map);
    free(el->value);
    LL_DELETE(node, el);
    free(el);
  }
}

void free_config_yaml_map(config_yaml_node_t * node) {
  config_yaml_node_t * el, *tmp;
  HASH_ITER(hh, node, el, tmp) {
    free(el->key);
    free_config_yaml_list(el->list);
    free_config_yaml_map(el->map);
    free(el->value);
    HASH_DELETE(hh, node, el);
    free(el);
  }
}

void free_config_yaml(config_yaml_node_t * node) {
  if (!node) return;
  if (node->key)
    free_config_yaml_map(node);
  else
    free_config_yaml_list(node);
}

config_yaml_node_t * parse_yaml_configuration(const char * config_file) {
  if (!config_file) {
    return NULL;
  }

  yaml_parser_t parser;
  if (!yaml_parser_initialize(&parser))
    return NULL;

  FILE * fp = fopen(config_file, "r");
  if (!fp) {
    yaml_parser_delete(&parser);
    return NULL;
  }

  yaml_parser_set_input_file(&parser, fp);
  yaml_document_t document;

  if (!yaml_parser_load(&parser, &document)) {
    yaml_parser_delete(&parser);
    fclose(fp);
    return NULL;
   }

  yaml_node_t * yml_root = yaml_document_get_root_node(&document);
  if (!yml_root || yml_root->type == YAML_NO_NODE || yml_root->type == YAML_SCALAR_NODE) {
    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    fclose(fp);
    return NULL;
  }

  config_yaml_node_t * node = NULL;
  if (!parse_yaml(&document, yml_root, &node)) {
    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    free_config_yaml(node);
    fclose(fp);
    return NULL;
  }

  yaml_document_delete(&document);
  yaml_parser_delete(&parser);
  fclose(fp);
  return node;;
}
