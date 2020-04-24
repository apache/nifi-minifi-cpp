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
#include <sys/stat.h>
#include <inttypes.h>

flow_file_list * add_flow_file_record(flow_file_list ** ff_list, flow_file_record * record) {
    if (!record) {
        return *ff_list;
    }

    struct flow_file_list * new_node = (struct flow_file_list *)malloc(sizeof(struct flow_file_list));
    new_node->ff_record = record;
    LL_APPEND(*ff_list, new_node);
    return new_node;
}

void free_flow_file_list(flow_file_list ** ff_list) {
    if (!*ff_list) {
        return;
    }
    flow_file_list * head = *ff_list;
    while (head) {
       flow_file_list * tmp = head;
       free_flowfile(tmp->ff_record);
       head = head->next;
       free(tmp);
    }
}

void add_attributes(flow_file_record * ffr, const char * file_path, uint64_t curr_offset) {
    char offset_str[21];
    snprintf(offset_str, sizeof(offset_str), "%"PRIu64, curr_offset);
    add_attribute(ffr, "current offset", offset_str, strlen(offset_str));
    char content_location[strlen(ffr->contentLocation) + 1];
    snprintf(content_location, sizeof(content_location), "%s", ffr->contentLocation);
    add_attribute(ffr, "content location", content_location, strlen(content_location));
    add_attribute(ffr, "tailfile path", (char*)file_path, strlen(file_path));
}

void update_attributes(flow_file_record * ffr, const char * file_path, uint64_t curr_offset) {
    char offset_str[21];
    snprintf(offset_str, sizeof(offset_str), "%"PRIu64, curr_offset);
    update_attribute(ffr, "current offset", offset_str, strlen(offset_str));
    char content_location[strlen(ffr->contentLocation) + 1];
    snprintf(content_location, sizeof(content_location), "%s", ffr->contentLocation);
    update_attribute(ffr, "content location", content_location, strlen(content_location));
    update_attribute(ffr, "tailfile path", (char*)file_path, strlen(file_path));
}

void transmit_flow_files(nifi_instance * instance, flow_file_list * ff_list, int complete) {
    if (!instance || !ff_list) {
        return;
    }
    flow_file_list * el = NULL;
    LL_FOREACH(ff_list, el) {
        if (!complete || el->complete) {
            transmit_flowfile(el->ff_record, instance);
        }
    }
}

void read_payload_and_transmit(struct flow_file_list * ffl, struct CRawSiteToSiteClient * client) {
    if (!ffl || !client) {
        return;
    }

    char * file = ffl->ff_record->contentLocation;
    FILE * fp = fopen(file, "rb");
    if (!fp) {
        return;
    }

    struct stat statfs;
    if (stat(file, &statfs) < 0) {
        return;
    }
    size_t file_size = statfs.st_size;

    attribute attr;
    attr.key = "current offset";
    if (get_attribute(ffl->ff_record, &attr) < 0) {
        printf("Error looking up flow file attribute %s\n", attr.key);
        return;
    }

    errno = 0;
    uint64_t offset = strtoull((const char *)attr.value, NULL, 10);
    if (errno != 0) {
        printf("Error converting flow file offset value\n");
        return;
    }
    uint64_t begin_offset =  offset - file_size;
    char * buff = (char *)malloc(sizeof(char) * 4097);
    size_t count = 0;
    while ((count = fread(buff, 1, 4096, fp)) > 0) {
        buff[count] = '\0';
        begin_offset += count;
        char offset_str[21];
        snprintf(offset_str, sizeof(offset_str), "%"PRIu64, begin_offset);
        update_attribute(ffl->ff_record, "current offset", offset_str, strlen(offset_str));

        attribute_set as;
        uint64_t num_attrs = get_attribute_quantity(ffl->ff_record);
        as.size = num_attrs;
        as.attributes = (attribute *)malloc(num_attrs * sizeof(attribute));
        get_all_attributes(ffl->ff_record, &as);

        if (transmitPayload(client, buff, &as) == 0) {
            printf("payload of %zu bytes from %s sent successfully\n", count, ffl->ff_record->contentLocation);
        }
        else {
            printf("Failed to send payload, flow file %s\n", ffl->ff_record->contentLocation);
        }
        free(as.attributes);
    }
    free(buff);
    fclose(fp);
}

void transmit_payload(struct CRawSiteToSiteClient * client, struct flow_file_list * ff_list, int complete) {
    if (!client || !ff_list) {
        return;
    }
    flow_file_list * el = NULL;
    LL_FOREACH(ff_list, el) {
        if (!complete || el->complete) {
            read_payload_and_transmit(el, client);
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
