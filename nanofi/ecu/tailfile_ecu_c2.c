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

#ifndef _WIN32
#include <unistd.h>
#endif
#include <stdio.h>
#include <signal.h>
#include <ecu_api/ecuapi.h>
#include <processors/file_input.h>
#include <core/log.h>
#include <core/yaml_config.h>
#include <core/ecu_config.h>

volatile sig_atomic_t stop_ecu = 0;

void ecu_signal_handler(int signum) {
  if (signum == SIGINT || signum == SIGTERM) {
    stop_ecu = 1;
  }
}

void setup_ecu_signal_action() {
#ifdef _WIN32
  signal(SIGINT, ecu_signal_handler);
  signal(SIGTERM, ecu_signal_handler);
#else
  struct sigaction action;
  memset(&action, 0, sizeof(sigaction));
  action.sa_handler = ecu_signal_handler;
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGINT, &action, NULL);
#endif
}

int main(int argc, char ** argv) {
  if (argc < 2) {
    printf("usage ./tailfile_ecu_c2 <ecu config yaml>\n");
    return 1;
  }
  set_log_level(err);

  config_yaml_node_t * config = parse_yaml_configuration(argv[1]);
  if (!config) {
    logc(err, "failed to parse configuration file %s", argv[1]);
    return 1;
  }

  storage_config * strg_conf = parse_storage_configuration(config);
  if (!strg_conf) {
    free_config_yaml(config);
    logc(err, "failed to parse storage configuration");
    return 1;
  }

  ecu_config * ecu_confs = parse_ecu_configuration(config);
  if (!ecu_confs) {
    free_config_yaml(config);
    free_storage_config(strg_conf);
    free(strg_conf);
    logc(err, "failed to parse ecu configuration");
    return 1;
  }

  free_config_yaml(config);

  setup_ecu_signal_action();
  io_context_t * io = create_io_context();
  io->strg_conf = strg_conf;

  if (!io) {
    logc(err, "Could not create io context\n");
    free_config_yaml(config);
    free_all_ecu_configuration(ecu_confs);
    return 1;
  }

  ecu_config * el, *tmp;
  HASH_ITER(hh, ecu_confs, el, tmp) {
    properties_t * io_props = el->input_props;
    properties_t * out;
    HASH_FIND(hh, io_props, "type", strlen("type"), out);
    if (!out || !out->value) {
      logc(warn,
          "input type not provided. skip creating ecu {name: %s}. skipping...",
          el->name);
      continue;
    }
    io_type_t ip_type = get_io_type(out->value);
    input_context_t * ip = create_input(ip_type);

    io_props = el->output_props;
    HASH_FIND(hh, io_props, "type", strlen("type"), out);
    if (!out || !out->value) {
      logc(warn,
          "output type not provided. skip creating ecu {name: %s}. skipping...",
          el->name);
      continue;
    }
    io_type_t op_type = get_io_type(out->value);
    output_context_t * op = create_output(op_type);

    if (!ip || !op) {
      logc(warn,
          "input/output context creation failed for ecu {name: %s}. skipping...",
          el->name);
      continue;
    }

    ecu_context_t * ecu = create_ecu(io, el->name, el->stream_name, ip, op);
    set_input_properties(ecu, el->input_props);
    set_output_properties(ecu, el->output_props);

    if (!ecu) {
      free_input(ip);
      free_output(op);
      logc(err, "ecu creation failed {name: %s}", el->name);
      continue;
    }
    if (start_ecu_async(ecu) < 0) {
      logc(err, "could not start ecu {name: %s}", ecu->name);
      destroy_ecu(ecu);
    }
  }

  if (!io->ecus) {
    logc(warn, "no ecus were started, exiting ...");
    free_all_ecu_configuration(ecu_confs);
    destroy_io_context(io);
    return 1;
  }

  while (!stop_ecu) {
#ifndef WIN32
    sleep(1);
#else
    Sleep(1000);
#endif
  }

  free_all_ecu_configuration(ecu_confs);
  destroy_io_context(io);
  logc(info, "%s", "destroyed io context");
}
