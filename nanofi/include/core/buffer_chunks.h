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

#ifndef NANOFI_INCLUDE_CORE_BUFFER_CHUNKS_H_
#define NANOFI_INCLUDE_CORE_BUFFER_CHUNKS_H_

#include "cstructs.h"
#include "uthash.h"
#include "utlist.h"

typedef struct chunk {
    char * buffer; //chunk data
    uint64_t len; //chunk size
    attribute_set as; //attributes associated with the data
    struct chunk * next;
} chunk_t;

typedef struct stream {
    chunk_t * ct; //the list of chunks in this stream
    uint64_t total_size; //the total amount of bytes stored in memory
} stream_t;

/**
 * Create a directory at the specified path
 * @param file_path the path in file system
 * @return returns a pointer to initialized storage
 */
stream_t * create_stream();

/**
 * Adds a chunk to the list of chunks
 * @param strg, the storage to append the chunk to
 * @param buffer, the data to write to chunk
 * @param len, the length of the buffer
 * @return pointer to the chunk added
 */
chunk_t * add_chunk(stream_t * strm, const char * buffer, size_t len);

/**
 * Adds an attribute set to a chunk
 * @param chunk, a pointer to the chunk in chunk list
 * @param as, the attribute set to add
 */
void add_chunk_attributes(chunk_t * chunk, attribute_set as);

/**
 * Move the list of chunks from source to destination
 * After move, the source chunk list will be empty
 * and total_size will be zero
 */
void move_chunks(stream_t * source, stream_t * dest);

/**
 * Read the chunks from storage into memory
 * skip reading those already in memory
 * @param strm, the stream to get chunks from
 */
void get_chunks(stream_t * strm);

#endif /* NANOFI_INCLUDE_CORE_BUFFER_CHUNKS_H_ */
