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
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>

#include "api/nanofi.h"
#include "blocks/file_blocks.h"
#include "blocks/comms.h"
#include "core/processors.h"
int is_dir(const char *path) {
  struct stat stat_struct;
  if (stat(path, &stat_struct) != 0)
    return 0;
  return S_ISDIR(stat_struct.st_mode);
}

pthread_mutex_t mutex;
pthread_cond_t condition;

int stopped;

int stop_callback(char *c) {
  pthread_mutex_lock(&mutex);
  stopped = 1;
  pthread_cond_signal(&condition);
  pthread_mutex_unlock(&mutex);
  return 0;
}

int is_stopped(void *ptr) {
  int is_stop = 0;
  pthread_mutex_lock(&mutex);
  is_stop = stopped;
  pthread_mutex_unlock(&mutex);
  return is_stop;
}

/**
 * This is an example of the C API that transmits a flow file to a remote instance.
 */
int main(int argc, char **argv) {
  if (argc < 5) {
    printf("Error: must run ./monitor_directory <instance> <remote port> <directory to monitor>\n");
    exit(1);
  }

  stopped = 0x00;

  char *instance_str = argv[1];
  char *portStr = argv[2];
  char *directory = argv[3];

  nifi_port port;

  port.port_id = portStr;

  C2_Server server;
  server.url = argv[4];
  server.ack_url = argv[5];
  server.identifier = "monitor_directory";
  server.type = REST;

  nifi_instance *instance = create_instance(instance_str, &port);

  // enable_async_c2(instance, &server, stop_callback, NULL, NULL);

  flow *new_flow = monitor_directory(instance, directory, 0x00, KEEP_SOURCE);

  transmit_to_nifi(instance, new_flow, is_stopped);

  free_flow(new_flow);

  free_instance(instance);
}
