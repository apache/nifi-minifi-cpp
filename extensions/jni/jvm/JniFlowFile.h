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

#ifndef EXTENSIONS_JNI_JVM_JNIFLOWFILE_H_
#define EXTENSIONS_JNI_JVM_JNIFLOWFILE_H_

#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <jni.h>
#include "core/Processor.h"
#include "core/ProcessSession.h"

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_org_apache_nifi_processor_JniFlowFile_initialise(JNIEnv *env, jobject obj);

JNIEXPORT jlong JNICALL Java_org_apache_nifi_processor_JniFlowFile_getId(JNIEnv *env, jobject obj);
JNIEXPORT jlong JNICALL Java_org_apache_nifi_processor_JniFlowFile_getEntryDate(JNIEnv *env, jobject obj);
JNIEXPORT jlong JNICALL Java_org_apache_nifi_processor_JniFlowFile_getLineageStartDate(JNIEnv *env, jobject obj);
JNIEXPORT jlong JNICALL Java_org_apache_nifi_processor_JniFlowFile_getLineageStartIndex(JNIEnv *env, jobject obj);
JNIEXPORT jlong JNICALL Java_org_apache_nifi_processor_JniFlowFile_getLastQueueDatePrim(JNIEnv *env, jobject obj);
JNIEXPORT jboolean JNICALL Java_org_apache_nifi_processor_JniFlowFile_isPenalized(JNIEnv *env, jobject obj);
JNIEXPORT jlong JNICALL Java_org_apache_nifi_processor_JniFlowFile_getQueueDateIndex(JNIEnv *env, jobject obj);
JNIEXPORT jstring JNICALL Java_org_apache_nifi_processor_JniFlowFile_getAttribute(JNIEnv *env, jobject obj, jstring key);
JNIEXPORT jstring JNICALL Java_org_apache_nifi_processor_JniFlowFile_getUUIDStr(JNIEnv *env, jobject obj);
JNIEXPORT jlong JNICALL Java_org_apache_nifi_processor_JniFlowFile_getSize(JNIEnv *env, jobject obj);
JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniFlowFile_getAttributes(JNIEnv *env, jobject obj);

#ifdef __cplusplus
}
#endif

#endif /* EXTENSIONS_JNI_JVM_JNIFLOWFILE_H_ */
