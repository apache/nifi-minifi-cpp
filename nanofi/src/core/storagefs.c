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

#include <core/cuuid.h>
#include <core/file_utils.h>
#include <core/storagefs.h>

storage_t * create_storage(const char * root_path) {
  if (!root_path) return NULL;

  storage_t * strg = (storage_t *)malloc(sizeof(storage_t));
  memset(strg, 0, sizeof(storage_t));
  CIDGenerator gen;
  gen.implementation_ = CUUID_DEFAULT_IMPL;
  generate_uuid(&gen, strg->uuid);
  strg->fs_path = concat_path(root_path, strg->uuid);
  if (make_dir(strg->fs_path) < 0) {
    free(strg->fs_path);
    free(strg);
    return NULL;
  }
  return strg;
}

chunks_t * add_chunk(storage_t * strg, const char * buffer, size_t len) {
  if (!strg || !buffer || len == 0) return NULL;

  chunks_t * ck = (chunks_t *)malloc(sizeof(chunks_t));
  strg->last_chunk_number++;
  ck->buffer = buffer;
  ck->len = len;
  snprintf(ck->path, sizeof(ck->path), "_%u", strg->last_chunk_number);
  LL_APPEND(strg->ct, ck);
  write_chunk_to_storage(strg, ck);
  return ck;
}

int write_chunk_to_storage(storage_t * strg, chunks_t * ck) {
  if (!strg || !strg->fs_path || strlen(strg->fs_path) == 0
        || !ck || strlen(ck->path) == 0)
    return -1;

  if (!ck->buffer || ck->len == 0) return -1;

  const char * path = concat_path(strg->fs_path, ck->path);
  FILE * fp = fopen(path, "w");
  if (!fp) {
      free(path);
      fclose(fp);
      return -1;
  }

  if (fwrite(ck->buffer, 1, ck->len, fp) < ck->len) {
      free(path);
      fclose(fp);
      return -1;
  }
  free(path);
  fclose(fp);
  return 0;
}

void add_chunk_attributes(chunks_t * chunk, attribute_set as) {
    if (!chunk) return;
    chunk->as = as;
}

int move_chunks(storage_t * source, storage_t * dest) {
    if (!source || !dest) return -1;
    strcpy(dest->uuid, source->uuid);
    if (source->fs_path) {
        if (!dest->fs_path) {
            dest->fs_path = (char *)malloc(strlen(source->fs_path) + 1);
            strpcy(dest->fs_path, source->fs_path);
        }
    }
    dest->ct = source->ct;
    source->ct = NULL;
    dest->total_size = source->total_size;
    dest->last_chunk_number = source->last_chunk_number;
    return 0;
}

void read_chunks_up(storage_t * strm) {
    if (!strm || !strm->fs_path) return;

}

void write_chunks_down(storage_t * strm) {

}
