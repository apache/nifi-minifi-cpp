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

#define STRINGIFY_HELPER(X) #X
#define STRINGIFY(X) STRINGIFY_HELPER(X)

#define MINIFI_API_MAJOR_VERSION 0
#define MINIFI_API_MINOR_VERSION 1
#define MINIFI_API_PATCH_VERSION 0
#define MINIFI_API_VERSION STRINGIFY(MINIFI_API_MAJOR_VERSION) "." STRINGIFY(MINIFI_API_MINOR_VERSION) "." STRINGIFY(MINIFI_API_PATCH_VERSION)
#define MINIFI_API_VERSION_TAG "MINIFI_API_VERSION=[" MINIFI_API_VERSION "]"
#define MINIFI_NULL 0
#define OWNED

typedef uint32_t MinifiBool;
#define MINIFI_TRUE MinifiBool(1)
#define MINIFI_FALSE MinifiBool(0)

typedef enum MinifiInputRequirement {
  MINIFI_INPUT_REQUIRED = 0,
  MINIFI_INPUT_ALLOWED = 1,
  MINIFI_INPUT_FORBIDDEN = 2
} MinifiInputRequirement;

typedef struct MinifiStringView {
  const char* data;
  uint32_t length;
} MinifiStringView;

typedef struct MinifiRelationship {
  MinifiStringView name;
  MinifiStringView description;
} MinifiRelationship;

typedef struct MinifiOutputAttribute {
  MinifiStringView name;
  uint32_t relationships_count;
  const MinifiRelationship* relationships_ptr;
  MinifiStringView description;
} MinifiOutputAttribute;

typedef struct MinifiDynamicProperty {
  MinifiStringView name;
  MinifiStringView value;
  MinifiStringView description;
  MinifiBool supports_expression_language;
} MinifiDynamicProperty;

#define DECLARE_HANDLE(name) typedef struct name ## _T* name
#define DECLARE_CONST_HANDLE(name) typedef const struct name ## _T* name

DECLARE_CONST_HANDLE(MinifiPropertyValidator);
DECLARE_HANDLE(MinifiFlowFile);
DECLARE_HANDLE(MinifiLogger);
DECLARE_HANDLE(MinifiProcessContext);
DECLARE_HANDLE(MinifiProcessSession);
DECLARE_HANDLE(MinifiInputStream);
DECLARE_HANDLE(MinifiOutputStream);
DECLARE_HANDLE(MinifiConfigure);
DECLARE_HANDLE(MinifiExtension);
DECLARE_HANDLE(MinifiPublishedMetrics);

typedef struct MinifiExtensionCreateInfo {
  MinifiStringView name;
  MinifiBool(*initialize)(void*, MinifiConfigure);
  void* user_data;
} MinifiExtensionCreateInfo;

typedef enum MinifiStatus {
  MINIFI_SUCCESS = 0,
  MINIFI_UNKNOWN_ERROR = 1,
  MINIFI_NOT_SUPPORTED_PROPERTY = 2,
  MINIFI_DYNAMIC_PROPERTIES_NOT_SUPPORTED = 3,
  MINIFI_PROPERTY_NOT_SET = 4,
  MINIFI_VALIDATION_FAILED = 5
} MinifiStatus;

typedef struct MinifiProperty {
  MinifiStringView name;
  MinifiStringView display_name;
  MinifiStringView description;
  MinifiBool is_required;
  MinifiBool is_sensitive;
  uint32_t dependent_properties_count;
  const MinifiStringView* dependent_properties_ptr;
  uint32_t exclusive_of_properties_count;
  const MinifiStringView* exclusive_of_property_names_ptr;
  const MinifiStringView* exclusive_of_property_values_ptr;

  const MinifiStringView* default_value;
  uint32_t allowed_values_count;
  const MinifiStringView* allowed_values_ptr;
  MinifiPropertyValidator validator;

  uint32_t types_count;
  const MinifiStringView* types_ptr;
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
  MinifiLogger logger;  // borrowed reference, live until the processor is live
} MinifiProcessorMetadata;

typedef struct MinifiProcessorCallbacks {
  OWNED void*(*create)(MinifiProcessorMetadata);
  void(*destroy)(OWNED void*);
  MinifiBool(*isWorkAvailable)(void*);
  void(*restore)(void*, OWNED MinifiFlowFile);
  MinifiBool(*getTriggerWhenEmpty)(void*);
  MinifiStatus(*onTrigger)(void*, MinifiProcessContext, MinifiProcessSession);
  MinifiStatus(*onSchedule)(void*, MinifiProcessContext);
  void(*onUnSchedule)(void*);
  OWNED MinifiPublishedMetrics(*calculateMetrics)(void*);
} MinifiProcessorCallbacks;

