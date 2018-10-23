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
#ifndef EXTENSIONS_COAP_NANOFI_COAP_SERVER_H
#define EXTENSIONS_COAP_NANOFI_COAP_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <coap2/coap.h>

#include <stdint.h>
#include <stdio.h>

/**
 * CoAP-2 in libcoap uses uint8_t *  while the first version uses a different type, so we will have to cast
 * the data. We have to keep this in mind with the API that we use.
 */
typedef struct {
  struct coap_context_t* ctx;

  coap_address_t src_addr;
  coap_optlist_t *optlist;

} CoAPServer;


typedef struct {
  CoAPServer *server;
  coap_resource_t *resource;
  //coap_method_handler_t handler;
} CoAPEndpoint;


/**
 * Create a new CoAPServer using the host name provide
 * @param server_hostname hostname
 * @param port port requested
 * @param title title of base resource
 * @return CoAPServer structure.
 */
CoAPServer * const create_server(const char *const server_hostname, const char * const port);

CoAPEndpoint * const create_endpoint(CoAPServer * const, const char * const resource_path, uint8_t method, coap_method_handler_t handler);

int8_t add_endpoint(CoAPEndpoint * const endpoint, uint8_t method, coap_method_handler_t handler);


/**
 * FRee the CoAP messages that are provided.
 */
void free_endpoint(CoAPEndpoint * const);
void free_server(CoAPServer * const);

#ifdef __cplusplus
}
#endif

#endif /* EXTENSIONS_COAP_NANOFI_COAP_SERVER_H */
