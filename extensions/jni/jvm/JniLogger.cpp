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

#include "JniLogger.h"

#include <string>
#include <memory>
#include <algorithm>
#include <iterator>
#include <set>
#include "core/Property.h"
#include "io/validation.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "properties/Configure.h"
#include "JVMLoader.h"

#include "core/Processor.h"
#include "JniFlowFile.h"
#include "../JavaException.h"

#ifdef __cplusplus
extern "C" {
#endif

jboolean Java_org_apache_nifi_processor_JniLogger_isWarnEnabled(JNIEnv *env, jobject obj) {
  minifi::jni::JniLogger *logger_ref = minifi::jni::JVMLoader::getPtr<minifi::jni::JniLogger>(env, obj);
  return true;
}
jboolean Java_org_apache_nifi_processor_JniLogger_isTraceEnabled(JNIEnv *env, jobject obj) {
  minifi::jni::JniLogger *logger_ref = minifi::jni::JVMLoader::getPtr<minifi::jni::JniLogger>(env, obj);
  return true;
}
jboolean Java_org_apache_nifi_processor_JniLogger_isInfoEnabled(JNIEnv *env, jobject obj) {
  minifi::jni::JniLogger *logger_ref = minifi::jni::JVMLoader::getPtr<minifi::jni::JniLogger>(env, obj);
  return true;
}
jboolean Java_org_apache_nifi_processor_JniLogger_isErrorEnabled(JNIEnv *env, jobject obj) {
  minifi::jni::JniLogger *logger_ref = minifi::jni::JVMLoader::getPtr<minifi::jni::JniLogger>(env, obj);
  return true;
}
jboolean Java_org_apache_nifi_processor_JniLogger_isDebugEnabled(JNIEnv *env, jobject obj) {
  minifi::jni::JniLogger *logger_ref = minifi::jni::JVMLoader::getPtr<minifi::jni::JniLogger>(env, obj);
  return true;
}

void Java_org_apache_nifi_processor_JniLogger_warn(JNIEnv *env, jobject obj, jstring msg) {
  minifi::jni::JniLogger *logger_ref = minifi::jni::JVMLoader::getPtr<minifi::jni::JniLogger>(env, obj);
  if (!logger_ref)
    return;
  const char *msgStr = env->GetStringUTFChars(msg, 0);
  logger_ref->logger_reference_->log_warn(msgStr);
  env->ReleaseStringUTFChars(msg, msgStr);
}

void Java_org_apache_nifi_processor_JniLogger_error(JNIEnv *env, jobject obj, jstring msg) {
  minifi::jni::JniLogger *logger_ref = minifi::jni::JVMLoader::getPtr<minifi::jni::JniLogger>(env, obj);
  if (!logger_ref)
    return;
  const char *msgStr = env->GetStringUTFChars(msg, 0);
  logger_ref->logger_reference_->log_error(msgStr);
  env->ReleaseStringUTFChars(msg, msgStr);
}
void Java_org_apache_nifi_processor_JniLogger_info(JNIEnv *env, jobject obj, jstring msg) {
  minifi::jni::JniLogger *logger_ref = minifi::jni::JVMLoader::getPtr<minifi::jni::JniLogger>(env, obj);
  if (!logger_ref)
    return;
  const char *msgStr = env->GetStringUTFChars(msg, 0);
  logger_ref->logger_reference_->log_info(msgStr);
  env->ReleaseStringUTFChars(msg, msgStr);
}
void Java_org_apache_nifi_processor_JniLogger_debug(JNIEnv *env, jobject obj, jstring msg) {
  minifi::jni::JniLogger *logger_ref = minifi::jni::JVMLoader::getPtr<minifi::jni::JniLogger>(env, obj);
  if (!logger_ref)
    return;
  const char *msgStr = env->GetStringUTFChars(msg, 0);
  logger_ref->logger_reference_->log_debug(msgStr);
  env->ReleaseStringUTFChars(msg, msgStr);
}
void Java_org_apache_nifi_processor_JniLogger_trace(JNIEnv *env, jobject obj, jstring msg) {
  minifi::jni::JniLogger *logger_ref = minifi::jni::JVMLoader::getPtr<minifi::jni::JniLogger>(env, obj);
  if (!logger_ref)
    return;
  const char *msgStr = env->GetStringUTFChars(msg, 0);
  logger_ref->logger_reference_->log_trace(msgStr);
  env->ReleaseStringUTFChars(msg, msgStr);
}

#ifdef __cplusplus
}
#endif
