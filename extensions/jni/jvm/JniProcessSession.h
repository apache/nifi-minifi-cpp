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
#ifndef EXTENSIONS_JNIPROCESSSESSION_H
#define EXTENSIONS_JNIPROCESSSESSION_H

#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <jni.h>
#include "io/BaseStream.h"
#include "FlowFileRecord.h"
#include "core/ProcessSession.h"
#include "core/ProcessSessionFactory.h"
#include "core/WeakReference.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniProcessSession_readFlowFile(JNIEnv *env, jobject obj, jobject ff);

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniProcessSession_get(JNIEnv *env, jobject obj);

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniProcessSession_create(JNIEnv *env, jobject obj);

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniProcessSession_clone(JNIEnv *env, jobject obj, jobject ff);

JNIEXPORT jboolean JNICALL Java_org_apache_nifi_processor_JniProcessSession_write(JNIEnv *env, jobject obj, jobject ff, jbyteArray byteArray);

JNIEXPORT void JNICALL Java_org_apache_nifi_processor_JniProcessSession_transfer(JNIEnv *env, jobject obj, jobject ff, jstring relationship);

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniProcessSession_putAttribute(JNIEnv *env, jobject obj, jobject ff, jstring key, jstring value);

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniProcessSession_createWithParent(JNIEnv *env, jobject obj, jobject parent);

JNIEXPORT void JNICALL Java_org_apache_nifi_processor_JniProcessSession_commit(JNIEnv *env, jobject obj);

JNIEXPORT void JNICALL Java_org_apache_nifi_processor_JniProcessSession_rollback(JNIEnv *env, jobject obj);

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniProcessSessionFactory_createSession(JNIEnv *env, jobject obj);

JNIEXPORT void JNICALL Java_org_apache_nifi_processor_JniProcessSession_remove(JNIEnv *env, jobject obj, jobject ff);

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniProcessSession_penalize(JNIEnv *env, jobject obj, jobject ff);

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniProcessSession_removeAttribute(JNIEnv *env, jobject obj, jobject ff, jstring attr);

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniProcessSession_clonePortion(JNIEnv *env, jobject obj, jobject ff, jlong offset, jlong size);

JNIEXPORT jboolean JNICALL Java_org_apache_nifi_processor_JniProcessSession_append(JNIEnv *env, jobject obj, jobject ff, jbyteArray byteArray);

JNIEXPORT jint JNICALL Java_org_apache_nifi_processor_JniInputStream_read(JNIEnv *env, jobject obj);

JNIEXPORT jint JNICALL Java_org_apache_nifi_processor_JniInputStream_readWithOffset(JNIEnv *env, jobject obj, jbyteArray, jint offset, jint length);

#ifdef __cplusplus
}
#endif

#endif /* EXTENSIONS_JNIPROCESSSESSION_H */
