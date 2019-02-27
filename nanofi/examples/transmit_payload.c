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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api/nanofi.h"
#include "core/log.h"
#include "sitetosite/CPeer.h"
#include "sitetosite/CRawSocketProtocol.h"

int main(int argc, char **argv) {

  if (argc < 4) {
    printf("Error: must run ./transmit_payload <host> <port> <nifi port>\n");
    exit(1);
  }

  set_log_level(info);

  char *host = argv[1];
  char *port_str = argv[2];
  char *nifi_port_str = argv[3];

  uint16_t port_num = atoi(port_str);

  struct CRawSiteToSiteClient * client = createClient(host, port_num, nifi_port_str);

  const char * payload = "Test MiNiFi payload";

  attribute attribute1;

  attribute1.key = "some_key";
  const char * attr_value = "some value";
  attribute1.value = (void *)attr_value;
  attribute1.value_size = strlen(attr_value);

  attribute_set as;
  as.size = 1;
  as.attributes = &attribute1;

  if(transmitPayload(client, payload, &as) == 0){
    printf("Packet successfully sent\n");
  } else {
    printf("Failed to send packet\n");
  }

  destroyClient(client);

  return 0;
}