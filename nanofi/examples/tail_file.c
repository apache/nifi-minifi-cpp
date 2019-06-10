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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>

typedef struct flow_file_records {
    flow_file_record ** records;
    uint64_t len;
} flow_file_records;

struct flow_file_records * flowfiles = NULL;
nifi_instance * instance = NULL;
standalone_processor * proc = NULL;
int file_offset = 0;
int stopped = 0;

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        stopped = 1;
    }
}

void transmit_flow_files(nifi_instance * instance) {
    NULL_CHECK( ,flowfiles);
    int i;
    for (i = 0; i < flowfiles->len; ++i) {
        NULL_CHECK( ,flowfiles->records[i]);
        transmit_flowfile(flowfiles->records[i], instance);
    }
}

void free_flow_file_records() {
    NULL_CHECK( ,flowfiles);
    int i;
    for (i = 0; i < flowfiles->len; ++i) {
        free_flowfile(flowfiles->records[i]);
    }
    free(flowfiles);
    flowfiles = NULL;
}

void set_offset(int offset) {
    file_offset = offset;
}

int get_offset() {
    return file_offset;
}

void free_all_strings(char ** strings, int num_strings) {
    int i;
    for (i = 0; i < num_strings; ++i) {
        free(strings[i]);
    }
}

void on_trigger_callback(processor_session * ps, processor_context * ctx) {

    char file_path[4096];
    char delimiter[2];

    if (get_property(ctx, "file_path", file_path, 50) != 0) {
        return;
    }

    if (get_property(ctx, "delimiter", delimiter, 2) != 0) {
        return;
    }

    if (strlen(delimiter) == 0) {
        printf("Delimiter not specified or it is empty\n");
        return;
    }
    char delim = '\0';
    if (strlen(delimiter) > 0) {
        delim = delimiter[0];
    }

    if (delim == '\0') {
        printf("Invalid delimiter \n");
        return;
    }

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

    int curr_offset = get_offset();
    int max_bytes_read = 4096;
    char buff[max_bytes_read + 1];
    memset(buff,'\0', max_bytes_read);
    FILE * fp = fopen(file_path, "rb");
    if (!fp) return;
    fseek(fp, curr_offset, SEEK_SET);

    int bytes_read = 0;
    while ((bytes_read = fread(buff, 1, max_bytes_read, fp)) > 0) {
        buff[bytes_read] = '\0';
        tokenizer_mode_t mode = TAILFILE_MODE;
        struct tokens tks = tokenize_string(buff, delim, mode);

        if (tks.num_strings == 0) return;

        set_offset(get_offset() + tks.total_bytes);

        flowfiles = (flow_file_records *)malloc(sizeof(flow_file_records));
        flowfiles->records = malloc(sizeof(flow_file_record *) * tks.num_strings);
        flowfiles->len = tks.num_strings;

        int i;
        for (i = 0; i < tks.num_strings; ++i) {
            flowfiles->records[i] = NULL;
        }

        for (i = 0; i < tks.num_strings; ++i) {
            if (tks.str_list[i] && strlen(tks.str_list[i]) > 0) {
                flow_file_record * ffr = generate_flow_file(instance, proc);
                const char * flow_file_path = ffr->contentLocation;
                FILE * ffp = fopen(flow_file_path, "wb");
                if (!ffp) {
                    printf("Cannot open flow file at path %s to write content to.\n", flow_file_path);
                    fclose(fp);
                    free_tokens(&tks);
                    return;
                }
                int count = strlen(tks.str_list[i]);
                int ret = fwrite(tks.str_list[i], 1, count, ffp);
                if (ret < count) {
                    fclose(ffp);
                    return;
                }
                fseek(ffp, 0, SEEK_END);
                ffr->size = ftell(ffp);
                fclose(ffp);
                flowfiles->records[i] = ffr;
            }
        }
        free_tokens(&tks);
    }
    fclose(fp);
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
    int ret = stat(file, &stats);

    errno = 0;
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
        free_flow_file_records();
        free_flowfile(new_ff);
        sleep(intrvl);
    }

    free_standalone_processor(proc);
    free(instance);

    return 0;
}
