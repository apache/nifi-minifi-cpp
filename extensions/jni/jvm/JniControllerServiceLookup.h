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

#ifndef EXTENSIONS_JNI_JVM_JNICONTROLLERSERVICELOOKUP_H_
#define EXTENSIONS_JNI_JVM_JNICONTROLLERSERVICELOOKUP_H_

#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <jni.h>
#include "core/Processor.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

class JniControllerService {
 public:

  std::shared_ptr<core::controller::ControllerService> cs_reference_;
};

class JniControllerServiceLookup {
 public:
  std::shared_ptr<core::controller::ControllerServiceLookup> cs_lookup_reference_;
};

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */


#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniControllerServiceLookup_getControllerService(JNIEnv *env, jobject obj, jstring cs);

JNIEXPORT jboolean JNICALL Java_org_apache_nifi_processor_JniControllerServiceLookup_isControllerServiceEnabled(JNIEnv *env, jobject obj, jstring cs);

JNIEXPORT jboolean JNICALL Java_org_apache_nifi_processor_JniControllerServiceLookup_isControllerServiceEnabling(JNIEnv *env, jobject obj, jstring cs);

JNIEXPORT jstring JNICALL Java_org_apache_nifi_processor_JniControllerServiceLookup_getControllerServiceName(JNIEnv *env, jobject obj, jstring cs);

#ifdef __cplusplus
}
#endif

#endif /* EXTENSIONS_JNI_JVM_JNICONTROLLERSERVICELOOKUP_H_ */
