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
#ifndef EXTENSIONS_JNIINITIALIZATIONCONTEXT_H
#define EXTENSIONS_JNIINITIALIZATIONCONTEXT_H

#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <jni.h>
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "JniControllerServiceLookup.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

struct JniInitializationContext {
  JniInitializationContext()
      : lookup_(nullptr),
        lookup_ref_(nullptr) {
  }

  std::string identifier_;
  JniControllerServiceLookup *lookup_;
  jobject lookup_ref_;
};

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jstring JNICALL Java_org_apache_nifi_processor_JniInitializationContext_getIdentifier(JNIEnv *env, jobject obj);

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniInitializationContext_getControllerServiceLookup(JNIEnv *env, jobject obj);

// configuration context
JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniConfigurationContext_getPropertyNames(JNIEnv *env, jobject obj);

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniConfigurationContext_getComponent(JNIEnv *env, jobject obj);

JNIEXPORT jstring JNICALL Java_org_apache_nifi_processor_JniConfigurationContext_getName(JNIEnv *env, jobject obj);

JNIEXPORT jstring JNICALL Java_org_apache_nifi_processor_JniConfigurationContext_getPropertyValue(JNIEnv *env, jobject obj, jstring property);


#ifdef __cplusplus
}
#endif


#endif /* EXTENSIONS_JNIINITIALIZATIONCONTEXT_H */
