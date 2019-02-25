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
#include "coap_server.h"
#include "coap_functions.h"
/**
 * Create a new CoAPServer
 */
CoapServerContext * const create_server(const char * const server_hostname, const char * const port) {
  CoapServerContext *server = (CoapServerContext*) malloc(sizeof(CoapServerContext));
  memset(server, 0x00, sizeof(CoapServerContext));
  if (create_endpoint_context(&server->ctx, server_hostname, port)) {
    free_server(server);
  }

  return server;
}

CoapEndpoint * const create_endpoint(CoapServerContext * const server, const char * const resource_path, uint8_t method, coap_method_handler_t handler) {
  CoapEndpoint *endpoint = (CoapEndpoint*) malloc(sizeof(CoapEndpoint));
  memset(endpoint, 0x00, sizeof(CoapEndpoint));
  endpoint->server = server;
  int8_t flags = COAP_RESOURCE_FLAGS_NOTIFY_CON;
  coap_str_const_t *path = NULL;
  if (NULL != resource_path) {
    path = coap_new_str_const((const uint8_t *) resource_path, strlen(resource_path));
  }
  endpoint->resource = coap_resource_init(path, flags);
  coap_add_attr(endpoint->resource, coap_make_str_const("title"), coap_make_str_const("\"Created CoapEndpoint\""), 0);
  if ( add_endpoint(endpoint, method, handler) ){
    return 0x00;
  }
  coap_add_resource(server->ctx, endpoint->resource);
  if (path) {
    coap_delete_str_const(path);
  }
  return endpoint;

}

int8_t add_endpoint(CoapEndpoint * const endpoint, uint8_t method, coap_method_handler_t handler) {
  if (endpoint == NULL || handler == NULL)
    return -1;

  coap_register_handler(endpoint->resource, method, handler);
  return 0;
}

/**
 * FRee the CoAP messages that are provided.
 */
void free_endpoint(CoapEndpoint * const endpoint) {
  if (endpoint) {
    free((void*) endpoint);
  }
}
void free_server(CoapServerContext * const server) {
  if (server) {
    if (server->ctx) {
      coap_delete_all_resources(server->ctx);
      coap_free_context(server->ctx);
    }
    free(server);
  }
}
