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

#include "core/cstructs.h"
#include "core/string_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

int validate_list(struct token_list * tk_list) {
    if (tk_list && tk_list->head && tk_list->tail && tk_list->size > 0) {
        return 1;
    }
    return 0;
}

void add_token_to_list(struct token_list * tk_list, const char * begin, uint64_t len) {
    struct token_node * new_node = (struct token_node *)malloc(sizeof(struct token_node));
    new_node->data = (char *)malloc((len+1) * sizeof(char));
    strncpy(new_node->data, begin, len);
    new_node->data[len] = '\0';
    new_node->next = NULL;

    if (!tk_list->head) {
        tk_list->head = tk_list->tail = new_node;
        tk_list->size++;
        tk_list->total_bytes += len;
        return;
    }

    tk_list->tail->next = new_node;
    tk_list->tail = new_node;
    tk_list->size++;
    tk_list->total_bytes += len;
}

void free_token_node(struct token_node * node) {
    if (node) {
        free(node->data);
    }
    free(node);
}

void free_all_tokens(struct token_list * tks) {
    while (tks && tks->head) {
        struct token_node * node = tks->head;
        tks->head = tks->head->next;
        free_token_node(node);
    }
}

void print_token_list(token_list * tokens) {
    if (tokens) {
        token_node * head = tokens->head;
        int i = 0;
        while (head) {
            printf("Token %d : %s Length = %lu\n", i, head->data, strlen(head->data));
            head = head->next;
            ++i;
        }
    }
}

void remove_last_node(token_list * tks) {
    if (!validate_list(tks)) {
        return;
    }

    if (tks->size == 1 || tks->head == tks->tail) {
        tks->total_bytes -= strlen(tks->tail->data);
        free_all_tokens(tks);
        tks->head = NULL;
        tks->tail = NULL;
        tks->size = 0;
        return;
    }

    struct token_node * tmp_head = tks->head;
    struct token_node * tmp_tail = tks->tail;

    while (tmp_head->next && (tmp_head->next != tmp_tail)) {
        tmp_head = tmp_head->next;
    }

    struct token_node * tail_node = tmp_tail;
    tks->tail = tmp_head;
    tks->tail->next = NULL;

    tks->size--;
    tks->total_bytes -= (strlen(tail_node->data));
    free_token_node(tail_node);
}

void attach_lists(token_list * to, token_list * from) {
    if (to && validate_list(from)) {
        if (!to->head) {
            to->head = from->head;
            to->tail = from->tail;
            to->size += from->size;
            return;
        }

        if (!to->tail) return;

        to->tail->next = from->head;
        to->tail = from->tail;
        to->size += from->size;
    }
}

token_list tokenize_string(const char * begin, char delim) {
    token_list tks;
    memset(&tks, 0, sizeof(struct token_list));

    if (!begin) return tks;

    const char * end = NULL;

    while ((end = strchr(begin, delim))) {
        if (begin == end) {
            begin++;
            tks.total_bytes++;
            continue;
        }
        int len = end - begin;
        add_token_to_list(&tks, begin, len);
        tks.total_bytes++;
        begin = (end+1);
    }

    if (begin && *begin != '\0') {
        int len = strlen(begin);
        if (len < MAX_BYTES_READ) {
            tks.has_non_delimited_token = 1;
        }
        add_token_to_list(&tks, begin, len);
    }

    return tks;
}

token_list tokenize_string_tailfile(const char * str, char delim) {
    token_list tks = tokenize_string(str, delim);
    if (tks.has_non_delimited_token) {
        remove_last_node(&tks);
    }
    tks.has_non_delimited_token = 0;
    return tks;
}

void copynstr(unsigned char * source, size_t len, char * dest) {
    if (!source || !len || !dest) return;
    strncpy(dest, source, len);
    dest[len] = '\0';
}

void copystr(const char * source, char ** dest) {
    if (!source || !dest) return;
    size_t len = strlen(source);
    if (!(*dest)) {
        *dest =  (char *)malloc(len + 1);
        memset(*dest, 0, len + 1);
    }
    strcpy(*dest, source);
}

int str_to_uint(const char * input_str, uint64_t * out) {
    if (!input_str) {
        return -1;
    }
    errno = 0;
    *out = (uint64_t)(strtoul(input_str, NULL, 10));
    if (errno != 0) {
        return -1;
    }
    return 0;
}

const char * uint_to_str(uint64_t value) {
    char value_str[21];
    snprintf(value_str, sizeof(value_str), "%llu", value);
    char * copy = NULL;
    copystr(value_str, &copy);
    return copy;
}

char ** parse_tokens(const char * str, size_t len, size_t num_tokens, const char * sep) {
    char * arg = (char *)malloc(len + 1);
    strcpy(arg, str);
    char * save_ptr;
#ifdef WIN32
    char * tok = strtok_s(arg, sep, &save_ptr);
#else
    char * tok = strtok_r(arg, sep, &save_ptr);
#endif
    char ** tokens = (char **)malloc(sizeof(char *) * num_tokens);
    memset(tokens, 0, sizeof(char *) * num_tokens);
    int i = 0;
    while (tok) {
        if (i > num_tokens) break;
        size_t s = strlen(tok);
        char * token = (char *)malloc(sizeof(char) * (s + 1));
        memset(token, 0, (s + 1));
        strcpy(token, tok);
        tokens[i++] = token;
#ifdef WIN32
        tok = strtok_s(NULL, sep, &save_ptr);
#else
        tok = strtok_r(NULL, sep, &save_ptr);
#endif
    }
    free(arg);
    return tokens;
}
