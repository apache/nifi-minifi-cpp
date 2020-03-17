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

#include <stdio.h>
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
  strg->dir_path = concat_path(root_path, strg->uuid);
  if (make_dir(strg->dir_path) < 0) {
    free(strg->dir_path);
    free(strg);
    return NULL;
  }
  return strg;
}

chunks_t * add_chunk(storage_t * strg, const char * buffer, size_t len) {
  if (!strg || !buffer || len == 0) return NULL;

  chunks_t * ck = (chunks_t *)malloc(sizeof(chunks_t));
  ck->buffer = buffer;
  ck->len = len;
  strg->total_size += len;
  CIDGenerator gen;
  gen.implementation_ = CUUID_DEFAULT_IMPL;
  generate_uuid(&gen, ck->name);
  LL_APPEND(strg->ct, ck);
  return ck;
}

int write_chunk_to_storage(storage_t * strg, chunks_t * ck) {
  if (!strg || !strg->dir_path || !ck)
    return -1;

  if (!ck->buffer || ck->len == 0) return -1;

  char * path = concat_path(strg->dir_path, ck->name);
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
  if (!source || !dest || !source->dir_path || !is_directory(source->dir_path))
    return -1;

  if (dest->dir_path) {
    char * tmp = dest->dir_path;
    free(tmp);
  }
  dest->dir_path = (char *)malloc(strlen(source->dir_path) + 1);
  strpcy(dest->dir_path, source->dir_path);
  strcpy(dest->uuid, source->uuid);
  dest->ct = source->ct;
  source->ct = NULL;
  return 0;
}

int read_chunks_up(storage_t * strm) {
  if (!strm || !strm->dir_path || !is_directory(strm->dir_path))
    return -1;

  DIR * dp = opendir(strm->dir_path);
  if (!dp) return -1;

  strm->total_size = 0;
  struct dirent * entry;
  while ((entry = readdir(dp)) != NULL) {
    char * path = concat_path(strm->dir_path, entry->d_name);
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || is_directory(path)) {
      // We expect all entries in the storage path to be files
      // so skip any thing that is a directory
      free(path);
      continue;
    }
    FILE * fp = fopen(path, "r");
    if (!fp) {
      closedir(dp);
      free(path);
      return -1;
    }
    size_t sz = get_file_size(fp);
    char * buffer = (char *)malloc(sz * sizeof(char));
    chunks_t * chunk = (chunks_t *)malloc(sizeof(chunks_t));
    chunk->buffer = buffer;
    chunk->len = sz;
    size_t name_len = sizeof(chunk->name);
    strncpy(chunk->name, entry->d_name, name_len - 1);
    chunk->name[name_len - 1] = '\0';
    LL_APPEND(strm->ct, chunk);
    strm->total_size += sz;
    fclose(fp);
  }
  return 0;
}

void write_chunks_down(storage_t * strm) {
  chunks_t * el;
  LL_FOREACH(strm->ct, el) {
    write_chunk_to_storage(strm, el);
  }
}
