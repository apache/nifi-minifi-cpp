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

#include "JniFlowFile.h"

#include <string>
#include <memory>
#include <algorithm>
#include <iterator>
#include <set>
#include "core/Property.h"
#include "io/validation.h"
#include "core/FlowFile.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "properties/Configure.h"
#include "JVMLoader.h"
#include "../JavaException.h"
#include "JniReferenceObjects.h"
#include "JavaDefs.h"

#ifdef __cplusplus
extern "C" {
#endif

jlong Java_org_apache_nifi_processor_JniFlowFile_getId(JNIEnv *env, jobject obj) {

  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, obj);

  auto ff = ptr->get();
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  jlong id = ff->getId();
  return id;

}
jlong Java_org_apache_nifi_processor_JniFlowFile_getEntryDate(JNIEnv *env, jobject obj) {
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, obj);

  auto ff = ptr->get();
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  jlong entryDate = ff->getEntryDate();
  return entryDate;
}
jlong Java_org_apache_nifi_processor_JniFlowFile_getLineageStartDate(JNIEnv *env, jobject obj) {
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, obj);

  auto ff = ptr->get();
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  jlong val = ff->getlineageStartDate();
  return val;
}
jlong Java_org_apache_nifi_processor_JniFlowFile_getLineageStartIndex(JNIEnv *env, jobject obj) {
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, obj);

  auto ff = ptr->get();
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  jlong val = ff->getlineageStartDate();
  return val;
}
jlong Java_org_apache_nifi_processor_JniFlowFile_getLastQueueDatePrim(JNIEnv *env, jobject obj) {
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, obj);

  auto ff = ptr->get();
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  jlong val = 0;
  return val;
}
jlong Java_org_apache_nifi_processor_JniFlowFile_getQueueDateIndex(JNIEnv *env, jobject obj) {
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, obj);

  auto ff = ptr->get();
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  jlong val = 0;
  return val;
}
jboolean Java_org_apache_nifi_processor_JniFlowFile_isPenalized(JNIEnv *env, jobject obj) {
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, obj);

  auto ff = ptr->get();
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  jboolean val = ff->isPenalized();
  return val;
}
jstring Java_org_apache_nifi_processor_JniFlowFile_getAttribute(JNIEnv *env, jobject obj, jstring key) {
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, obj);

  auto ff = ptr->get();
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  std::string value;
  ff->getAttribute(JniStringToUTF(env, key), value);
  return env->NewStringUTF(value.c_str());
}
jlong Java_org_apache_nifi_processor_JniFlowFile_getSize(JNIEnv *env, jobject obj) {
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, obj);
  auto ff = ptr->get();
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  jlong val = ff->getSize();
  return val;
}
jstring Java_org_apache_nifi_processor_JniFlowFile_getUUIDStr(JNIEnv *env, jobject obj) {
  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, obj);

  auto ff = ptr->get();
  THROW_IF_NULL(ff, env, NO_FF_OBJECT);
  return env->NewStringUTF(ff->getUUIDStr().c_str());
}

jobject Java_org_apache_nifi_processor_JniFlowFile_getAttributes(JNIEnv *env, jobject obj) {

  minifi::jni::JniFlowFile *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniFlowFile>(env, obj);

  auto ff = ptr->get();
  jclass mapClass = env->FindClass("java/util/HashMap");
  if (mapClass == nullptr) {
    return nullptr;
  }

  jsize map_len = ff->getAttributes().size();

  jmethodID init = env->GetMethodID(mapClass, "<init>", "(I)V");
  jobject hashMap = env->NewObject(mapClass, init, map_len);

  jmethodID put = env->GetMethodID(mapClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

  for (auto kf : ff->getAttributes()) {
    env->CallObjectMethod(hashMap, put, env->NewStringUTF(kf.first.c_str()), env->NewStringUTF(kf.second.c_str()));
    minifi::jni::ThrowIf(env);
  }

  return hashMap;
}

#ifdef __cplusplus
}
#endif
