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

#include "JniInitializationContext.h"

#include "JniConfigurationContext.h"
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

jstring Java_org_apache_nifi_processor_JniInitializationContext_getIdentifier(JNIEnv *env, jobject obj) {
  minifi::jni::JniInitializationContext *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniInitializationContext>(env, obj);
  return env->NewStringUTF(ptr->identifier_.c_str());
}

jobject Java_org_apache_nifi_processor_JniInitializationContext_getControllerServiceLookup(JNIEnv *env, jobject obj) {
  minifi::jni::JniInitializationContext *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniInitializationContext>(env, obj);
  if (ptr->lookup_ref_ == nullptr) {
    auto csl = minifi::jni::JVMLoader::getInstance()->load_class("org/apache/nifi/processor/JniControllerServiceLookup", env);
    ptr->lookup_ref_ = csl.newInstance(env);
    minifi::jni::ThrowIf(env);
    minifi::jni::JVMLoader::getInstance()->setReference(ptr->lookup_ref_, env, ptr->lookup_);
  }
  return ptr->lookup_ref_;
}



jstring Java_org_apache_nifi_processor_JniConfigurationContext_getPropertyValue(JNIEnv *env, jobject obj, jstring propertyName) {
  if (obj == nullptr || propertyName == nullptr) {
    return nullptr;
  }
  std::string value;
  minifi::jni::JniConfigurationContext *context = minifi::jni::JVMLoader::getPtr<minifi::jni::JniConfigurationContext>(env, obj);

  if (context == nullptr || context->service_reference_ == nullptr) {
    return nullptr;
  }
  std::string keystr = JniStringToUTF(env, propertyName);
  if (!context->service_reference_->getProperty(keystr, value)) {
    if (!context->service_reference_->getDynamicProperty(keystr, value)) {
      return nullptr;
    }
  }

  return env->NewStringUTF(value.c_str());
}

jobject Java_org_apache_nifi_processor_JniConfigurationContext_getPropertyNames(JNIEnv *env, jobject obj) {
  minifi::jni::JniConfigurationContext *context = minifi::jni::JVMLoader::getPtr<minifi::jni::JniConfigurationContext>(env, obj);
  auto cppProcessor = context->service_reference_;
  auto keys = cppProcessor->getProperties();
  jclass arraylist = env->FindClass("java/util/ArrayList");
  jmethodID init_method = env->GetMethodID(arraylist, "<init>", "(I)V");
  jmethodID add_method = env->GetMethodID(arraylist, "add", "(Ljava/lang/Object;)Z");
  jobject result = env->NewObject(arraylist, init_method, keys.size());
  for (const auto &s : keys) {
    if (s.second.isTransient()) {
      jstring element = env->NewStringUTF(s.first.c_str());
      env->CallBooleanMethod(result, add_method, element);
      minifi::jni::ThrowIf(env);
      env->DeleteLocalRef(element);
    }
  }
  return result;
}

jobject Java_org_apache_nifi_processor_JniConfigurationContext_getComponent(JNIEnv *env, jobject obj) {
  minifi::jni::JniConfigurationContext *context = minifi::jni::JVMLoader::getPtr<minifi::jni::JniConfigurationContext>(env, obj);
  minifi::jni::ThrowIf(env);
  return context->service_reference_->getClassInstance();
}

jstring Java_org_apache_nifi_processor_JniConfigurationContext_getName(JNIEnv *env, jobject obj) {
  minifi::jni::JniConfigurationContext *context = minifi::jni::JVMLoader::getPtr<minifi::jni::JniConfigurationContext>(env, obj);
  minifi::jni::ThrowIf(env);
  return env->NewStringUTF(context->service_reference_->getName().c_str());
}
