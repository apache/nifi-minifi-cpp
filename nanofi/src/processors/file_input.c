/*
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

#ifndef WIN32
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <core/log.h>
#include <core/string_utils.h>
#include <core/core_utils.h>
#include <core/file_utils.h>
#include <processors/file_input.h>

void initialize_file_input(file_input_context_t * ctx) {
  initialize_lock(&ctx->stop_mutex);
  initialize_cv(&ctx->stop_cond, NULL);
}

void start_file_input(file_input_context_t * ctx) {
  acquire_lock(&ctx->stop_mutex);
  ctx->stop = 0;
  ctx->running = 1;
  release_lock(&ctx->stop_mutex);
}

void wait_file_input_stop(file_input_context_t * ctx) {
  acquire_lock(&ctx->stop_mutex);
  ctx->stop = 1;
  while (ctx->running) {
    condition_variable_wait(&ctx->stop_cond, &ctx->stop_mutex);
  }
  release_lock(&ctx->stop_mutex);
}

void free_file_list(fileinfo * fps) {
  fileinfo * el, *tmp;
  LL_FOREACH_SAFE(fps, el, tmp) {
    LL_DELETE(fps, el);
    free(el->path);
    free(el);
  }
}

int validate_file_delimiter(const char * delimiter_str, char * delim) {
  if (!delimiter_str || strlen(delimiter_str) == 0) {
    return -1;
  }

  char delimiter[3];
  strncpy(delimiter, delimiter_str, 2);
  delimiter[2] = '\0';
  *delim = delimiter[0];

  if (*delim == '\\' && strlen(delimiter) > 1) {
    switch (delimiter[1]) {
    case 'r':
      *delim = '\r';
      break;
    case 'n':
      *delim = '\n';
      break;
    case 't':
      *delim = '\t';
      break;
    case '\\':
      *delim = '\\';
      break;
    }
  }
  return 0;
}

int validate_file_properties(struct file_input_context * context) {
  if (!context || !context->input_properties) {
    logc(err, "input context properties not defined");
    return -1;
  }

  properties_t * props = context->input_properties;
  properties_t * el = NULL;
  HASH_FIND_STR(props, "file_path", el);
  if (!el) {
    return -1;
  }
  char * file_path = el->value;
  if (!file_path) {
    return -1;
  }

  properties_t * cs = NULL;
  properties_t * dl = NULL;
  HASH_FIND_STR(props, "chunk_size_bytes", cs);
  HASH_FIND_STR(props, "delimiter", dl);
  if (dl && cs) {
    return -1;
  }

  if (!dl && !cs) {
    return -1;
  }

  char * chunk_size_str = NULL;
  char * delimiter = NULL;

  if (cs) {
    chunk_size_str = cs->value;
  }
  if (dl) {
    delimiter = dl->value;
  }

  el = NULL;
  HASH_FIND_STR(props, "tail_frequency_ms", el);
  if (!el) {
    return -1;
  }

  char * tail_frequency_str = el->value;

  uint64_t chunk_size_uint = 0;
  uint64_t tail_frequency_uint = 0;
  char delim = '\0';

  fileinfo * files = scan_matching_files(file_path, 0);
  if (!files) {
    logc(err, "No matching files found for path %s", file_path);
    return -1;
  }

  if ((dl && validate_file_delimiter(delimiter, &delim) < 0)
      || (cs && str_to_uint(chunk_size_str, &chunk_size_uint) < 0)
      || (str_to_uint(tail_frequency_str, &tail_frequency_uint) < 0)
      || (dl && delim == '\0')) {
    return -1;
  }

  free_file_list(context->files);
  context->files = files;
  context->tail_frequency_ms = tail_frequency_uint;

  if (cs)
    context->chunk_size = chunk_size_uint;
  if (dl)
    context->delimiter = delim;
  return 0;
}

int set_file_input_property(file_input_context_t * ctx, const char * name, const char * value) {
  return add_property(&ctx->input_properties, name, value);
}

void prepare_metadata(size_t current_offset, const char * path, char ** meta_data, size_t * len) {
  char * offset_str = uint_to_str(current_offset);
  properties_t * props = NULL;
  add_property(&props, "file_path", path);
  add_property(&props, "current_offset", offset_str);
  serialize_properties(props, meta_data, len);
  free(offset_str);
}

content_t prepare_content(char * from, size_t data_len, char * meta_data, size_t meta_len) {
  content_t content;
  content.data = (char *) malloc(data_len * sizeof(char));
  memcpy(content.data, from, data_len);
  content.data_len = data_len;
  content.meta_data = meta_data;
  content.meta_len = meta_len;
  return content;
}

void free_content(content_t content) {
  free(content.data);
  free(content.meta_data);
}

uint64_t process_content(file_input_context_t * ctx, char * data, uint64_t len, uint64_t offset, const char * path) {
  char * meta_data;
  uint64_t meta_len = 0;
  prepare_metadata(offset, path, &meta_data, &meta_len);
  content_t content = prepare_content(data, len, meta_data, meta_len);
  if (!write_chunk(ctx->stream, content)) {
    free_content(content);
    return 0;
  }
  free_content(content);
  return len;
}

void reset_file_list(file_input_context_t * ctx) {
  LL_CONCAT(ctx->files, ctx->temps);
  ctx->temps = NULL;
}

void reset_file_context(file_input_context_t * ctx, fileinfo ** file) {
  assert(ctx && file);
  LL_DELETE(ctx->files, *file);
  LL_APPEND(ctx->temps, *file);
  reset_file_list(ctx);
}

void read_file_chunk(file_input_context_t * ctx) {
  fileinfo * file, *tmp;
  LL_FOREACH_SAFE(ctx->files, file, tmp) {
    errno = 0;
    FILE * fp = fopen(file->path, "rb");
    if (!fp) {
      logc(err, "Error opening file %s, error: %s\n", file->path, strerror(errno));
      return;
    }
    uint64_t bytes_read = 0;
    char * data = (char *) malloc((ctx->chunk_size) * sizeof(char));
    fseek(fp, file->offset, SEEK_SET);
    while ((bytes_read = fread(data, 1, ctx->chunk_size, fp)) > 0) {
      if (bytes_read < ctx->chunk_size) {
        break;
      }
      uint64_t processed_bytes = process_content(ctx, data, bytes_read, ftell(fp), file->path);
      if (!processed_bytes) {
        fclose(fp);
        free(data);
        reset_file_context(ctx, &file);
        return;
      }

      file->offset += processed_bytes;
      fseek(fp, file->offset, SEEK_SET);
    }
    free(data);
    fclose(fp);
    LL_DELETE(ctx->files, file);
    LL_APPEND(ctx->temps, file);
  }
  reset_file_list(ctx);
}

void read_file_delim(file_input_context_t * ctx) {
  fileinfo * file, *tmp;
  LL_FOREACH_SAFE(ctx->files, file, tmp) {
    FILE * fp = fopen(file->path, "rb");
    errno = 0;
    if (!fp) {
      logc(err, "Unable to open file. {file: %s, reason: %s}\n", file->path, strerror(errno));
      return;
    }

    char data[4096];
    memset(data, 0, sizeof(data));
    fseek(fp, file->offset, SEEK_SET);
    uint64_t old_offset = file->offset;
    uint64_t bytes_read = 0;
    while ((bytes_read = fread(data, 1, 4096, fp)) > 0) {
      const char * begin = data;
      const char * end = NULL;

      uint64_t search_bytes = bytes_read;
      while ((end = memchr(begin, ctx->delimiter, search_bytes))) {
        old_offset = file->offset;
        uint64_t len = end - begin;
        file->offset += (len + 1);
        if (len > 0) {
          uint64_t processed_bytes = process_content(ctx, (char *)begin, len, file->offset, file->path);
          if (!processed_bytes) {
            fclose(fp);
            file->offset = old_offset;
            reset_file_context(ctx, &file);
            return;
          }
        }
        begin = (end + 1);
        search_bytes -= (len + 1);
      }

      //at this point we did not find the delimiter in search_bytes bytes
      //if search_bytes is less than 4096, we will come back later
      old_offset = file->offset;
      if (search_bytes != 0) {
        if (search_bytes < 4096) {
          fclose(fp);
          return;
        }
        file->offset += search_bytes;
        uint64_t processed_bytes = process_content(ctx, (char *)begin, search_bytes, file->offset, file->path);
        if (!processed_bytes) {
          fclose(fp);
          file->offset = old_offset;
          reset_file_context(ctx, &file);
          return;
        }
      }
      //ship out the bytes anyway even though we did not find
      //the delimiter. This is necessary to avoid getting
      //stuck at an offset and not being able to ship contents
      //even though delimiter might appear albeit 4096 bytes beyond
      //current offset
      fseek(fp, file->offset, SEEK_SET);
    }
    fclose(fp);
    LL_DELETE(ctx->files, file);
    LL_APPEND(ctx->temps, file);
  }
  reset_file_list(ctx);
}

file_input_context_t * create_file_input_context() {
  file_input_context_t * ctx = (file_input_context_t *) malloc(
      sizeof(file_input_context_t));
  memset(ctx, 0, sizeof(file_input_context_t));
  initialize_file_input(ctx);
  return ctx;
}

task_state_t file_reader_processor(void * args, void * state) {
  file_input_context_t * ctx = (file_input_context_t *) args;
  acquire_lock(&ctx->stop_mutex);
  if (ctx->stop) {
    ctx->running = 0;
    condition_variable_broadcast(&ctx->stop_cond);
    release_lock(&ctx->stop_mutex);
    return DONOT_RUN_AGAIN;
  }
  release_lock(&ctx->stop_mutex);

  if (ctx->chunk_size > 0) {
    read_file_chunk(ctx);
  } else {
    read_file_delim(ctx);
  }
  return RUN_AGAIN;
}

void free_file_input_properties(file_input_context_t * ctx) {
  properties_t * ip_props = ctx->input_properties;
  ctx->input_properties = NULL;
  free_properties(ip_props);
}

void free_file_input_context(file_input_context_t * ctx) {
  free_properties(ctx->input_properties);
  free(ctx->file_path);
  free_file_list(ctx->files);
  destroy_lock(&ctx->stop_mutex);
  free(ctx);
}
