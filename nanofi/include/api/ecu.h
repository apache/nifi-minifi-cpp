
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

#ifndef NANOFI_INCLUDE_API_ECU_H_
#define NANOFI_INCLUDE_API_ECU_H_

#include <signal.h>
#include "api/nanofi.h"
#include "uthash.h"
#include "utlist.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct proc_properties {
    char * file_path;
    char delimiter;
    uint64_t chunk_size;
} proc_properties;

typedef struct processor_params {
    char uuid_str[37]; //key
    struct flow_file_list * ff_list;
    uint64_t curr_offset;
    struct proc_properties * properties;
    UT_hash_handle hh;
} processor_params;

extern processor_params * procparams;
extern volatile sig_atomic_t stopped;

typedef struct tailfile_input_params {
    char * file;
    char * interval;
    char * delimiter;
    char * instance;
    char * tcp_port;
    char * nifi_port_uuid;
    char * chunk_size;
} tailfile_input_params;

typedef struct nifi_proc_params {
    nifi_instance * instance;
    standalone_processor * processor;
} nifi_proc_params;

/**
 * Tails a delimited file starting from an offset up to the end of file
 * @param file the path to the file to tail
 * @param delim the delimiter character
 * @param ctx the process context
 * For eg. To tail from beginning of the file curr_offset = 0
 * @return a list of flow file info containing list of flow file records
 * and the current offset in the file
 */
flow_file_info log_aggregate(const char * file_path, char delim, processor_context * ctx);
void on_trigger_tailfilechunk(processor_session * ps, processor_context * ctx);
void on_trigger_logaggregator(processor_session * ps, processor_context * ctx);
void on_trigger_tailfiledelimited(processor_session * ps, processor_context * ctx);
void signal_handler(int signum);
void delete_all_flow_files_from_proc(const char * uuid);
void delete_completed_flow_files_from_proc(const char * uuid);
void update_proc_params(const char * uuid, uint64_t value, flow_file_list * ff);
processor_params * get_proc_params(const char * uuid);

void init_common_input(tailfile_input_params * input_params, char ** args);
tailfile_input_params init_logaggregate_input(char ** args);
tailfile_input_params init_tailfile_chunk_input(char ** args);

int validate_input_params(tailfile_input_params * params, uint64_t * intrvl, uint64_t * port_num);
void setup_signal_action();
nifi_proc_params setup_nifi_processor(tailfile_input_params * input_params, const char * processor_name, void(*callback)(processor_session *, processor_context *));
void free_proc_params(const char * uuid);

#ifdef __cplusplus
}
#endif

#endif /* NANOFI_INCLUDE_API_ECU_H_ */
