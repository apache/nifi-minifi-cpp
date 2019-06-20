
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

#ifdef __cplusplus
extern "C" {
#endif

extern nifi_instance * instance;
extern standalone_processor * proc;
extern flow_file_list * ff_list;
extern uint64_t curr_offset;
extern volatile sig_atomic_t stopped;

/**
 * Tails a delimited file starting from an offset up to the end of file
 * @param file the path to the file to tail
 * @param delim the delimiter character
 * @param curr_offset the offset in the file to tail from.
 * For eg. To tail from beginning of the file curr_offset = 0
 * @return a list of flow file info containing list of flow file records
 * and the current offset in the file
 */
flow_file_info log_aggregate(const char * file_path, char delim, uint64_t curr_offset);
void on_trigger_tailfilechunk(processor_session * ps, processor_context * ctx);
void on_trigger_logaggregator(processor_session * ps, processor_context * ctx);
void signal_handler(int signum);
void set_offset(uint64_t offset);
uint64_t get_offset();

#ifdef __cplusplus
}
#endif

#endif /* NANOFI_INCLUDE_API_ECU_H_ */
