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

#ifndef MINIFI_API_INCLUDE_MINIFI_C_MINIFI_API_H_
#define MINIFI_API_INCLUDE_MINIFI_C_MINIFI_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#if __STDC_VERSION__ < 202311l
#include <stdbool.h>
#endif  // < C23

#define MINIFI_PRIVATE_STRINGIFY_HELPER(X) #X
#define MINIFI_PRIVATE_STRINGIFY(X) MINIFI_PRIVATE_STRINGIFY_HELPER(X)

#define MINIFI_PRIVATE_JOIN_HELPER(X, Y) X##_##Y
#define MINIFI_PRIVATE_JOIN(X, Y) MINIFI_PRIVATE_JOIN_HELPER(X, Y)

#define MINIFI_OWNED
#define MINIFI_NULLABLE

#ifndef MINIFI_REGISTER_EXTENSION_FN
#define MINIFI_REGISTER_EXTENSION_FN minifi_register_extension
#endif

/// To declare a processor property that expects an SSLContextService,
/// use MINIFI_SSL_CONTEXT_SERVICE_PROPERTY_TYPE in the type field of the property definition (minifi_property_definition::type)
#define MINIFI_SSL_CONTEXT_SERVICE_PROPERTY_TYPE "org.apache.nifi.minifi.controllers.SSLContextServiceInterface"

/// To declare a processor property that expects an ProxyConfigurationService,
/// use MINIFI_PROXY_CONFIGURATION_SERVICE_PROPERTY_TYPE in the type field of the property definition (minifi_property_definition::type)
#define MINIFI_PROXY_CONFIGURATION_SERVICE_PROPERTY_TYPE "org.apache.nifi.minifi.controllers.ProxyConfigurationServiceInterface"

enum : uint32_t {
  MINIFI_API_VERSION = 1
};

enum minifi_io_status : int64_t {
  MINIFI_IO_ERROR = -1,
  MINIFI_IO_CANCEL = -125
};

enum minifi_input_requirement : uint32_t {
  MINIFI_INPUT_REQUIRED = 0,
  MINIFI_INPUT_ALLOWED = 1,
  MINIFI_INPUT_FORBIDDEN = 2
};

/// Represents a non-owning read-only view to a sized, not necessarily null-terminated string.
/// invariant: [data, data + length) is a valid range
struct minifi_string_view {
  /// nullable, non-owning pointer to the beginning of a continuous character sequence
  const char* data;
  /// the length of the buffer pointed-to by data
  size_t length;
};

/// Represents an output relationship of a processor, part of the processor metadata
struct minifi_relationship_definition {
  /// Name, processors use this to transfer flow files to the connections connected to this relationship using MinifiProcessSessionTransfer.
  struct minifi_string_view name;
  /// Human-readable description of what flow files get routed to this relationship. Included in C2 manifest and generated processor docs.
  struct minifi_string_view description;
};

struct minifi_output_attribute_definition {
  struct minifi_string_view name;
  size_t relationships_count;
  const struct minifi_string_view* relationships_ptr;
  struct minifi_string_view description;
};

/// Describes what the dynamic properties can be used for
struct minifi_dynamic_property_definition {
  struct minifi_string_view name;
  struct minifi_string_view value;
  struct minifi_string_view description;
  bool supports_expression_language;
};

struct minifi_flow_file;
struct minifi_logger;
struct minifi_process_context;
struct minifi_process_session;
struct minifi_controller_service;
struct minifi_controller_service_context;
struct minifi_input_stream;
struct minifi_output_stream;
struct minifi_extension;
struct minifi_extension_context;

enum minifi_status : uint32_t {
  MINIFI_STATUS_SUCCESS = 0,
  MINIFI_STATUS_UNKNOWN_ERROR = 1,
  MINIFI_STATUS_NOT_SUPPORTED_PROPERTY = 2,
  MINIFI_STATUS_DYNAMIC_PROPERTIES_NOT_SUPPORTED = 3,
  MINIFI_STATUS_PROPERTY_NOT_SET = 4,
  MINIFI_STATUS_VALIDATION_FAILED = 5,
  MINIFI_STATUS_PROCESSOR_YIELD = 6,
};

enum minifi_validator : uint32_t {
  MINIFI_VALIDATOR_ALWAYS_VALID = 0,
  MINIFI_VALIDATOR_NON_BLANK = 1,
  MINIFI_VALIDATOR_TIME_PERIOD = 2,
  MINIFI_VALIDATOR_BOOLEAN = 3,
  MINIFI_VALIDATOR_INTEGER = 4,
  MINIFI_VALIDATOR_UNSIGNED_INTEGER = 5,
  MINIFI_VALIDATOR_DATA_SIZE = 6,
  MINIFI_VALIDATOR_PORT = 7,
};

