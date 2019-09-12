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

#include <processors/file_input.h>
#include <processors/site2site_output.h>
#include <ecu_api/io_api.h>

void * create_input_context_file() {
    file_input_context_t * f_ctx = create_file_input_context();
    return (void *)f_ctx;
}

int set_input_property_file(void * ip_ctx, const char * name, const char * value) {
    if (!ip_ctx || !name || !value) {
        return -1;
    }
    file_input_context_t * f_ctx = (file_input_context_t *)ip_ctx;
    return set_file_input_property(f_ctx, name, value);
}

properties_t * get_input_properties_file(void * ip_ctx) {
    if (!ip_ctx) return NULL;
    file_input_context_t * f_ctx = (file_input_context_t *)ip_ctx;
    return f_ctx->input_properties;
}

int validate_input_properties_file(void * ip_ctx) {
    file_input_context_t * f_ctx = (file_input_context_t *)ip_ctx;
    return validate_file_properties(f_ctx);
}

void free_input_properties_file(void * ip_ctx) {
    file_input_context_t * f_ctx = (file_input_context_t *)ip_ctx;
    free_file_input_properties(f_ctx);
}

void free_input_context_file(void * ip_ctx) {
    file_input_context_t * f_ctx = (file_input_context_t *)ip_ctx;
    free_file_input_context(f_ctx);
}

properties_t * clone_input_props_file(void * ip_ctx) {
    file_input_context_t * f_ctx = (file_input_context_t *)ip_ctx;
    return clone_properties(f_ctx->input_properties);
}

void wait_input_stop_file(void * ip_ctx) {
    file_input_context_t * f_ctx = (file_input_context_t *)ip_ctx;
    wait_file_input_stop(f_ctx);
}

void * create_input_context_manual() {
    manual_input_context_t * m_ctx = create_manual_input_context();
    return (void *)m_ctx;
}

void * create_output_context_stos() {
    site2site_output_context_t * stos = create_s2s_output_context();
    return (void *)stos;
}

int set_output_property_stos(void * op_ctx, const char * name, const char * value) {
    if (!op_ctx || !name || !value) {
        return -1;
    }
    site2site_output_context_t * stos = (site2site_output_context_t *)op_ctx;
    return set_s2s_output_property(stos, name, value);
}

properties_t * get_output_properties_stos(void * op_ctx) {
    if (!op_ctx) return NULL;
    site2site_output_context_t * stos = (site2site_output_context_t *)op_ctx;
    return stos->output_properties;
}

int validate_output_properties_stos(void * op_ctx) {
    if (!op_ctx) return -1;
    site2site_output_context_t * stos = (site2site_output_context_t *)op_ctx;
    return validate_s2s_properties(stos);
}

void free_output_properties_stos(void * op_ctx) {
    if (!op_ctx) return;
    site2site_output_context_t * stos = (site2site_output_context_t *)op_ctx;
    free_s2s_output_properties(stos);
}

void free_output_context_stos(void * op_ctx) {
    if (!op_ctx) return;
    site2site_output_context_t * stos = (site2site_output_context_t *)op_ctx;
    free_s2s_output_context(stos);
}

properties_t * clone_output_props_stos(void * op_ctx) {
    site2site_output_context_t * stos = (site2site_output_context_t *)op_ctx;
    return clone_properties(stos->output_properties);
}

void wait_output_stop_stos(void * op_ctx) {
    site2site_output_context_t * stos = (site2site_output_context_t *)op_ctx;
    wait_s2s_output_stop(stos);
}
