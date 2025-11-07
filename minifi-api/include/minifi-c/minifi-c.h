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
#include <stdbool.h>

#define STRINGIFY_HELPER(X) #X
#define STRINGIFY(X) STRINGIFY_HELPER(X)

#define MINIFI_API_MAJOR_VERSION 0
#define MINIFI_API_MINOR_VERSION 1
#define MINIFI_API_PATCH_VERSION 0
#define MINIFI_API_VERSION STRINGIFY(MINIFI_API_MAJOR_VERSION) "." STRINGIFY(MINIFI_API_MINOR_VERSION) "." STRINGIFY(MINIFI_API_PATCH_VERSION)
#define MINIFI_API_VERSION_TAG "MINIFI_API_VERSION=[" MINIFI_API_VERSION "]"
#define MINIFI_NULL 0
#define MINIFI_OWNED

typedef bool MinifiBool;
#define MINIFI_TRUE true
#define MINIFI_FALSE false

typedef enum MinifiInputRequirement {
  MINIFI_INPUT_REQUIRED = 0,
  MINIFI_INPUT_ALLOWED = 1,
  MINIFI_INPUT_FORBIDDEN = 2
} MinifiInputRequirement;

typedef struct MinifiStringView {
  const char* data;
  size_t length;
} MinifiStringView;

typedef struct MinifiRelationship {
  MinifiStringView name;
  MinifiStringView description;
} MinifiRelationship;

typedef struct MinifiOutputAttribute {
  MinifiStringView name;
  size_t relationships_count;
  const MinifiRelationship* relationships_ptr;
  MinifiStringView description;
} MinifiOutputAttribute;

typedef struct MinifiDynamicProperty {
  MinifiStringView name;
  MinifiStringView value;
  MinifiStringView description;
  MinifiBool supports_expression_language;
} MinifiDynamicProperty;

typedef struct MinifiPropertyValidator MinifiPropertyValidator;
typedef struct MinifiFlowFile MinifiFlowFile;
typedef struct MinifiLogger MinifiLogger;
typedef struct MinifiProcessContext MinifiProcessContext;
typedef struct MinifiProcessSession MinifiProcessSession;
typedef struct MinifiInputStream MinifiInputStream;
typedef struct MinifiOutputStream MinifiOutputStream;
typedef struct MinifiConfig MinifiConfig;
typedef struct MinifiExtension MinifiExtension;
typedef struct MinifiPublishedMetrics MinifiPublishedMetrics;

typedef enum MinifiStatus {
  MINIFI_STATUS_SUCCESS = 0,
  MINIFI_STATUS_UNKNOWN_ERROR = 1,
  MINIFI_STATUS_NOT_SUPPORTED_PROPERTY = 2,
  MINIFI_STATUS_DYNAMIC_PROPERTIES_NOT_SUPPORTED = 3,
  MINIFI_STATUS_PROPERTY_NOT_SET = 4,
  MINIFI_STATUS_VALIDATION_FAILED = 5,
  MINIFI_STATUS_PROCESSOR_YIELD = 6
} MinifiStatus;

typedef struct MinifiProperty {
  MinifiStringView name;
  MinifiStringView display_name;
  MinifiStringView description;
  MinifiBool is_required;
  MinifiBool is_sensitive;
  size_t dependent_properties_count;
  const MinifiStringView* dependent_properties_ptr;
  size_t exclusive_of_properties_count;
  const MinifiStringView* exclusive_of_property_names_ptr;
  const MinifiStringView* exclusive_of_property_values_ptr;

  const MinifiStringView* default_value;
  size_t allowed_values_count;
  const MinifiStringView* allowed_values_ptr;
  const MinifiPropertyValidator* validator;

  const MinifiStringView* type;
  MinifiBool supports_expression_language;
} MinifiProperty;

