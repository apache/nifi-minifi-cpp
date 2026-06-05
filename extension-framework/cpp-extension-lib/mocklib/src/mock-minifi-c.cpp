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

#include "minifi-c.h"

extern "C" {
MinifiExtension* MINIFI_REGISTER_EXTENSION_FN(MinifiExtensionContext*, const MinifiExtensionDefinition*) {
  throw std::runtime_error("Not implemented");
}

MinifiStatus MinifiRegisterProcessor(MinifiExtension*, const MinifiProcessorClassDefinition*) {
  throw std::runtime_error("Not implemented");
}

MinifiStatus MinifiRegisterControllerService(MinifiExtension*, const MinifiControllerServiceClassDefinition*) {
  throw std::runtime_error("Not implemented");
}

MINIFI_OWNED MinifiPublishedMetrics* MinifiPublishedMetricsCreate(size_t, const MinifiStringView*, const double*) {
  throw std::runtime_error("Not implemented");
}

MinifiStatus MinifiProcessContextGetProperty(MinifiProcessContext*, MinifiStringView, MinifiFlowFile*,
    void (*)(void* user_ctx, MinifiStringView property_value), void*) {
  throw std::runtime_error("Not implemented");
}
MinifiBool MinifiProcessContextHasNonEmptyProperty(MinifiProcessContext*, MinifiStringView) {
  throw std::runtime_error("Not implemented");
}

MinifiStatus MinifiProcessContextGetControllerService(MinifiProcessContext*, MinifiStringView, MinifiStringView, MinifiControllerService**) {
  throw std::runtime_error("Not implemented");
}
void MinifiProcessContextGetDynamicProperties(MinifiProcessContext*, MinifiFlowFile*,
    void (*)(void* user_ctx, MinifiStringView dynamic_property_name, MinifiStringView dynamic_property_value), void*) {
  throw std::runtime_error("Not implemented");
}

MinifiStatus MinifiProcessContextGetSslDataFromProperty(MinifiProcessContext*, MinifiStringView,
    void (*)(void* user_ctx, const MinifiSslData* ssl_data), void*) {
  throw std::runtime_error("Not implemented");
}

void MinifiLoggerSetMaxLogSize(MinifiLogger*, int32_t) {
  throw std::runtime_error("Not implemented");
}
void MinifiLoggerLogString(MinifiLogger*, MinifiLogLevel, MinifiStringView) {
  throw std::runtime_error("Not implemented");
}
MinifiBool MinifiLoggerShouldLog(MinifiLogger*, MinifiLogLevel) {
  throw std::runtime_error("Not implemented");
}
MinifiLogLevel MinifiLoggerLevel(MinifiLogger*) {
  throw std::runtime_error("Not implemented");
}

MINIFI_OWNED MinifiFlowFile* MinifiProcessSessionGet(MinifiProcessSession*) {
  throw std::runtime_error("Not implemented");
}
MINIFI_OWNED MinifiFlowFile* MinifiProcessSessionCreate(MinifiProcessSession*, MinifiFlowFile*) {
  throw std::runtime_error("Not implemented");
}

MinifiStatus MinifiProcessSessionPenalize(MinifiProcessSession*, MinifiFlowFile*) {
  throw std::runtime_error("Not implemented");
}

MinifiStatus MinifiProcessSessionTransfer(MinifiProcessSession*, MINIFI_OWNED MinifiFlowFile*, MinifiStringView) {
  throw std::runtime_error("Not implemented");
}

MinifiStatus MinifiProcessSessionRemove(MinifiProcessSession*, MINIFI_OWNED MinifiFlowFile*) {
  throw std::runtime_error("Not implemented");
}

MinifiStatus MinifiProcessSessionRead(MinifiProcessSession*, MinifiFlowFile*, int64_t (*)(void* user_ctx, MinifiInputStream*), void*) {
  throw std::runtime_error("Not implemented");
}
MinifiStatus MinifiProcessSessionWrite(MinifiProcessSession*, MinifiFlowFile*, int64_t (*)(void* user_ctx, MinifiOutputStream*), void*) {
  throw std::runtime_error("Not implemented");
}

void MinifiConfigGet(MinifiExtensionContext*, MinifiStringView, void (*)(void* user_ctx, MinifiStringView config_value), void*) {
  throw std::runtime_error("Not implemented");
}

size_t MinifiInputStreamSize(MinifiInputStream*) {
  throw std::runtime_error("Not implemented");
}

int64_t MinifiInputStreamRead(MinifiInputStream*, char*, size_t) {
  throw std::runtime_error("Not implemented");
}
int64_t MinifiOutputStreamWrite(MinifiOutputStream*, const char*, size_t) {
  throw std::runtime_error("Not implemented");
}

MinifiStatus MinifiProcessSessionSetFlowFileAttribute(MinifiProcessSession*, MinifiFlowFile*, MinifiStringView, const MinifiStringView*) {
  throw std::runtime_error("Not implemented");
}
MinifiBool MinifiProcessSessionGetFlowFileAttribute(MinifiProcessSession*, MinifiFlowFile*, MinifiStringView,
    void (*)(void* user_ctx, MinifiStringView attribute_value), void*) {
  throw std::runtime_error("Not implemented");
}
void MinifiProcessSessionGetFlowFileAttributes(MinifiProcessSession*, MinifiFlowFile*,
    void (*)(void* user_ctx, MinifiStringView attribute_name, MinifiStringView attribute_value), void*) {
  throw std::runtime_error("Not implemented");
}
uint64_t MinifiProcessSessionGetFlowFileSize(MinifiProcessSession*, MinifiFlowFile*) {
  throw std::runtime_error("Not implemented");
}
MinifiStatus MinifiProcessSessionGetFlowFileId(MinifiProcessSession*, MinifiFlowFile*, void (*)(void* user_ctx, MinifiStringView flow_file_id),
    void*) {
  throw std::runtime_error("Not implemented");
}

MinifiStatus MinifiControllerServiceContextGetProperty(MinifiControllerServiceContext*, MinifiStringView,
    void (*)(void* user_ctx, MinifiStringView property_value), void*) {
  throw std::runtime_error("Not implemented");
}
}  // extern "C"
