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
#include <sys/types.h>
#include <sys/stat.h>

#include "api/nanofi.h"

/**
 * This is an example of the C API that transmits a flow file to a remote instance.
 */
int main(int argc, char **argv) {

  if (argc < 3) {
    printf("Error: must run ./generate_flow <instance> <remote port> \n");
    exit(1);
  }

  char *instance_str = argv[1];
  char *portStr = argv[2];

  nifi_port port;

  port.port_id = portStr;

  nifi_instance *instance = create_instance(instance_str, &port);

  flow *new_flow = create_new_flow(instance);
  processor *generate_proc = add_processor(new_flow, "GenerateFlowFile");

  flow_file_record *record = get_next_flow_file(instance, new_flow);

  if (record == 0) {
    printf("Could not create flow file");
    exit(1);
  }

  transmit_flowfile(record, instance);

  free_flowfile(record);

  // initializing will make the transmission slightly more efficient.
  //initialize_instance(instance);
  //transfer_file_or_directory(instance,file);

  free_flow(new_flow);

  free_instance(instance);
}
