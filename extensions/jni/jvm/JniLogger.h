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
#ifndef EXTENSIONS_JNILOGGER_H
#define EXTENSIONS_JNILOGGER_H

#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <jni.h>
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

class JniLogger {
 public:
  jclass getClass() {
    return clazz_;
  }
  jclass clazz_;
  std::shared_ptr<logging::Logger> logger_reference_;
};

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jboolean JNICALL Java_org_apache_nifi_processor_JniLogger_isWarnEnabled(JNIEnv *env, jobject obj);
JNIEXPORT jboolean JNICALL Java_org_apache_nifi_processor_JniLogger_isTraceEnabled(JNIEnv *env, jobject obj);
JNIEXPORT jboolean JNICALL Java_org_apache_nifi_processor_JniLogger_isInfoEnabled(JNIEnv *env, jobject obj);
JNIEXPORT jboolean JNICALL Java_org_apache_nifi_processor_JniLogger_isErrorEnabled(JNIEnv *env, jobject obj);
JNIEXPORT jboolean JNICALL Java_org_apache_nifi_processor_JniLogger_isDebugEnabled(JNIEnv *env, jobject obj);

JNIEXPORT void JNICALL Java_org_apache_nifi_processor_JniLogger_warn(JNIEnv *env, jobject obj, jstring msg);
JNIEXPORT void JNICALL Java_org_apache_nifi_processor_JniLogger_error(JNIEnv *env, jobject obj, jstring msg);
JNIEXPORT void JNICALL Java_org_apache_nifi_processor_JniLogger_info(JNIEnv *env, jobject obj, jstring msg);
JNIEXPORT void JNICALL Java_org_apache_nifi_processor_JniLogger_debug(JNIEnv *env, jobject obj, jstring msg);
JNIEXPORT void JNICALL Java_org_apache_nifi_processor_JniLogger_trace(JNIEnv *env, jobject obj, jstring msg);

#ifdef __cplusplus
}
#endif

#endif /* EXTENSIONS_JNILOGGER_H */
