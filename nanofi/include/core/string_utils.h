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

#define MAX_BYTES_READ 4096

/**
 * Tokenizes a delimited string and returns a list of tokens
 * @param str the string to be tokenized
 * @param delim the delimiting character
 * @return a list of tokens
 */
token_list tokenize_string(const char * str, char delim);

/**
 * Tokenizes a delimited string and returns a list of tokens but excludes
 * the last token if it is not delimited by the delimiter. This function
 * is used by tailfile processor
 * @param str the cstring to tokenize
 * @param delim the delimiter to tokenize by
 * @return a list of tokens
 */
token_list tokenize_string_tailfile(const char * str, char delim);

/**
 * Adds a token to the token list
 * @param tks the token list to add the token to
 * @param begin the beginning of the token
 * @param len the length of the token
 */
void add_token_to_list(token_list * tks, const char * begin, uint64_t len);

/**
 * Deallocate one token node
 * @param node the node in the list to be deallocated
 */
void free_token_node(token_node * node);

/**
 * Deallocate the dynamically allocated token list
 * @param tks the token list to be freed
 */
void free_all_tokens(token_list * tks);

/**
 * Remove the tail node from the list
 * @param tks the linked list of token nodes
 */
void remove_last_node(token_list * tks);

/**
 * Validate a linked list
 * @param tks_list the list to be validated
 * @return 1 if the list if valid else 0
 */
int validate_list(token_list * tk_list);

/**
 * Append one list to other (Chaining)
 * @param to, the destination list to append to
 * @param from, the source list
 * @attention, if the to list is empty, to and from will be same after appending
 */
void attach_lists(token_list * to, token_list * from);

#ifdef __cplusplus
}
#endif

#endif //NIFI_MINIFI_CPP_STRING_UTILS_H
