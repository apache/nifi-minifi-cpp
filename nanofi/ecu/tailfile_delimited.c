/*
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

#include "api/ecu.h"
#include "core/flowfiles.h"
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char** argv) {

    if (argc < 6) {
        printf("Error: must run ./tailfile_delimited <file> <interval> <delimiter> <nifi instance url> <remote port>\n");
        exit(1);
    }

    char * file = argv[1];
    char * interval = argv[2];
    char * delimiter = argv[3];
    char * instance_str = argv[4];
    char * port_str = argv[5];

    if (access(file, F_OK) == -1) {
        printf("Error: %s doesn't exist!\n", file);
        exit(1);
    }

    struct stat stats;
    errno = 0;
    int ret = stat(file, &stats);

    if (ret == -1) {
        printf("Error occurred while getting file status {file: %s, error: %s}\n", file, strerror(errno));
        exit(1);
    }
    // Check for file existence
    if (S_ISDIR(stats.st_mode)){
        printf("Error: %s is a directory!\n", file);
        exit(1);
    }

    errno = 0;
    unsigned long intrvl = strtol(interval, NULL, 10);

    if (errno != 0) {
        printf("Invalid interval value specified\n");
        return 0;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(sigaction));
    action.sa_handler = signal_handler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    nifi_port port;

    port.port_id = port_str;

    instance = create_instance(instance_str, &port);

    const char * processor_name = "TailFileDelimited";

    add_custom_processor(processor_name, on_trigger_tailfiledelimited);

    proc = create_processor(processor_name);

    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "delimiter", delimiter);

    curr_offset = 0;
    while (!stopped) {
        flow_file_record * new_ff = invoke(proc);
        transmit_flow_files(instance, &ff_list);
        free_flow_file_list(&ff_list);
        free_flowfile(new_ff);
        sleep(intrvl);
    }

    printf("processor stopped\n");
    free_standalone_processor(proc);
    free_instance(instance);

    return 0;
}
