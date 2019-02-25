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
#ifndef EXTENSIONS_JNIPROCESSCONTEXT_H
#define EXTENSIONS_JNIPROCESSCONTEXT_H

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

class JniProcessContext {
 public:
  JniProcessContext()
      : nifi_processor_(nullptr),
        processor_(nullptr),
        context_(nullptr) {
  }

  jclass getClass() {
    return clazz_;
  }
  jclass clazz_;
  jobject nifi_processor_;
  std::shared_ptr<core::Processor> processor_;
  std::shared_ptr<core::ProcessContext> context_;
};

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jstring JNICALL Java_org_apache_nifi_processor_JniProcessContext_getPropertyValue(JNIEnv *env, jobject obj, jstring propertyName);

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniProcessContext_getPropertyNames(JNIEnv *env, jobject obj);

JNIEXPORT jobject JNICALL Java_org_apache_nifi_processor_JniProcessContext_getProcessor(JNIEnv *env, jobject obj);

#ifdef __cplusplus
}
#endif

/*
 class JniProcessContext {
 public:
 JniProcessContext(const std::shared_ptr<core::ProcessContext> &ctx)
 : ctx_(ctx) {

 }
 private:
 std::shared_ptr<core::ProcessContext> ctx_;
 };*/

#endif /* EXTENSIONS_JNIPROCESSCONTEXT_H */
