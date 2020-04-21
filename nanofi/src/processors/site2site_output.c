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

#include <errno.h>
#include <stdio.h>
#include <core/log.h>
#include <core/core_utils.h>
#include <processors/site2site_output.h>

void initialize_s2s_output(site2site_output_context_t * ctx) {
  mk_list_init(&ctx->backlog_chunks);
  initialize_lock(&ctx->client_mutex);
  initialize_lock(&ctx->stop_mutex);
  initialize_cv(&ctx->stop_cond, NULL);
}

void start_s2s_output(site2site_output_context_t * ctx) {
  acquire_lock(&ctx->stop_mutex);
  ctx->stop = 0;
  if (ctx->stream) {
    reset_stream_get_chunks(ctx->stream, &ctx->backlog_chunks);
  }
  ctx->running = 1;
  release_lock(&ctx->stop_mutex);
}

void wait_s2s_output_stop(site2site_output_context_t * ctx) {
  acquire_lock(&ctx->stop_mutex);
  ctx->stop = 1;
  while (ctx->running) {
    condition_variable_wait(&ctx->stop_cond, &ctx->stop_mutex);
  }
  release_lock(&ctx->stop_mutex);
}

void free_s2s_output_context(site2site_output_context_t * ctx) {
  free_properties(ctx->output_properties);
  free(ctx->host_name);
  if (ctx->client)
    destroyClient(ctx->client);
  free(ctx->client);
  destroy_lock(&ctx->client_mutex);
  destroy_lock(&ctx->stop_mutex);
  destroy_cv(&ctx->stop_cond);
  free(ctx);
}

void write_to_s2s(site2site_output_context_t * s2s_ctx, struct mk_list * chunks) {
  struct mk_list * head;
  struct mk_list * tmp;
  mk_list_foreach_safe(head, tmp, chunks) {
    struct cio_chunk * chunk = mk_list_entry(head, struct cio_chunk, _head);
    char * data = NULL;
    size_t len = 0;
    char * meta = NULL;
    int mlen = 0;
    if (cio_chunk_get_content(chunk, &data, &len) != CIO_OK
        || cio_meta_read(chunk, &meta, &mlen) < 0) {
      close_chunk(s2s_ctx->stream, chunk);
      continue;
    }
    if (data && len && meta && mlen) {
      attribute_set as = unpack_metadata(meta, (size_t)mlen);
      if (!as.attributes || !as.size) {
        close_chunk(s2s_ctx->stream, chunk);
        continue;
      }
      acquire_lock(&s2s_ctx->client_mutex);
      transmitRawPayload(s2s_ctx->client, data, len, &as);
      release_lock(&s2s_ctx->client_mutex);
      free_attributes(as);
      close_chunk(s2s_ctx->stream, chunk);
    }
  }
}

task_state_t site2site_writer_processor(void * args, void * state) {
  site2site_output_context_t * ctx = (site2site_output_context_t *) args;

  acquire_lock(&ctx->stop_mutex);
  if (ctx->stop) {
    ctx->running = 0;
    condition_variable_broadcast(&ctx->stop_cond);
    release_lock(&ctx->stop_mutex);
    return DONOT_RUN_AGAIN;
  }
  release_lock(&ctx->stop_mutex);

  if (mk_list_is_empty(&ctx->backlog_chunks) != 0) {
    chunks_up(ctx->stream, &ctx->backlog_chunks);
    write_to_s2s(ctx, &ctx->backlog_chunks);
  }

  struct mk_list chunks;
  reset_stream_get_chunks(ctx->stream, &chunks);
  write_to_s2s(ctx, &chunks);
  return RUN_AGAIN;
}

int validate_s2s_properties(site2site_output_context_t * ctx) {
  if (!ctx) {
    return -1;
  }
  properties_t * props = ctx->output_properties;
  properties_t * tcp_el;
  HASH_FIND_STR(props, "tcp_port", tcp_el);
  if (!tcp_el) {
    return -1;
  }
  if (!tcp_el->value) {
    return -1;
  }
  uint64_t tcp_port = (uint64_t) strtoul(tcp_el->value, NULL, 10);
  if (errno != 0) {
    return -1;
  }
  ctx->tcp_port = tcp_port;

  properties_t * nifi_el = NULL;
  HASH_FIND_STR(props, "nifi_port_uuid", nifi_el);
  if (!nifi_el) {
    return -1;
  }
  if (!nifi_el->value) {
    return -1;
  }
  strcpy(ctx->port_uuid, nifi_el->value);

  properties_t * host_el = NULL;
  HASH_FIND_STR(props, "host_name", host_el);
  if (!host_el || !host_el->value || strlen(host_el->value) == 0) {
    logc(err, "host name for sitetosite not specified");
    return -1;
  }

  size_t hlen = strlen(host_el->value);
  char * host_name = (char *) malloc(hlen + 1);
  strcpy(host_name, host_el->value);
  char * hn = ctx->host_name;
  if (hn)
    free(hn);
  ctx->host_name = host_name;

  properties_t * fel = NULL;
  HASH_FIND_STR(props, "flush_interval_ms", fel);
  if (!fel || !fel->value || strlen(fel->value) == 0) {
    logc(err, "flush interval ms not specified for sitetosite");
    return -1;
  }

  if (str_to_uint(fel->value, &ctx->flush_interval_ms) < 0) {
    logc(err, "flush interval incorrect format");
    return -1;
  }

  if (ctx->client) {
    struct CRawSiteToSiteClient * cl = ctx->client;
    if (cl) {
      destroyClient(cl);
    }
  }
  ctx->client = createClient(ctx->host_name, (uint16_t) tcp_port, ctx->port_uuid);
  return 0;
}

site2site_output_context_t * create_s2s_output_context() {
  site2site_output_context_t * ctx = (site2site_output_context_t *) malloc(sizeof(*ctx));
  memset(ctx, 0, sizeof(site2site_output_context_t));
  initialize_s2s_output(ctx);
  return ctx;
}

int set_s2s_output_property(site2site_output_context_t * ctx, const char * name,
    const char * value) {
  return add_property(&ctx->output_properties, name, value);
}

void free_s2s_output_properties(site2site_output_context_t * ctx) {
  properties_t * props = ctx->output_properties;
  ctx->output_properties = NULL;
  free_properties(props);
}
