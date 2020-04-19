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

#include <core/storage.h>
#include <chunkio/cio_log.h>
#include <chunkio/cio_chunk.h>
#include <chunkio/cio_file.h>
#include <chunkio/cio_utils.h>
#include <chunkio/cio_meta.h>
#include <ecu_api/ecuapi.h>
#include <core/cuuid.h>
#include <core/log.h>
#include <core/file_utils.h>

void generate_chunk_id(storage_chunk * chunk) {
  CIDGenerator gen;
  gen.implementation_ = CUUID_DEFAULT_IMPL;
  generate_uuid(&gen, chunk->uuid);
  chunk->uuid[36] = '\0';
}

storage_stream * create_stream(struct storage_config * config, const char * stream_name) {
  if (!config || !stream_name) return NULL;

  char * storage_path = config->storage_path;
  int storage_type = config->storage_type;

  int flags = CIO_CHECKSUM;
  if (storage_type == CIO_STORE_FS) {
    if (!storage_path || strlen(storage_path) == 0) {
      logc(err, "no storage path provided for file system storage type");
      return NULL;
    }
    if (strcasecmp(config->sync_mode, "full") == 0) {
      flags |= CIO_FULL_SYNC;
    }
  }

  if (!stream_name || strlen(stream_name) == 0) {
    logc(err, "storage stream name not specified or it is empty");
    return NULL;
  }

  storage_stream * strm = (storage_stream *)malloc(sizeof(storage_stream));
  memset(strm, 0, sizeof(*strm));
  strm->strg_config = config;

  if (storage_type == CIO_STORE_FS) {
    strm->cio_ctx = cio_create(storage_path, NULL, CIO_LOG_DEBUG, flags);
  } else {
    strm->cio_ctx = cio_create(NULL, NULL, CIO_LOG_DEBUG, flags);
  }

  if (!strm->cio_ctx) {
    free(strm);
    logc(err, "failed to create cio context");
    return NULL;
  }

  struct cio_stream * cio_strm =
      (storage_type == CIO_STORE_FS) ?
          cio_stream_create(strm->cio_ctx, stream_name, CIO_STORE_FS) :
          cio_stream_create(strm->cio_ctx, stream_name, CIO_STORE_MEM);

  if (!cio_strm) {
    cio_destroy(strm->cio_ctx);
    free(strm);
    logc(err, "failed to create cio stream");
    return NULL;
  }

  strm->cio_strm = cio_strm;
  load_backlog_chunks(strm);
  cio_set_max_chunks_up(strm->cio_ctx, config->max_chunks_up);
  initialize_lock(&strm->lock);
  return strm;
}

void destroy_stream(storage_stream * stream) {
  if (stream) {
    acquire_lock(&stream->lock);
    cio_destroy(stream->cio_ctx);
    release_lock(&stream->lock);
  }
}

storage_chunk create_chunk(storage_stream * stream, size_t chunk_size) {
  storage_chunk chunk;
  memset(&chunk, 0, sizeof(chunk));
  if (!stream)
    return chunk;

  chunk.size = chunk_size;
  generate_chunk_id(&chunk);
  int err;
  chunk.ck = cio_chunk_open(stream->cio_ctx, stream->cio_strm, chunk.uuid, CIO_OPEN, chunk.size, &err);
  chunk.error = err;
  return chunk;
}

void close_chunk(storage_stream * stream, struct cio_chunk * chunk) {
  acquire_lock(&stream->lock);
  int del = cio_chunk_is_up(chunk);
  cio_chunk_close(chunk, del);
  release_lock(&stream->lock);
}

void close_chunks(storage_stream * stream, struct mk_list * chunks) {
  struct mk_list * head;
  struct mk_list * tmp;
  acquire_lock(&stream->lock);
  mk_list_foreach_safe(head, tmp, chunks) {
    struct cio_chunk * entry = mk_list_entry(head, struct cio_chunk, _head);
    cio_chunk_close(entry, CIO_TRUE);
  }
  release_lock(&stream->lock);
}

int write_chunk_meta_data(struct cio_chunk * chunk, char * meta_data, size_t meta_len) {
  assert(chunk);
  if (meta_data && meta_len) {
    return cio_meta_write(chunk, meta_data, meta_len) >= 0;
  }
  return 1;
}

int write_chunk(storage_stream * stream, content_t content) {
  if (!stream || !content.data || content.data_len == 0) {
    return 0;
  }
  acquire_lock(&stream->lock);
  size_t sz = stream->strg_config->chunk_size;

  if (sz < (content.data_len + content.meta_len)) {
    sz = content.data_len + content.meta_len;
  }

  storage_chunk new_chunk = create_chunk(stream, sz);
  if (!new_chunk.ck) {
    release_lock(&stream->lock);
    return 0;
  }

  int ret = write_chunk_meta_data(new_chunk.ck, content.meta_data, content.meta_len)
      && cio_chunk_write(new_chunk.ck, content.data, content.data_len) >= 0;

  // we were not able to write to the chunk, lets remove it from the stream
  if (!ret) {
    int del = cio_chunk_is_up(new_chunk.ck);
    cio_chunk_close(new_chunk.ck, del);
  }
  release_lock(&stream->lock);
  return ret;
}

void reset_stream_get_chunks(struct storage_stream * stream, struct mk_list * chunks) {
  acquire_lock(&stream->lock);
  struct mk_list * old = &stream->cio_strm->chunks;
  chunks->prev = old->prev;
  chunks->next = old->next;
  old->next->prev = chunks;
  old->prev->next = chunks;
  mk_list_init(&stream->cio_strm->chunks);
  release_lock(&stream->lock);
}

void load_backlog_chunks(storage_stream * stream) {
#ifndef _WIN32
  assert(stream);
  if (stream->strg_config->storage_type != CIO_STORE_FS)
    return;

  char * storage_path = stream->strg_config->storage_path;
  char * stream_name = stream->cio_strm->name;
  char * path = concat_path(storage_path, stream_name);
  if (!is_directory(path)) {
    free(path);
    return;
  }
  struct dirent * dir;
  DIR * d = opendir(path);

  acquire_lock(&stream->lock);
  while ((dir = readdir(d)) != NULL) {
    char * entry_name = dir->d_name;
    if (!strcmp(entry_name, ".") || !strcmp(entry_name, "..")) {
        continue;
    }
    int err;
    struct cio_chunk * chunk = cio_chunk_open(stream->cio_ctx, stream->cio_strm, entry_name,
        CIO_OPEN, stream->strg_config->chunk_size, &err);
    if (!chunk) {
      char * chunk_path = concat_path(path, entry_name);
      remove_directory(chunk_path);
      free(chunk_path);
    }
  }
  release_lock(&stream->lock);
  closedir(d);
  free(path);
#endif
}

void chunks_up(storage_stream * stream, struct mk_list * chunks) {
  assert(stream);
  assert(chunks);
  if (stream->cio_strm->type == CIO_STORE_FS) {
    acquire_lock(&stream->lock);
    struct mk_list * head;
    struct mk_list * tmp;
    mk_list_foreach_safe(head, tmp, chunks) {
      struct cio_chunk * chunk = mk_list_entry(head, struct cio_chunk, _head);
      if (!cio_chunk_is_up(chunk) && cio_chunk_up_force(chunk) != CIO_OK) {
        cio_chunk_close(chunk, CIO_TRUE);
      }
    }
    release_lock(&stream->lock);
  }
}
