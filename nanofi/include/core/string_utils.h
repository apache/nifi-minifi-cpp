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


#ifndef NIFI_MINIFI_CPP_STRING_UTILS_H
#define NIFI_MINIFI_CPP_STRING_UTILS_H

#include "cstructs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum TOKENIZER_MODE {
    TAILFILE_MODE = 0, /* Do not include a non delimiting string */
    DEFAULT_MODE /* include a non delimiting string */
} tokenizer_mode_t;

/**
 * Tokenizes a delimited string and returns a list of tokens
 * @param str the string to be tokenized
 * @param delim the delimiting character
 * @param tokenizer_mode_t the enumeration value specified to include/exclude a non delimiting string in the result
 * @return a list of strings wrapped inside tokens struct
 */
tokens tokenize_string(const char * str, char delim, tokenizer_mode_t);

/**
 * Free the dynamically allocated tokens
 * @param tks the tokens to be freed
 */
void free_tokens(tokens * tks);

#ifdef __cplusplus
}
#endif

#endif //NIFI_MINIFI_CPP_STRING_UTILS_H
