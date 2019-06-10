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

#include "core/flowfiles.h"
#include <string.h>

void add_flow_file_record(flow_file_list * ff_list, flow_file_record * record) {
    if (!ff_list || !record) return;

    struct flow_file_list_node * new_node = (struct flow_file_list_node *)malloc(sizeof(struct flow_file_list_node));
    new_node->ff_record = record;
    new_node->next = NULL;

    if (!ff_list->head || !ff_list->tail) {
        ff_list->head = ff_list->tail = new_node;
        ff_list->len = 1;
        return;
    }

    ff_list->tail->next = new_node;
    ff_list->tail = new_node;
    ff_list->len++;
}

void free_flow_file_list(flow_file_list * ff_list) {
    if (!ff_list || !ff_list->head) {
        return;
    }

    flow_file_list_node * head = ff_list->head;
    while (head) {
        free_flowfile(head->ff_record);
        flow_file_list_node * tmp = head;
        head = head->next;
        free(tmp);
    }
    memset(ff_list, 0, sizeof(struct flow_file_list));
}