typedef enum MinifiLogLevel {
  MINIFI_TRACE = 0,
  MINIFI_DEBUG = 1,
  MINIFI_INFO = 2,
  MINIFI_WARNING = 3,
  MINIFI_ERROR = 4,
  MINIFI_CRITICAL = 5,
  MINIFI_OFF = 6
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

typedef struct MinifiProcessorClassDescription {
  MinifiStringView full_name;  // '::'-delimited fully qualified name e.g. 'org::apache::nifi::minifi::GenerateFlowFile'
  MinifiStringView description;
  size_t class_properties_count;
  const MinifiProperty* class_properties_ptr;
  size_t dynamic_properties_count;
  const MinifiDynamicProperty* dynamic_properties_ptr;
  size_t class_relationships_count;
  const MinifiRelationship* class_relationships_ptr;
  size_t output_attributes_count;
  const MinifiOutputAttribute* output_attributes_ptr;
  MinifiBool supports_dynamic_properties;
  MinifiBool supports_dynamic_relationships;
  MinifiInputRequirement input_requirement;
  MinifiBool is_single_threaded;

  MinifiProcessorCallbacks callbacks;
} MinifiProcessorClassDescription;

typedef enum MinifiStandardPropertyValidator {
  MINIFI_ALWAYS_VALID_VALIDATOR = 0,
  MINIFI_NON_BLANK_VALIDATOR = 1,
  MINIFI_TIME_PERIOD_VALIDATOR = 2,
  MINIFI_BOOLEAN_VALIDATOR = 3,
  MINIFI_INTEGER_VALIDATOR = 4,
  MINIFI_UNSIGNED_INTEGER_VALIDATOR = 5,
  MINIFI_DATA_SIZE_VALIDATOR = 6,
  MINIFI_PORT_VALIDATOR = 7
} MinifiStandardPropertyValidator;

typedef struct MinifiExtensionCreateInfo {
  MinifiStringView name;
  MinifiStringView version;
  void(*deinit)(void*);
  void* user_data;
  size_t processors_count;
  const MinifiProcessorClassDescription* processors_ptr;
} MinifiExtensionCreateInfo;

MinifiExtension* MinifiCreateExtension(const MinifiExtensionCreateInfo*);

const MinifiPropertyValidator* MinifiGetStandardValidator(MinifiStandardPropertyValidator);

MINIFI_OWNED MinifiPublishedMetrics* MinifiPublishedMetricsCreate(size_t count, const MinifiStringView* metric_name, const double* metric_value);

MinifiStatus MinifiProcessContextGetProperty(MinifiProcessContext* context, MinifiStringView property_name, MinifiFlowFile* flowfile, void(*cb)(void* user_ctx, MinifiStringView property_value), void* user_ctx);
void MinifiProcessContextGetProcessorName(MinifiProcessContext* context, void(*cb)(void* user_ctx, MinifiStringView processor_name), void* user_ctx);
MinifiBool MinifiProcessContextHasNonEmptyProperty(MinifiProcessContext* context, MinifiStringView property_name);

void MinifiLoggerSetMaxLogSize(MinifiLogger*, int32_t);
void MinifiLoggerLogString(MinifiLogger*, MinifiLogLevel, MinifiStringView);
MinifiBool MinifiLoggerShouldLog(MinifiLogger*, MinifiLogLevel);

MINIFI_OWNED MinifiFlowFile* MinifiProcessSessionGet(MinifiProcessSession*);
MINIFI_OWNED MinifiFlowFile* MinifiProcessSessionCreate(MinifiProcessSession* session, MinifiFlowFile* parent_flowfile);

void MinifiProcessSessionTransfer(MinifiProcessSession* session, MINIFI_OWNED MinifiFlowFile* flowfile, MinifiStringView relationship);
void MinifiProcessSessionRemove(MinifiProcessSession* session, MINIFI_OWNED MinifiFlowFile* flowfile);

MinifiStatus MinifiProcessSessionRead(MinifiProcessSession*, MinifiFlowFile*, int64_t(*cb)(void* user_ctx, MinifiInputStream*), void* user_ctx);
MinifiStatus MinifiProcessSessionWrite(MinifiProcessSession*, MinifiFlowFile*, int64_t(*cb)(void* user_ctx, MinifiOutputStream*), void* user_ctx);

void MinifiConfigGet(MinifiConfig* config, MinifiStringView config_key, void(*cb)(void* user_ctx, MinifiStringView config_value), void* user_ctx);

size_t MinifiInputStreamSize(MinifiInputStream*);

int64_t MinifiInputStreamRead(MinifiInputStream* stream, char* buffer, size_t size);
int64_t MinifiOutputStreamWrite(MinifiOutputStream* stream, const char* data, size_t size);

void MinifiStatusToString(MinifiStatus, void(*cb)(void* user_ctx, MinifiStringView str), void* user_ctx);

void MinifiFlowFileSetAttribute(MinifiProcessSession* session, MinifiFlowFile* flowfile, MinifiStringView attribute_name, const MinifiStringView* attribute_value);
MinifiBool MinifiFlowFileGetAttribute(MinifiProcessSession* session, MinifiFlowFile* flowfile, MinifiStringView attribute_name, void(*cb)(void* user_ctx, MinifiStringView attribute_value), void* user_ctx);
void MinifiFlowFileGetAttributes(MinifiProcessSession* session, MinifiFlowFile* flowfile, void(*cb)(void* user_ctx, MinifiStringView attribute_name, MinifiStringView attribute_value), void* user_ctx);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // MINIFI_API_INCLUDE_MINIFI_C_MINIFI_C_H_
