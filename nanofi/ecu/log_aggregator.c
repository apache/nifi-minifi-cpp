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

#include "api/nanofi.h"
#include "api/ecu.h"
#include "core/string_utils.h"
#include "core/cstructs.h"
#include "core/file_utils.h"
#include "core/flowfiles.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>

int main(int argc, char** argv) {

    if (argc < 6) {
        printf("Error: must run ./log_aggregator <file> <interval> <delimiter> <nifi instance url> <remote port>\n");
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

    if (errno == ERANGE || intrvl == LONG_MAX || intrvl == LONG_MIN) {
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

    const char * processor_name = "TailFile";

    add_custom_processor(processor_name, on_trigger_logaggregator);

    proc = create_processor(processor_name);

    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "delimiter", delimiter);

    set_offset(0);
    while (!stopped) {
        flow_file_record * new_ff = invoke(proc);
        transmit_flow_files(instance, ff_list);
        free_flow_file_list(ff_list);
        free_flowfile(new_ff);
        sleep(intrvl);
    }

    printf("tail file processor stopped\n");
    free_standalone_processor(proc);
    free_instance(instance);

    return 0;
}
