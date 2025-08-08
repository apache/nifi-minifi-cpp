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

#define MINIFI_VERSION 100
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

typedef struct MinifiPropertyValidator_T* MinifiPropertyValidator;
typedef struct MinifiFlowFile_T* MinifiFlowFile;
typedef struct MinifiLogger_T* MinifiLogger;
typedef struct MinifiProcessorDescriptor_T* MinifiProcessorDescriptor;
typedef struct MinifiSerializedResponseNodeVec_T* MinifiSerializedResponseNodeVec;
typedef struct MinifiPublishedMetricVec_T* MinifiPublishedMetricVec;
typedef struct MinifiLoggerCallback_T* MinifiLoggerCallback;
typedef struct MinifiProcessContext_T* MinifiProcessContext;
typedef struct MinifiProcessSessionFactory_T* MinifiProcessSessionFactory;
typedef struct MinifiProcessSession_T* MinifiProcessSession;
typedef struct MinifiInputStream_T* MinifiInputStream;
typedef struct MinifiOutputStream_T* MinifiOutputStream;
typedef struct MinifiString_T* MinifiString;
typedef struct MinifiConfigure_T* MinifiConfigure;
typedef struct MinifiExtension_T* MinifiExtension;

typedef struct MinifiPropertyValidatorCreateInfo {
  MinifiStringView equivalent_nifi_standard_validator_name;
  MinifiBool(*validate)(MinifiStringView);
} MinifiPropertyValidatorCreateInfo;

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

typedef enum MinifiValueType {
  MINIFI_UINT64_TYPE = 0,
  MINIFI_INT64_TYPE = 1,
  MINIFI_UINT32_TYPE = 2,
  MINIFI_INT_TYPE = 3,
  MINIFI_BOOL_TYPE = 4,
  MINIFI_DOUBLE_TYPE = 5,
  MINIFI_STRING_TYPE = 6
} MinifiValueType;

typedef struct MinifiValue {
  MinifiValueType type;
  MinifiStringView value;
} MinifiValue;

typedef struct MinifiSerializedResponseNode {
  MinifiStringView name;
  MinifiValue value;
  MinifiBool array;
  MinifiBool collapsible;
  MinifiBool keep_empty;
  uint32_t children_count;
  const MinifiSerializedResponseNode* children_ptr;
} MinifiSerializedResponseNode;

typedef struct MinifiPublishedMetric {
  MinifiStringView name;
  double value;
  uint32_t labels_count;
  const MinifiStringView* label_keys;
  const MinifiStringView* label_values;
} MinifiPublishedMetric;

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
  void*(*create)(MinifiProcessorMetadata);
  void(*destroy)(void*);
  MinifiBool(*isWorkAvailable)(void*);
  void(*restore)(void*, MinifiFlowFile);
  MinifiBool(*supportsDynamicProperties)(void*);
  MinifiBool(*supportsDynamicRelationships)(void*);
  void(*initialize)(void*, MinifiProcessorDescriptor);
  MinifiBool(*isSingleThreaded)(void*);
  void(*getProcessorType)(void*, MinifiString);
  MinifiBool(*getTriggerWhenEmpty)(void*);
  void(*onTrigger)(void*, MinifiProcessContext, MinifiProcessSession, MinifiString error);
  void(*onSchedule)(void*, MinifiProcessContext, MinifiProcessSessionFactory, MinifiString error);
  void(*onUnSchedule)(void*);
  void(*notifyStop)(void*);
  MinifiInputRequirement(*getInputRequirement)(void*);
  void(*serializeMetrics)(void*, MinifiSerializedResponseNodeVec);
  void(*calculateMetrics)(void*, MinifiPublishedMetricVec);
  void(*forEachLogger)(void*, MinifiLoggerCallback);
} MinifiProcessorCallbacks;

typedef struct MinifiProcessorClassDescription {
  MinifiStringView module_name;
  MinifiStringView short_name;
  MinifiStringView full_name;
  MinifiStringView internal_name;
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
MinifiPropertyValidator MinifiCreatePropertyValidator(const MinifiPropertyValidatorCreateInfo*);
void MinifiSerializedResponseNodeVecPush(MinifiSerializedResponseNodeVec, const MinifiSerializedResponseNode*);
void MinifiPublishedMetricVecPush(MinifiPublishedMetricVec, const MinifiPublishedMetric*);
void MinifiRegisterProcessorClass(const MinifiProcessorClassDescription*);
void MinifiMinifiLoggerCallbackCall(MinifiLoggerCallback, MinifiLogger);
void MinifiProcessorDescriptorSetSupportedRelationships(MinifiProcessorDescriptor, uint32_t relationships_count, const MinifiRelationship* relationships_ptr);
void MinifiProcessorDescriptorSetSupportedProperties(MinifiProcessorDescriptor, uint32_t properties_count, const MinifiProperty* properties_ptr);

MinifiStatus MinifiProcessContextGetProperty(MinifiProcessContext, MinifiStringView, MinifiFlowFile, void(*result_cb)(void* data, MinifiStringView result), void* data);
void MinifiProcessContextYield(MinifiProcessContext);
void MinifiProcessContextGetProcessorName(MinifiProcessContext, void(*result_cb)(void* data, MinifiStringView result), void* data);
MinifiBool MinifiProcessContextHasNonEmptyProperty(MinifiProcessContext, MinifiStringView);

void MinifiStringAssign(MinifiString, MinifiStringView);

void MinifiLoggerSetMaxLogSize(MinifiLogger, int32_t);
void MinifiLoggerGetId(MinifiLogger, void(*cb)(void* data, MinifiStringView error), void* data);
void MinifiLoggerLogString(MinifiLogger, MinifiLogLevel, MinifiStringView);
MinifiBool MinifiLoggerShouldLog(MinifiLogger, MinifiLogLevel);
MinifiLogLevel MinifiLoggerLevel(MinifiLogger);
int32_t MinifiLoggerGetMaxLogSize(MinifiLogger);

OWNED MinifiFlowFile MinifiProcessSessionGet(MinifiProcessSession);
OWNED MinifiFlowFile MinifiProcessSessionCreate(MinifiProcessSession, MinifiFlowFile);
void MinifiDestroyFlowFile(OWNED MinifiFlowFile);
OWNED MinifiFlowFile MinifiCopyFlowFile(MinifiFlowFile);
void MinifiProcessSessionTransfer(MinifiProcessSession, MinifiFlowFile, MinifiStringView);
void MinifiProcessSessionRead(MinifiProcessSession, MinifiFlowFile, int64_t(*cb)(void* data, MinifiInputStream), void* data);
void MinifiProcessSessionWrite(MinifiProcessSession, MinifiFlowFile, int64_t(*cb)(void* data, MinifiOutputStream), void* data);

void MinifiConfigureGet(MinifiConfigure, MinifiStringView, void(*cb)(void*, MinifiStringView), void*);

uint64_t MinifiInputStreamSize(MinifiInputStream);

int64_t MinifiInputStreamRead(MinifiInputStream, char*, uint64_t);
int64_t MinifiOutputStreamWrite(MinifiOutputStream, const char*, uint64_t);

void MinifiStatusToString(MinifiStatus, void(*cb)(void* data, MinifiStringView str), void* data);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // MINIFI_C_H
