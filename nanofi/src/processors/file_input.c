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
#include <core/log.h>
#include <sys/types.h>
#include <core/string_utils.h>
#include <core/core_utils.h>
#include <processors/file_input.h>

void initialize_file_input(file_input_context_t * ctx) {
  initialize_lock(&ctx->stop_mutex);
  initialize_cv(&ctx->stop_cond, NULL);
}

void start_file_input(file_input_context_t * ctx) {
  acquire_lock(&ctx->stop_mutex);
  ctx->stop = 0;
  release_lock(&ctx->stop_mutex);
}

void wait_file_input_stop(file_input_context_t * ctx) {
  acquire_lock(&ctx->stop_mutex);
  ctx->stop = 1;
  while (!ctx->done) {
    condition_variable_wait(&ctx->stop_cond, &ctx->stop_mutex);
  }
  release_lock(&ctx->stop_mutex);
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
    printf("input context properties not defined\n");
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

  if ((is_file(file_path) < 0)
      || (dl && validate_file_delimiter(delimiter, &delim) < 0)
      || (cs && str_to_uint(chunk_size_str, &chunk_size_uint) < 0)
      || (str_to_uint(tail_frequency_str, &tail_frequency_uint) < 0)
      || (dl && delim == '\0')) {
    return -1;
  }

  //populate file input context with parameters
  size_t file_path_len = strlen(file_path);
  char * fp = context->file_path;
  free(fp);
  context->file_path = (char *) malloc(file_path_len + 1);
  strcpy(context->file_path, file_path);

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

void prepare_meta_data(file_input_context_t * ctx, char ** meta_data, size_t * len) {
  char * offset_str = uint_to_str(ctx->current_offset);
  properties_t * props = NULL;
  add_property(&props, "file_path", ctx->file_path);
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

void read_file_chunk(file_input_context_t * ctx) {
  errno = 0;
  FILE * fp = fopen(ctx->file_path, "rb");
  if (!fp) {
    logc(err, "Error opening file %s, error: %s\n", ctx->file_path, strerror(errno));
    return;
  }
  size_t bytes_read = 0;
  char * data = (char *) malloc((ctx->chunk_size) * sizeof(char));
  fseek(fp, ctx->current_offset, SEEK_SET);
  while ((bytes_read = fread(data, 1, ctx->chunk_size, fp)) > 0) {
    if (bytes_read < ctx->chunk_size) {
      break;
    }
    size_t old_offset = ctx->current_offset;
    ctx->current_offset = ftell(fp);
    char * meta_data;
    size_t meta_len = 0;
    prepare_meta_data(ctx, &meta_data, &meta_len);
    content_t content = prepare_content(data, bytes_read, meta_data, meta_len);
    // prepare meta data, the file name and current offset
    if (!write_chunk(ctx->stream, content)) {
      ctx->current_offset = old_offset;
      break;
    }
    free_content(content);
    fseek(fp, ctx->current_offset, SEEK_SET);
  }
  free(data);
  fclose(fp);
}

void read_file_delim(file_input_context_t * ctx) {
  FILE * fp = fopen(ctx->file_path, "rb");
  errno = 0;
  if (!fp) {
    logc(err, "Unable to open file. {file: %s, reason: %s}\n", ctx->file_path, strerror(errno));
    return;
  }

  char data[4096];
  memset(data, 0, sizeof(data));
  fseek(fp, ctx->current_offset, SEEK_SET);
  size_t old_offset = ctx->current_offset;
  size_t bytes_read = 0;
  while ((bytes_read = fread(data, 1, 4096, fp)) > 0) {
    const char * begin = data;
    const char * end = NULL;

    size_t search_bytes = bytes_read;
    while ((end = memchr(begin, ctx->delimiter, search_bytes))) {
      old_offset = ctx->current_offset;
      uint64_t len = end - begin;
      ctx->current_offset += (len + 1);
      if (len > 0) {
        char * meta_data;
        size_t meta_len = 0;
        prepare_meta_data(ctx, &meta_data, &meta_len);
        content_t content = prepare_content((char *)begin, len, meta_data, meta_len);
        if (!write_chunk(ctx->stream, content)) {
          free_content(content);
          fclose(fp);
          ctx->current_offset = old_offset;
          return;
        }
        free_content(content);
      }
      begin = (end + 1);
      search_bytes -= (len + 1);
    }

    //at this point we did not find the delimiter in search_bytes bytes
    //if search_bytes is less than 4096, we will come back later
    old_offset = ctx->current_offset;
    if (search_bytes != 0) {
      if (search_bytes < 4096) {
        fclose(fp);
        return;
      }
      ctx->current_offset += search_bytes;
      char * meta_data;
      size_t meta_len = 0;
      prepare_meta_data(ctx, &meta_data, &meta_len);
      content_t content = prepare_content((char *)begin, search_bytes, meta_data, meta_len);
      if (!write_chunk(ctx->stream, content)) {
        free_content(content);
        fclose(fp);
        ctx->current_offset = old_offset;
        return;
      }
      free_content(content);
    }
    //ship out the bytes anyway even though we did not find
    //the delimiter. This is necessary to avoid getting
    //stuck at an offset and not being able to ship contents
    //even though delimiter might appear albeit 4096 bytes beyond
    //current offset
    fseek(fp, ctx->current_offset, SEEK_SET);
  }
  fclose(fp);
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
    ctx->done = 1;
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
  destroy_lock(&ctx->stop_mutex);
  free(ctx);
}

void set_file_params(file_input_context_t * f_ctx, const char * file_path,
  uint64_t tail_freq, uint64_t current_offset) {
  size_t fp_len = strlen(file_path);
  f_ctx->file_path = (char *) malloc(fp_len + 1);
  strcpy(f_ctx->file_path, file_path);
  f_ctx->tail_frequency_ms = tail_freq;
  f_ctx->current_offset = current_offset;
}

void set_file_chunk_params(file_input_context_t * f_ctx, const char * file_path,
    uint64_t tail_freq, uint64_t current_offset, uint64_t chunk_size) {
  set_file_params(f_ctx, file_path, tail_freq, current_offset);
  f_ctx->chunk_size = chunk_size;
}

void set_file_delim_params(file_input_context_t * f_ctx, const char * file_path,
    uint64_t tail_freq, uint64_t current_offset, char delim) {
  set_file_params(f_ctx, file_path, tail_freq, current_offset);
  f_ctx->delimiter = delim;
}

data_buff_t read_file_chunk_data(FILE * fp, file_input_context_t * ctx) {
  data_buff_t chunk;
  memset(&chunk, 0, sizeof(data_buff_t));
  if (!ctx || ctx->chunk_size == 0 || !fp) {
    return chunk;
  }
  size_t bytes_read = 0;
  chunk.data = (char *) malloc(ctx->chunk_size + 1);
  fseek(fp, ctx->current_offset, SEEK_SET);
  bytes_read = fread(chunk.data, 1, ctx->chunk_size, fp);
  chunk.data[bytes_read] = '\0';
  chunk.len = bytes_read;
  ctx->current_offset += bytes_read;
  return chunk;
}
