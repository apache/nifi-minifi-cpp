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

#ifndef NANOFI_INCLUDE_CORE_STORAGE_H_
#define NANOFI_INCLUDE_CORE_STORAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <chunkio/chunkio_compat.h>
#include <chunkio/chunkio.h>

#include <core/threadutils.h>

struct storage_config * strg_config;
struct mk_list * list;

typedef struct storage_stream {
  struct cio_ctx * cio_ctx;
  struct cio_stream * cio_strm;
  struct storage_config * strg_config;
  lock_t lock;
} storage_stream;

typedef struct storage_chunk {
  struct cio_chunk * ck;
  char uuid[37];
  size_t size;
  size_t meta_size;
  int error;
} storage_chunk;

typedef struct content {
  char * data;
  size_t data_len;
  char * meta_data;
  size_t meta_len;
} content_t;

storage_stream * create_stream(struct storage_config * config, const char * stream_name);

void destroy_stream(storage_stream * stream);

int write_chunk(storage_stream * stream, content_t content);

void close_chunk(storage_stream * stream, struct cio_chunk * chunk);

void close_chunks(storage_stream * stream, struct mk_list * chunks);

size_t get_backlog_chunks(storage_stream * stream, struct mk_list * out_chunks);

/**
 * Puts the cio_chunk list of cio_stream strm into out param
 * and resets the streams chunk list to empty
 * @note it is expected that the caller supplies the out parameter
 */
void reset_stream_get_chunks(storage_stream * strm, struct mk_list * out);

#ifdef __cplusplus
}
#endif
#endif /* NANOFI_INCLUDE_CORE_STORAGE_H_ */
