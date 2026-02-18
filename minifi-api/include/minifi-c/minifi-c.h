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

#ifndef MINIFI_API_INCLUDE_MINIFI_C_MINIFI_C_H_
#define MINIFI_API_INCLUDE_MINIFI_C_MINIFI_C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#if __STDC_VERSION__ < 202311l
#  include <stdbool.h>
#endif  // < C23

#define MINIFI_PRIVATE_STRINGIFY_HELPER(X) #X
#define MINIFI_PRIVATE_STRINGIFY(X) MINIFI_PRIVATE_STRINGIFY_HELPER(X)

#define MINIFI_API_MAJOR_VERSION 0
#define MINIFI_API_MINOR_VERSION 1
#define MINIFI_API_PATCH_VERSION 0
#define MINIFI_API_VERSION MINIFI_PRIVATE_STRINGIFY(MINIFI_API_MAJOR_VERSION) "." MINIFI_PRIVATE_STRINGIFY(MINIFI_API_MINOR_VERSION) "." MINIFI_PRIVATE_STRINGIFY(MINIFI_API_PATCH_VERSION)
#define MINIFI_API_VERSION_TAG "MINIFI_API_VERSION=[" MINIFI_API_VERSION "]"
#define MINIFI_NULL nullptr
#define MINIFI_OWNED

typedef bool MinifiBool;

typedef enum MinifiInputRequirement : uint32_t {
  MINIFI_INPUT_REQUIRED = 0,
  MINIFI_INPUT_ALLOWED = 1,
  MINIFI_INPUT_FORBIDDEN = 2
} MinifiInputRequirement;

/// Represents a non-owning read-only view to a sized, not necessarily null-terminated string.
/// invariant: [data, data + length) is a valid range
typedef struct MinifiStringView {
  /// nullable, non-owning pointer to the beginning of a continuous character sequence
  const char* data;
  /// the length of the buffer pointed-to by data
  size_t length;
} MinifiStringView;

/// Represents an output relationship of a processor, part of the processor metadata
typedef struct MinifiRelationshipDefinition {
  /// Name, processors use this to transfer flow files to the connections connected to this relationship using MinifiProcessSessionTransfer.
  MinifiStringView name;
  /// Human-readable description of what flow files get routed to this relationship. Included in C2 manifest and generated processor docs.
  MinifiStringView description;
} MinifiRelationshipDefinition;

typedef struct MinifiOutputAttributeDefinition {
  MinifiStringView name;
  size_t relationships_count;
  const MinifiStringView* relationships_ptr;
  MinifiStringView description;
} MinifiOutputAttributeDefinition;

typedef struct MinifiDynamicPropertyDefinition {
  MinifiStringView name;
  MinifiStringView value;
  MinifiStringView description;
  MinifiBool supports_expression_language;
} MinifiDynamicPropertyDefinition;

typedef struct MinifiFlowFile MinifiFlowFile;
typedef struct MinifiLogger MinifiLogger;
typedef struct MinifiProcessContext MinifiProcessContext;
typedef struct MinifiProcessSession MinifiProcessSession;
typedef struct MinifiInputStream MinifiInputStream;
typedef struct MinifiOutputStream MinifiOutputStream;
typedef struct MinifiConfig MinifiConfig;
typedef struct MinifiExtension MinifiExtension;
typedef struct MinifiPublishedMetrics MinifiPublishedMetrics;

typedef enum MinifiStatus : uint32_t {
  MINIFI_STATUS_SUCCESS = 0,
  MINIFI_STATUS_UNKNOWN_ERROR = 1,
  MINIFI_STATUS_NOT_SUPPORTED_PROPERTY = 2,
  MINIFI_STATUS_DYNAMIC_PROPERTIES_NOT_SUPPORTED = 3,
  MINIFI_STATUS_PROPERTY_NOT_SET = 4,
  MINIFI_STATUS_VALIDATION_FAILED = 5,
  MINIFI_STATUS_PROCESSOR_YIELD = 6
} MinifiStatus;

