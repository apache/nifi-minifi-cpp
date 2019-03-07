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

#include "JniControllerServiceLookup.h"

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
#include "../JNIUtil.h"
#include "JniReferenceObjects.h"
#include "JavaDefs.h"
#include "core/controller/ControllerService.h"
#include "core/controller/ControllerServiceLookup.h"
#include "../ExecuteJavaControllerService.h"

#ifdef __cplusplus
extern "C" {
#endif

jobject Java_org_apache_nifi_processor_JniControllerServiceLookup_getControllerService(JNIEnv *env, jobject obj, jstring cs) {
  minifi::jni::JniControllerServiceLookup *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniControllerServiceLookup>(env, obj);
  auto str = JniStringToUTF(env, cs);
  auto controller_service = ptr->cs_lookup_reference_->getControllerService(str);
  if (nullptr != controller_service) {
    auto ecs = std::dynamic_pointer_cast<minifi::jni::controllers::ExecuteJavaControllerService>(controller_service);
    if (nullptr != ecs) {
      return ecs->getClassInstance();
    }
  }
  return nullptr;
}

jboolean Java_org_apache_nifi_processor_JniControllerServiceLookup_isControllerServiceEnabled(JNIEnv *env, jobject obj, jstring cs) {
  minifi::jni::JniControllerServiceLookup *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniControllerServiceLookup>(env, obj);
  auto str = JniStringToUTF(env, cs);
  return ptr->cs_lookup_reference_->isControllerServiceEnabled(str);
}

jboolean Java_org_apache_nifi_processor_JniControllerServiceLookup_isControllerServiceEnabling(JNIEnv *env, jobject obj, jstring cs) {
  minifi::jni::JniControllerServiceLookup *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniControllerServiceLookup>(env, obj);
  auto str = JniStringToUTF(env, cs);
  return ptr->cs_lookup_reference_->isControllerServiceEnabling(str);
}

jstring Java_org_apache_nifi_processor_JniControllerServiceLookup_getControllerServiceName(JNIEnv *env, jobject obj, jstring cs) {
  minifi::jni::JniControllerServiceLookup *ptr = minifi::jni::JVMLoader::getInstance()->getReference<minifi::jni::JniControllerServiceLookup>(env, obj);
  auto str = JniStringToUTF(env, cs);
  return env->NewStringUTF(ptr->cs_lookup_reference_->getControllerServiceName(str).c_str());
}

#ifdef __cplusplus
}
#endif
