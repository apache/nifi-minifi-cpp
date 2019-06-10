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
#include "core/string_utils.h"
#include "core/cstructs.h"
#include "core/file_utils.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>

struct flow_file_records * flowfiles = NULL;
nifi_instance * instance = NULL;
standalone_processor * proc = NULL;
int file_offset = 0;
int stopped = 0;
flow_file_list ff_list;
token_list tks;

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        stopped = 1;
    }
}

void transmit_flow_files(nifi_instance * instance) {
    flow_file_list_node * head = ff_list.head;
    while (head) {
        transmit_flowfile(head->ff_record, instance);
        head = head->next;
    }
}

void set_offset(int offset) {
    file_offset = offset;
}

int get_offset() {
    return file_offset;
}

void on_trigger_callback(processor_session * ps, processor_context * ctx) {

    char file_path[4096];
    char delimiter[3];

    if (get_property(ctx, "file_path", file_path, sizeof(file_path)) != 0) {
        return;
    }

    if (get_property(ctx, "delimiter", delimiter, sizeof(delimiter)) != 0) {
        return;
    }

    if (strlen(delimiter) == 0) {
        printf("Delimiter not specified or it is empty\n");
        return;
    }
    char delim = delimiter[0];

    if (delim == '\\') {
          if (strlen(delimiter) > 1) {
            switch (delimiter[1]) {
              case 'r':
                delim = '\r';
                break;
              case 't':
                delim = '\t';
                break;
              case 'n':
                delim = '\n';
                break;
              case '\\':
                delim = '\\';
                break;
              default:
                break;
            }
        }
    }

    tks = tail_file(file_path, delim, get_offset());

    if (!validate_list(&tks)) return;

    set_offset(get_offset() + tks.total_bytes);

    token_node * head;
    for (head = tks.head; head && head->data; head = head->next) {
        flow_file_record * ffr = generate_flow_file(instance, proc);
        const char * flow_file_path = ffr->contentLocation;
        FILE * ffp = fopen(flow_file_path, "wb");
        if (!ffp) {
            printf("Cannot open flow file at path %s to write content to.\n", flow_file_path);
            break;
        }

        int count = strlen(head->data);
        int ret = fwrite(head->data, 1, count, ffp);
        if (ret < count) {
            fclose(ffp);
            break;
        }
        fseek(ffp, 0, SEEK_END);
        ffr->size = ftell(ffp);
        fclose(ffp);
        add_flow_file_record(&ff_list, ffr);
    }
    free_all_tokens(&tks);
}

int main(int argc, char** argv) {

    if (argc < 6) {
        printf("Error: must run ./tail_file <file> <interval> <delimiter> <nifi instance url> <remote port>\n");
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

    add_custom_processor(processor_name, on_trigger_callback);

    proc = create_processor(processor_name);

    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "delimiter", delimiter);

    set_offset(0);
    while (!stopped) {
        flow_file_record * new_ff = invoke(proc);
        transmit_flow_files(instance);
        free_flow_file_list(&ff_list);
        free_flowfile(new_ff);
        sleep(intrvl);
    }

    printf("tail file processor stopped\n");
    free_standalone_processor(proc);
    free(instance);

    return 0;
}