typedef enum MinifiValidator : uint32_t {
  MINIFI_VALIDATOR_ALWAYS_VALID = 0,
  MINIFI_VALIDATOR_NON_BLANK = 1,
  MINIFI_VALIDATOR_TIME_PERIOD = 2,
  MINIFI_VALIDATOR_BOOLEAN = 3,
  MINIFI_VALIDATOR_INTEGER = 4,
  MINIFI_VALIDATOR_UNSIGNED_INTEGER = 5,
  MINIFI_VALIDATOR_DATA_SIZE = 6,
  MINIFI_VALIDATOR_PORT = 7
} MinifiValidator;

typedef struct MinifiPropertyDefinition {
  MinifiStringView name;
  MinifiStringView display_name;
  MinifiStringView description;
  MinifiBool is_required;
  MinifiBool is_sensitive;

  const MinifiStringView* default_value;
  size_t allowed_values_count;
  const MinifiStringView* allowed_values_ptr;
  MinifiValidator validator;

  const MinifiStringView* type;
  MinifiBool supports_expression_language;
} MinifiPropertyDefinition;

typedef enum MinifiLogLevel : uint32_t {
  MINIFI_LOG_LEVEL_TRACE = 0,
  MINIFI_LOG_LEVEL_DEBUG = 1,
  MINIFI_LOG_LEVEL_INFO = 2,
  MINIFI_LOG_LEVEL_WARNING = 3,
  MINIFI_LOG_LEVEL_ERROR = 4,
  MINIFI_LOG_LEVEL_CRITICAL = 5,
  MINIFI_LOG_LEVEL_OFF = 6
} MinifiLogLevel;

typedef struct MinifiProcessorMetadata {
  MinifiStringView uuid;
  MinifiStringView name;
  MinifiLogger* logger;  // borrowed reference, live until the processor is live
} MinifiProcessorMetadata;

typedef struct MinifiProcessorCallbacks {
  MINIFI_OWNED void*(*create)(MinifiProcessorMetadata);
  void(*destroy)(MINIFI_OWNED void*);
  MinifiBool(*isWorkAvailable)(void*);
  void(*restore)(void*, MINIFI_OWNED MinifiFlowFile*);
  MinifiBool(*getTriggerWhenEmpty)(void*);
  MinifiStatus(*onTrigger)(void*, MinifiProcessContext*, MinifiProcessSession*);
  MinifiStatus(*onSchedule)(void*, MinifiProcessContext*);
  void(*onUnSchedule)(void*);
  MINIFI_OWNED MinifiPublishedMetrics*(*calculateMetrics)(void*);
} MinifiProcessorCallbacks;

typedef struct MinifiProcessorClassDefinition {
  MinifiStringView full_name;  // '::'-delimited fully qualified name e.g. 'org::apache::nifi::minifi::GenerateFlowFile'
  MinifiStringView description;
  size_t class_properties_count;
  const MinifiPropertyDefinition* class_properties_ptr;
  size_t dynamic_properties_count;
  const MinifiDynamicPropertyDefinition* dynamic_properties_ptr;
  size_t class_relationships_count;
  const MinifiRelationshipDefinition* class_relationships_ptr;
  size_t output_attributes_count;
  const MinifiOutputAttributeDefinition* output_attributes_ptr;
  MinifiBool supports_dynamic_properties;
  MinifiBool supports_dynamic_relationships;
  MinifiInputRequirement input_requirement;
  MinifiBool is_single_threaded;

  MinifiProcessorCallbacks callbacks;
} MinifiProcessorClassDefinition;

typedef struct MinifiExtensionCreateInfo {
  MinifiStringView name;
  MinifiStringView version;
  void(*deinit)(void* user_data);
  void* user_data;
  size_t processors_count;
  const MinifiProcessorClassDefinition* processors_ptr;
} MinifiExtensionCreateInfo;

