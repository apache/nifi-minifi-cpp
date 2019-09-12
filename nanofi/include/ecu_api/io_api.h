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

#ifndef NANOFI_INCLUDE_ECU_API_IO_API_H_
#define NANOFI_INCLUDE_ECU_API_IO_API_H_

#include <core/cstructs.h>

typedef enum io_type {
    TAILFILE,
    SITE2SITE,
    KAFKA,
    MQTTIO,
    MANUAL
} io_type_t;

static const char * io_type_str[MANUAL+1] = {"FILE", "SITE2SITE", "KAFKA", "MQTT", "MANUAL"};

typedef struct input_api {
    void*(*create_input_context)();
    int(*set_input_property)(void * ip_ctx, const char * name, const char * value);
    properties_t*(*get_input_properties)(void * ip_ctx);
    int(*validate_input_properties)(void * ip_ctx);
    void(*free_input_properties)(void * ip_ctx);
    void(*free_input_context)(void * ip_ctx);
    properties_t*(*clone_input_properties)(void * ip_ctx);
    void(*wait_input_stop)(void * ip_ctx);
} input_api_t;

typedef struct output_api {
    void*(*create_output_context)();
    int(*set_output_property)(void * op_ctx, const char * name, const char * value);
    properties_t*(*get_output_properties)(void * op_ctx);
    int(*validate_output_properties)(void * op_ctx);
    void(*free_output_properties)(void * op_ctx);
    void(*free_output_context)(void * op_ctx);
    properties_t*(*clone_output_properties)(void * op_ctx);
    void(*wait_output_stop)(void * op_ctx);
} output_api_t;

void * create_input_context_file();
int set_input_property_file(void * ip_ctx, const char * name, const char * value);
properties_t * get_input_properties_file(void * ip_ctx);
int validate_input_properties_file(void * ip_ctx);
void free_input_properties_file(void * ip_ctx);
void free_input_context_file(void * ip_ctx);
properties_t * clone_input_props_file(void * ip_ctx);
void wait_input_stop_file(void * ip_ctx);

void * create_input_context_manual();

void * create_output_context_stos();
int set_output_property_stos(void * op_ctx, const char * name, const char * value);
properties_t * get_output_properties_stos(void * op_ctx);
int validate_output_properties_stos(void * op_ctx);
void free_output_properties_stos(void * op_ctx);
void free_output_context_stos(void * op_ctx);
properties_t * clone_output_props_stos(void * op_ctx);
void wait_output_stop_stos(void * op_ctx);

/*
 * This array index is indexed by the
 * enum io_type shown above in this file
 */
static input_api_t input_map[5] = {
        {create_input_context_file,
         set_input_property_file,
         get_input_properties_file,
         validate_input_properties_file,
         free_input_properties_file,
         free_input_context_file,
         clone_input_props_file,
         wait_input_stop_file,
        },
        {0},
        {0},
        {0},
        {create_input_context_manual,0,0,0,0,0,0,0}
};

static output_api_t output_map[5] = {
        {0},
        {create_output_context_stos,
         set_output_property_stos,
         get_output_properties_stos,
         validate_output_properties_stos,
         free_output_properties_stos,
         free_output_context_stos,
         clone_output_props_stos,
         wait_output_stop_stos
        },
        {0},
        {0},
        {0}
};

#endif /* NANOFI_INCLUDE_ECU_API_IO_API_H_ */
