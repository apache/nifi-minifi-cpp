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

tokens tokenize_string(const char * str, char delim, tokenizer_mode_t mode) {
    tokens tks;
    tks.num_strings = 0;
    tks.total_bytes = 0;

    if (!str) return tks;

    char * begin = (char *)str;
    char * end = NULL;
    int num_strings = 0;
    while ((end = strchr(begin, delim))) {
        if (begin == end) {
            begin++;
            continue;
        }
        begin = (end+1);
        num_strings++;
    }

    if (mode == DEFAULT_MODE && (*begin != '\0')) {
        num_strings++;
    }

    tks.str_list = calloc(num_strings, sizeof(char *));
    tks.num_strings = 0;
    tks.total_bytes = 0;

    begin = (char *)str;
    end = NULL;
    while ((end = strchr(begin, delim))) {
        if (begin == end) {
            begin++;
            tks.total_bytes++;
            continue;
        }
        int len = end - begin;
        char * substr = (char *)malloc((len+1) * sizeof(char));
        strncpy(substr, begin, len);
        substr[len] = '\0';
        tks.str_list[tks.num_strings++] = substr;
        tks.total_bytes += (len+1);
        begin = (end+1);
    }

    if (mode == DEFAULT_MODE && (*begin != '\0')) {
        int len = strlen(begin);
        char * substr = (char *)malloc((len+1) * sizeof(char));
        strncpy(substr, begin, len);
        substr[len] = '\0';
        tks.str_list[tks.num_strings++] = substr;
        tks.total_bytes += (len+1);
    }
    return tks;
}

void free_tokens(tokens * tks) {
    if (tks) {
        int i;
        for (i = 0; i < tks->num_strings; ++i) {
            free(tks->str_list[i]);
        }
    }
}
