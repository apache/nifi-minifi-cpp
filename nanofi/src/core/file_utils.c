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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "api/nanofi.h"
#include "core/string_utils.h"
#include "core/file_utils.h"

token_list tail_file(const char * file_path, char delim, int curr_offset) {
    token_list tkn_list;
    memset(&tkn_list, 0, sizeof(struct token_list));

    if (!file_path) {
        return tkn_list;
    }

    char buff[MAX_BYTES_READ + 1];
    memset(buff, 0, MAX_BYTES_READ+1);
    errno = 0;
    FILE * fp = fopen(file_path, "rb");
    if (!fp) {
        printf("Cannot open file: {file: %s, reason: %s}\n", file_path, strerror(errno));
        return tkn_list;
    }
    fseek(fp, curr_offset, SEEK_SET);

    int bytes_read = 0;
    int i = 0;
    while ((bytes_read = fread(buff, 1, MAX_BYTES_READ, fp)) > 0) {
        buff[bytes_read] = '\0';
        struct token_list tokens = tokenize_string_tailfile(buff, delim);
        if (tokens.size > 0) {
            attach_lists(&tkn_list, &tokens);
        }
        tkn_list.total_bytes += tokens.total_bytes;
        if (tokens.total_bytes > 0) {
            curr_offset += tokens.total_bytes;
            fseek(fp, curr_offset, SEEK_SET);
        }
        memset(buff, 0, MAX_BYTES_READ);
    }
    fclose(fp);
    return tkn_list;
}
