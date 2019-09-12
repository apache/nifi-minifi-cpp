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

#ifndef NANOFI_INCLUDE_CORE_RING_BUFFER_H_
#define NANOFI_INCLUDE_CORE_RING_BUFFER_H_

#include <string.h>

typedef struct ring_buffer {
    char * data;
    size_t size;
    size_t capacity;
    size_t read_index;
    size_t write_index;
} ring_buffer_t;

size_t write_ring_buffer(ring_buffer_t * rb, const char * payload, size_t length);
size_t read_ring_buffer(ring_buffer_t * rb, char * payload, size_t length);
size_t check_available_space(ring_buffer_t * rb);
void free_ring_buffer(ring_buffer_t * rb);

#endif /* NANOFI_INCLUDE_CORE_RING_BUFFER_H_ */
