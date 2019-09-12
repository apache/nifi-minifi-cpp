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

#ifndef NIFI_MINIFI_CPP_FILE_INPUT_H
#define NIFI_MINIFI_CPP_FILE_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <core/threadutils.h>
#include <core/message_queue.h>
#include <ecu_api/ecuapi.h>

typedef struct file_input_context {
    uint64_t current_offset;
    properties_t * input_properties;
    char * file_path;
    uint64_t chunk_size;
    char delimiter;
    uint64_t tail_frequency_ms;
    message_queue_t * msg_queue;
    int stop;
    lock_t stop_mutex;
    conditionvariable_t stop_cond;
} file_input_context_t;

typedef struct data_buff {
    char * data;
    size_t len;
    char * file_path;
} data_buff_t;

void initialize_file_input(file_input_context_t * ctx);
void start_file_input(file_input_context_t * ctx);
void stop_file_input(file_input_context_t * ctx);
int validate_file_properties(struct file_input_context * context);
task_state_t file_reader_processor(void * args, void * state);
void read_file_chunk(file_input_context_t * ctx);
void read_file_delim(file_input_context_t * ctx);
void free_file_input_properties(file_input_context_t * ctx);
void free_file_input_context(file_input_context_t * ctx);
void wait_file_input_stop(file_input_context_t * ctx);

message_attrs_t * get_updated_attributes(message_t * msg, size_t bytes_written, size_t bytes_enqueued);

int set_file_input_property(file_input_context_t * ctx, const char * name, const char * value);
file_input_context_t * create_file_input_context();
void set_file_params(file_input_context_t * f_ctx,
        const char * file_path,
        uint64_t tail_freq,
        uint64_t current_offset);
void set_file_chunk_params(file_input_context_t * f_ctx,
        const char * file_path,
        uint64_t tail_freq,
        uint64_t current_offset,
        uint64_t chunk_size);
void set_file_delim_params(file_input_context_t * f_ctx,
        const char * file_path,
        uint64_t tail_freq,
        uint64_t current_offset,
        char delim);
data_buff_t read_file_chunk_data(FILE * fp, file_input_context_t * ctx);

static const property_descriptor chunk_prop_desc[3] = {
        {"file_path", "File Path", "Path of the file to tail on", 1, 0, 0, "PathValidator"},
        {"chunk_size_bytes", "Chunk size in bytes", "File chunk size in bytes to tail", 1, 0, 0, "Int64Validator"},
        {"tail_frequency_ms", "Tail Frequency MS", "The frequency at which to perform tail", 1, 0, 0, "Int64Validator"}
};

static const property_descriptor delim_prop_desc[3] = {
        {"file_path", "File Path", "Path of the file to tail on", 1, 0, 0, "PathValidator"},
        {"delimiter", "Delimiter", "Delimiter to tail against", 1, 0, 0, "StringValidator"},
        {"tail_frequency_ms", "Tail Frequency MS", "The frequency at which to perform tail", 1, 0, 0, "Int64Validator"}
};

static const io_descriptor file_input_desc[2] = {
        {"TAILFILECHUNK", 3, chunk_prop_desc},
        {"TAILFILEDELIMITER", 3, delim_prop_desc}
};

#ifdef __cplusplus
}
#endif

#endif //NIFI_MINIFI_CPP_FILE_INPUT_H