struct minifi_property_definition {
  struct minifi_string_view name;
  struct minifi_string_view display_name;
  struct minifi_string_view description;
  bool is_required;
  bool is_sensitive;

  const struct minifi_string_view* default_value;
  size_t allowed_values_count;
  const struct minifi_string_view* allowed_values_ptr;
  enum minifi_validator validator;

  const struct minifi_string_view* allowed_type;
  bool supports_expression_language;
};

enum minifi_log_level : uint32_t {
  MINIFI_LOG_LEVEL_TRACE = 0,
  MINIFI_LOG_LEVEL_DEBUG = 1,
  MINIFI_LOG_LEVEL_INFO = 2,
  MINIFI_LOG_LEVEL_WARNING = 3,
  MINIFI_LOG_LEVEL_ERROR = 4,
  MINIFI_LOG_LEVEL_CRITICAL = 5,
  MINIFI_LOG_LEVEL_OFF = 6
};

struct minifi_processor_metadata {
  struct minifi_string_view uuid;
  struct minifi_string_view name;
  struct minifi_logger* logger;  // borrowed non-null reference, live until the processor is live
};

struct minifi_controller_service_metadata {
  struct minifi_string_view uuid;
  struct minifi_string_view name;
  struct minifi_logger* logger;  // borrowed non-null reference, live until the controller service is live
};

struct minifi_processor_callbacks {
  MINIFI_OWNED void* (*create)(struct minifi_processor_metadata);
  void (*destroy)(MINIFI_OWNED void*);
  enum minifi_status (*trigger)(void*, struct minifi_process_context*, struct minifi_process_session*);
  enum minifi_status (*schedule)(void*, struct minifi_process_context*);
  void (*unschedule)(void*);
};

struct minifi_controller_service_callbacks {
  MINIFI_OWNED void* (*create)(struct minifi_controller_service_metadata);
  void (*destroy)(MINIFI_OWNED void*);
  enum minifi_status (*enable)(void*, struct minifi_controller_service_context*);
  void (*disable)(void*);
  void* (*get_interface)(void* ctx, struct minifi_string_view interface_type);
};

struct minifi_processor_class_definition {
  struct minifi_string_view full_name;  // '::'-delimited fully qualified name e.g. 'org::apache::nifi::minifi::GenerateFlowFile'
  struct minifi_string_view description;
  size_t properties_count;
  const struct minifi_property_definition* properties_ptr;
  size_t dynamic_properties_count;
  const struct minifi_dynamic_property_definition* dynamic_properties_ptr;
  size_t relationships_count;
  const struct minifi_relationship_definition* relationships_ptr;
  size_t output_attributes_count;
  const struct minifi_output_attribute_definition* output_attributes_ptr;
  bool supports_dynamic_properties;
  bool supports_dynamic_relationships;
  enum minifi_input_requirement input_requirement;
  bool is_single_threaded;

  struct minifi_processor_callbacks callbacks;
};

struct minifi_controller_service_class_definition {
  struct minifi_string_view full_name;  // '::'-delimited fully qualified name e.g. 'org::apache::nifi::minifi::extensions::gcp::GCPCredentialsControllerService
  struct minifi_string_view description;
  size_t properties_count;
  const struct minifi_property_definition* properties_ptr;
  size_t provided_interfaces_count;
  const struct minifi_string_view* provided_interfaces_ptr;

  struct minifi_controller_service_callbacks callbacks;
};

struct minifi_extension_definition {
  struct minifi_string_view name;
  struct minifi_string_view version;
  void (*deinit)(void* user_data);
  void* user_data;
};

//  When directly linking against the agent library (legacy c++ extension) this declares a build identifier dependent function
//  which prevents loading extensions from different builds (e.g. the agent provides MinifiRegisterCppExtension_123 but the extension
//  expects MinifiRegisterCppExtension_567). Otherwise, it declares minifi_register_extension.
struct minifi_extension* MINIFI_REGISTER_EXTENSION_FN(struct minifi_extension_context* extension_context,
    const struct minifi_extension_definition* extension_definition);

enum minifi_status minifi_register_processor(struct minifi_extension* extension, const struct minifi_processor_class_definition* processor);
enum minifi_status minifi_register_controller_service(struct minifi_extension* extension,
    const struct minifi_controller_service_class_definition* controller_service);

enum minifi_status minifi_process_context_set_trigger_when_empty(struct minifi_process_context*, bool);
enum minifi_status minifi_process_context_report_metrics(struct minifi_process_context*, size_t count, const struct minifi_string_view* metric_names,
    const double* metric_values);

enum minifi_status minifi_process_context_get_property(struct minifi_process_context* context, struct minifi_string_view property_name,
    MINIFI_NULLABLE struct minifi_flow_file* flowfile, void (*cb)(void* user_ctx, struct minifi_string_view property_value), void* user_ctx);

