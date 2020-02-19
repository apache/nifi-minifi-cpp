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

#ifndef NANOFI_INCLUDE_CORE_STORAGEFS_H_
#define NANOFI_INCLUDE_CORE_STORAGEFS_H_

#include "cstructs.h"
#include "uthash.h"
#include "utlist.h"

typedef struct chunk {
    char * buffer; //chunk data
    uint64_t len; //chunk size
    attribute_set as; //attributes associated with the data
    char path[21]; // the relative path of the chunk in file system
    struct chunk * next;
} chunks_t;

typedef struct storage {
    char uuid[37]; //the uuid of this stream
    char * fs_path; //the root path of this stream
    chunks_t * ct; //the list of chunks in this stream
    uint64_t total_size; //the total bytes stored in memory
    uint64_t last_chunk_number; //the number of last chunk stored
} storage_t;

/**
 * Create a directory at the specified path
 * @param file_path the path in file system
 * @return returns a pointer to initialized storage
 */
storage_t * create_storage(const char * root_path);

/**
 * Adds a chunk to the list of chunks
 * @param strg, the storage to append the chunk to
 * @param buffer, the data to write to chunk
 * @param len, the length of the buffer
 * @return pointer to the chunk added
 */
chunks_t * add_chunk(storage_t * strm, const char * buffer, size_t len);

/**
 * Writes a chunk to the file system
 */
int write_chunk_to_storage(storage_t * strg, chunks_t * ck);

/**
 * Adds an attribute set to a chunk
 * @param chunk, a pointer to the chunk in chunk list
 * @param as, the attribute set to add
 */
void add_chunk_attributes(chunks_t * chunk, attribute_set as);

/**
 * Move the list of chunks from source to destination
 * After move, the source chunk list will be empty
 * and total_size will be zero
 */
int move_chunks(storage_t * source, storage_t * dest);

/**
 * Read all chunks from storage into memory
 * @param strm, the stream to get chunks from
 */
void read_chunks_up(storage_t * strm);

/**
 * Write all chunks to storage
 */
void write_chunks_down(storage_t * strm);

#endif /* NANOFI_INCLUDE_CORE_BUFFER_CHUNKS_H_ */
