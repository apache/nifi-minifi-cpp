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

#include "api/nanofi.h"
#include "core/flowfiles.h"

#include "utlist.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void add_flow_file_record(flow_file_list ** ff_list, flow_file_record * record) {
    if (!record) return;

    struct flow_file_list * new_node = (struct flow_file_list *)malloc(sizeof(struct flow_file_list));
    new_node->ff_record = record;
    LL_APPEND(*ff_list, new_node);
}

void free_flow_file_list(flow_file_list ** ff_list) {
    if (!*ff_list) return;
    flow_file_list * el = NULL;
    LL_FOREACH(*ff_list, el) {
        if (el) {
            free_flowfile(el->ff_record);
        }
    }
}

flow_file_record * write_to_flow_file(char * buff, nifi_instance * instance, standalone_processor * proc) {
    if (!buff || !instance || !proc) {
        return NULL;
    }

    flow_file_record * ffr = generate_flow_file(instance, proc);
    if (ffr == NULL) {
        printf("Could not generate flow file\n");
        return NULL;
    }

    FILE * ffp = fopen(ffr->contentLocation, "wb");
    if (!ffp) {
        printf("Cannot open flow file at path %s to write content to.\n", ffr->contentLocation);
        free_flowfile(ffr);
        return NULL;
    }

    int count = strlen(buff);
    int ret = fwrite(buff, 1, count, ffp);
    if (ret < count) {
        fclose(ffp);
        free_flowfile(ffr);
        return NULL;
    }
    fseek(ffp, 0, SEEK_END);
    ffr->size = ftell(ffp);
    fclose(ffp);
    return ffr;
}

void transmit_flow_files(nifi_instance * instance, flow_file_list * ff_list) {
    if (!instance || !ff_list) {
        return;
    }

    flow_file_list * el = NULL;
    LL_FOREACH(ff_list, el) {
        if (el) {
            transmit_flowfile(el->ff_record, instance);
        }
    }
}

uint64_t flow_files_size(flow_file_list * ff_list) {
    if (!ff_list) {
        return 0;
    }

    uint64_t counter = 0;
    flow_file_list * el = NULL;
    LL_COUNT(ff_list, el, counter);
    return counter;
}