typedef struct MinifiProcessorClassDescription {
  MinifiStringView module_name;
  MinifiStringView full_name;  // '::'-delimited fully qualified name e.g. 'org::apache::nifi::minifi::GenerateFlowFile'
  MinifiStringView description;
  uint32_t class_properties_count;
  const MinifiProperty* class_properties_ptr;
  uint32_t dynamic_properties_count;
  const MinifiDynamicProperty* dynamic_properties_ptr;
  uint32_t class_relationships_count;
  const MinifiRelationship* class_relationships_ptr;
  uint32_t output_attributes_count;
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

OWNED MinifiExtension MinifiCreateExtension(const MinifiExtensionCreateInfo*);
void MinifiDestroyExtension(OWNED MinifiExtension);

MinifiPropertyValidator MinifiGetStandardValidator(MinifiStandardPropertyValidator);
void MinifiRegisterProcessorClass(const MinifiProcessorClassDescription*);

OWNED MinifiPublishedMetrics MinifiPublishedMetricsCreate(uint32_t count, const MinifiStringView*, const double*);

MinifiStatus MinifiProcessContextGetProperty(MinifiProcessContext, MinifiStringView, MinifiFlowFile, void(*result_cb)(void* user_ctx, MinifiStringView result), void* user_ctx);
void MinifiProcessContextYield(MinifiProcessContext);
void MinifiProcessContextGetProcessorName(MinifiProcessContext, void(*result_cb)(void* user_ctx, MinifiStringView result), void* user_ctx);
MinifiBool MinifiProcessContextHasNonEmptyProperty(MinifiProcessContext, MinifiStringView);

void MinifiLoggerSetMaxLogSize(MinifiLogger, int32_t);
void MinifiLoggerGetId(MinifiLogger, void(*cb)(void* user_ctx, MinifiStringView id), void* user_ctx);
void MinifiLoggerLogString(MinifiLogger, MinifiLogLevel, MinifiStringView);
MinifiBool MinifiLoggerShouldLog(MinifiLogger, MinifiLogLevel);
MinifiLogLevel MinifiLoggerLevel(MinifiLogger);
int32_t MinifiLoggerGetMaxLogSize(MinifiLogger);

OWNED MinifiFlowFile MinifiProcessSessionGet(MinifiProcessSession);
OWNED MinifiFlowFile MinifiProcessSessionCreate(MinifiProcessSession, MinifiFlowFile);
void MinifiDestroyFlowFile(OWNED MinifiFlowFile);
void MinifiProcessSessionTransfer(MinifiProcessSession, MinifiFlowFile, MinifiStringView);
void MinifiProcessSessionRemove(MinifiProcessSession, MinifiFlowFile);
MinifiStatus MinifiProcessSessionRead(MinifiProcessSession, MinifiFlowFile, int64_t(*cb)(void* user_ctx, MinifiInputStream), void* user_ctx);
MinifiStatus MinifiProcessSessionWrite(MinifiProcessSession, MinifiFlowFile, int64_t(*cb)(void* user_ctx, MinifiOutputStream), void* user_ctx);

void MinifiConfigureGet(MinifiConfigure, MinifiStringView, void(*cb)(void*, MinifiStringView), void*);

uint64_t MinifiInputStreamSize(MinifiInputStream);

int64_t MinifiInputStreamRead(MinifiInputStream, char*, uint64_t);
int64_t MinifiOutputStreamWrite(MinifiOutputStream, const char*, uint64_t);

void MinifiStatusToString(MinifiStatus, void(*cb)(void* user_ctx, MinifiStringView str), void* user_ctx);

void MinifiFlowFileSetAttribute(MinifiProcessSession, MinifiFlowFile, MinifiStringView, const MinifiStringView*);
MinifiBool MinifiFlowFileGetAttribute(MinifiProcessSession, MinifiFlowFile, MinifiStringView, void(*cb)(void* user_ctx, MinifiStringView), void* user_ctx);
void MinifiFlowFileGetAttributes(MinifiProcessSession, MinifiFlowFile, void(*cb)(void* user_ctx, MinifiStringView, MinifiStringView), void* user_ctx);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // MINIFI_API_INCLUDE_MINIFI_C_MINIFI_C_H_