// api_version is used to provide backwards compatible changes to the MinifiExtensionCreateInfo structure,
// e.g. if MinifiExtensionCreateInfo gets a new field in version 1.2.0, extensions built with api 1.1.0 do not
// have to be rebuilt
MinifiExtension* MinifiCreateExtension(MinifiStringView api_version, const MinifiExtensionCreateInfo*);

MINIFI_OWNED MinifiPublishedMetrics* MinifiPublishedMetricsCreate(size_t count, const MinifiStringView* metric_names, const double* metric_values);

MinifiStatus MinifiProcessContextGetProperty(MinifiProcessContext* context, MinifiStringView property_name, MinifiFlowFile* flowfile,
                                             void(*cb)(void* user_ctx, MinifiStringView property_value), void* user_ctx);
void MinifiProcessContextGetProcessorName(MinifiProcessContext* context, void(*cb)(void* user_ctx, MinifiStringView processor_name), void* user_ctx);
MinifiBool MinifiProcessContextHasNonEmptyProperty(MinifiProcessContext* context, MinifiStringView property_name);

void MinifiLoggerSetMaxLogSize(MinifiLogger*, int32_t);
void MinifiLoggerLogString(MinifiLogger*, MinifiLogLevel, MinifiStringView);
MinifiBool MinifiLoggerShouldLog(MinifiLogger*, MinifiLogLevel);
MinifiLogLevel MinifiLoggerLevel(MinifiLogger*);

MINIFI_OWNED MinifiFlowFile* MinifiProcessSessionGet(MinifiProcessSession*);
MINIFI_OWNED MinifiFlowFile* MinifiProcessSessionCreate(MinifiProcessSession* session, MinifiFlowFile* parent_flowfile);

void MinifiProcessSessionTransfer(MinifiProcessSession* session, MINIFI_OWNED MinifiFlowFile* flowfile, MinifiStringView relationship_name);
void MinifiProcessSessionRemove(MinifiProcessSession* session, MINIFI_OWNED MinifiFlowFile* flowfile);

MinifiStatus MinifiProcessSessionRead(MinifiProcessSession*, MinifiFlowFile*, int64_t(*cb)(void* user_ctx, MinifiInputStream*), void* user_ctx);
MinifiStatus MinifiProcessSessionWrite(MinifiProcessSession*, MinifiFlowFile*, int64_t(*cb)(void* user_ctx, MinifiOutputStream*), void* user_ctx);

void MinifiConfigGet(MinifiConfig* config, MinifiStringView config_key, void(*cb)(void* user_ctx, MinifiStringView config_value), void* user_ctx);

size_t MinifiInputStreamSize(MinifiInputStream*);

int64_t MinifiInputStreamRead(MinifiInputStream* stream, char* buffer, size_t size);
int64_t MinifiOutputStreamWrite(MinifiOutputStream* stream, const char* data, size_t size);

void MinifiStatusToString(MinifiStatus, void(*cb)(void* user_ctx, MinifiStringView str), void* user_ctx);

void MinifiFlowFileSetAttribute(MinifiProcessSession* session, MinifiFlowFile* flowfile, MinifiStringView attribute_name, const MinifiStringView* attribute_value);
MinifiBool MinifiFlowFileGetAttribute(MinifiProcessSession* session, MinifiFlowFile* flowfile, MinifiStringView attribute_name,
                                      void(*cb)(void* user_ctx, MinifiStringView attribute_value), void* user_ctx);
void MinifiFlowFileGetAttributes(MinifiProcessSession* session, MinifiFlowFile* flowfile, void(*cb)(void* user_ctx, MinifiStringView attribute_name, MinifiStringView attribute_value), void* user_ctx);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // MINIFI_API_INCLUDE_MINIFI_C_MINIFI_C_H_