enum minifi_status minifi_process_context_get_controller_service_from_property(struct minifi_process_context* process_context,
    struct minifi_string_view property_name, struct minifi_string_view controller_service_type,
    struct minifi_controller_service** controller_service_out);
void minifi_process_context_get_dynamic_properties(struct minifi_process_context* context, MINIFI_NULLABLE struct minifi_flow_file* minifi_flow_file,
    void (*cb)(void* user_ctx, struct minifi_string_view dynamic_property_name, struct minifi_string_view dynamic_property_value), void* user_ctx);

void minifi_logger_log_string(struct minifi_logger*, enum minifi_log_level, struct minifi_string_view);
bool minifi_logger_should_log(struct minifi_logger*, enum minifi_log_level);

MINIFI_OWNED struct minifi_flow_file* minifi_process_session_get(struct minifi_process_session*);
MINIFI_OWNED struct minifi_flow_file* minifi_process_session_create(struct minifi_process_session* session,
    MINIFI_NULLABLE struct minifi_flow_file* parent_flowfile);

enum minifi_status minifi_process_session_penalize(struct minifi_process_session* session, struct minifi_flow_file* flowfile);
enum minifi_status minifi_process_session_transfer(struct minifi_process_session* session, MINIFI_OWNED struct minifi_flow_file* flowfile,
    struct minifi_string_view relationship_name);
enum minifi_status minifi_process_session_remove(struct minifi_process_session* session, MINIFI_OWNED struct minifi_flow_file* flowfile);

enum minifi_status minifi_process_session_read(struct minifi_process_session*, struct minifi_flow_file*,
    int64_t (*cb)(void* user_ctx, struct minifi_input_stream*), void* user_ctx);
enum minifi_status minifi_process_session_write(struct minifi_process_session*, struct minifi_flow_file*,
    int64_t (*cb)(void* user_ctx, struct minifi_output_stream*), void* user_ctx);

void minifi_config_get(struct minifi_extension_context* extension_context, struct minifi_string_view config_key,
    void (*cb)(void* user_ctx, struct minifi_string_view config_value), void* user_ctx);

size_t minifi_input_stream_size(struct minifi_input_stream*);

int64_t minifi_input_stream_read(struct minifi_input_stream* stream, char* buffer, size_t size);
int64_t minifi_output_stream_write(struct minifi_output_stream* stream, const char* data, size_t size);

enum minifi_status minifi_process_session_set_flow_file_attribute(struct minifi_process_session* session, struct minifi_flow_file* flowfile,
    struct minifi_string_view attribute_name, const struct minifi_string_view* attribute_value);
bool minifi_process_session_get_flow_file_attribute(struct minifi_process_session* session, struct minifi_flow_file* flowfile,
    struct minifi_string_view attribute_name, void (*cb)(void* user_ctx, struct minifi_string_view attribute_value), void* user_ctx);
void minifi_process_session_get_flow_file_attributes(struct minifi_process_session* session, struct minifi_flow_file* flowfile,
    void (*cb)(void* user_ctx, struct minifi_string_view attribute_name, struct minifi_string_view attribute_value), void* user_ctx);
uint64_t minifi_process_session_get_flow_file_size(struct minifi_process_session* session, struct minifi_flow_file* flowfile);
enum minifi_status minifi_process_session_get_flow_file_id(struct minifi_process_session* session, struct minifi_flow_file* flowfile,
    void (*cb)(void* user_ctx, struct minifi_string_view flow_file_id), void* user_ctx);

enum minifi_status minifi_controller_service_context_get_property(struct minifi_controller_service_context* context,
    struct minifi_string_view property_name, void (*cb)(void* user_ctx, struct minifi_string_view property_value), void* user_ctx);

struct minifi_ssl_data {
  struct minifi_string_view ca_certificate_file;
  struct minifi_string_view certificate_file;
  struct minifi_string_view private_key_file;
  struct minifi_string_view passphrase;
};

enum minifi_status minifi_process_context_get_ssl_data_from_property(struct minifi_process_context* process_context,
    struct minifi_string_view property_name, void (*cb)(void* user_ctx, const struct minifi_ssl_data* ssl_data), void* user_ctx);

enum minifi_proxy_type : uint8_t {
  MINIFI_PROXY_TYPE_DIRECT,
  MINIFI_PROXY_TYPE_HTTP
};

struct minifi_proxy_data {
  enum minifi_proxy_type proxy_type;
  struct minifi_string_view hostname;
  uint16_t port;
  struct minifi_string_view* username;
  struct minifi_string_view* password;
};

enum minifi_status minifi_process_context_get_proxy_data_from_property(struct minifi_process_context* process_context,
    struct minifi_string_view property_name, void (*cb)(void* user_ctx, const struct minifi_proxy_data* proxy_data), void* user_ctx);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // MINIFI_API_INCLUDE_MINIFI_C_MINIFI_API_H_
