/* Licensed to the Apache Software Foundation (ASF) under one or more
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
 * This is an example of the C API that registers terminate handler and generates an exception.
 */

void example_terminate_handler() {
  fprintf(stderr, "Unhandled exception! Let's pretend that this is normal!");
  exit(0);
}

int main(int argc, char **argv) {

  nifi_port port;

  port.port_id = "12345";

  set_terminate_callback(example_terminate_handler);

  nifi_instance *instance = create_instance("random instance", &port);

  flow *new_flow = create_new_flow(instance);

  processor *generate_proc = add_processor(new_flow, "GenerateFlowFile");

  processor *put_proc = add_processor(new_flow, "PutFile");

  // Target directory for PutFile is missing, it's not allowed to create, so tries to transmit to failure relationship
  // As it doesn't exist, an exception is thrown
  set_property(put_proc, "Directory", "/tmp/never_existed");
  set_property(put_proc, "Create Missing Directories", "false");

  flow_file_record *record = get_next_flow_file(instance, new_flow );

  // Here be dragons - nothing below this line gets executed
  fprintf(stderr, "Dragons!!!");
  free_flowfile(record);
  free_flow(new_flow);
  free_instance(instance);
}
