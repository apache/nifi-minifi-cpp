/**
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

#include <stdexcept>

#include "minifi-api.h"

extern "C" {
minifi_extension* MINIFI_REGISTER_EXTENSION_FN(minifi_extension_context*, const minifi_extension_definition*) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_register_processor(minifi_extension*, const minifi_processor_class_definition*) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_register_controller_service(minifi_extension*, const minifi_controller_service_class_definition*) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_process_context_get_property(minifi_process_context*, minifi_string_view, minifi_flow_file*,
    void (*)(void* user_ctx, minifi_string_view property_value), void*) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_process_context_get_controller_service_from_property(minifi_process_context*, minifi_string_view, minifi_string_view,
    minifi_controller_service**) {
  throw std::runtime_error("Not implemented");
}

void minifi_process_context_get_dynamic_properties(minifi_process_context*, minifi_flow_file*,
    void (*)(void* user_ctx, minifi_string_view dynamic_property_name, minifi_string_view dynamic_property_value), void*) {
  throw std::runtime_error("Not implemented");
}

void minifi_logger_log_string(minifi_logger*, minifi_log_level, minifi_string_view) {
  throw std::runtime_error("Not implemented");
}

bool minifi_logger_should_log(minifi_logger*, minifi_log_level) {
  throw std::runtime_error("Not implemented");
}

MINIFI_OWNED minifi_flow_file* minifi_process_session_get(minifi_process_session*) {
  throw std::runtime_error("Not implemented");
}

MINIFI_OWNED minifi_flow_file* minifi_process_session_create(minifi_process_session*, minifi_flow_file*) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_process_session_penalize(minifi_process_session*, minifi_flow_file*) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_process_session_transfer(minifi_process_session*, MINIFI_OWNED minifi_flow_file*, minifi_string_view) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_process_session_remove(minifi_process_session*, MINIFI_OWNED minifi_flow_file*) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_process_session_read(minifi_process_session*, minifi_flow_file*, int64_t (*)(void* user_ctx, minifi_input_stream*), void*) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_process_session_write(minifi_process_session*, minifi_flow_file*, int64_t (*)(void* user_ctx, minifi_output_stream*), void*) {
  throw std::runtime_error("Not implemented");
}

void minifi_config_get(minifi_extension_context*, minifi_string_view, void (*)(void* user_ctx, minifi_string_view config_value), void*) {
  throw std::runtime_error("Not implemented");
}

size_t minifi_input_stream_size(minifi_input_stream*) {
  throw std::runtime_error("Not implemented");
}

int64_t minifi_input_stream_read(minifi_input_stream*, char*, size_t) {
  throw std::runtime_error("Not implemented");
}

int64_t minifi_output_stream_write(minifi_output_stream*, const char*, size_t) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_process_session_set_flow_file_attribute(minifi_process_session*, minifi_flow_file*, minifi_string_view,
    const minifi_string_view*) {
  throw std::runtime_error("Not implemented");
}

bool minifi_process_session_get_flow_file_attribute(minifi_process_session*, minifi_flow_file*, minifi_string_view,
    void (*)(void* user_ctx, minifi_string_view attribute_value), void*) {
  throw std::runtime_error("Not implemented");
}

void minifi_process_session_get_flow_file_attributes(minifi_process_session*, minifi_flow_file*,
    void (*)(void* user_ctx, minifi_string_view attribute_name, minifi_string_view attribute_value), void*) {
  throw std::runtime_error("Not implemented");
}

uint64_t minifi_process_session_get_flow_file_size(minifi_process_session*, minifi_flow_file*) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_process_session_get_flow_file_id(minifi_process_session*, minifi_flow_file*,
    void (*)(void* user_ctx, minifi_string_view flow_file_id), void*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_controller_service_context_get_property(struct minifi_controller_service_context*, minifi_string_view,
    void (*)(void* user_ctx, minifi_string_view property_value), void*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_process_context_get_ssl_data_from_property(struct minifi_process_context*, struct minifi_string_view,
    void (*)(void*, const struct minifi_ssl_data*), void*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_process_context_get_proxy_data_from_property(struct minifi_process_context*, struct minifi_string_view,
    void (*)(void*, const struct minifi_proxy_data*), void*) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_process_context_set_trigger_when_empty(minifi_process_context*, bool) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_process_context_report_metrics(minifi_process_context*, size_t, const minifi_string_view*, const double*) {
  throw std::runtime_error("Not implemented");
}

}  // extern "C"
