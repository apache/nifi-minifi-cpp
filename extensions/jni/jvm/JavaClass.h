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
#ifndef EXTENSIONS_JAVACLASS_H
#define EXTENSIONS_JAVACLASS_H

#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <jni.h>
#include "JniProcessContext.h"
#include "JniFlowFile.h"
#include "JniProcessSession.h"
#include "JniMethod.h"
#include "../JavaException.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

/**
 * Purpose and Justification: Represents a java class that can be used
 * for caching access to Java objects and classes.
 *
 */
class JavaClass {

 public:

  JavaClass()
      : class_ref_(nullptr) {

  }

  /**
   * Initializes the java class with the name ( package + class name )
   * @param name class name
   * @param classref class reference
   * @param jenv Java environment -- should be thread associated
   */
  explicit JavaClass(const std::string &name, jclass classref, JNIEnv* jenv)
      : name_(name) {
    class_ref_ = classref;
    cnstrctr = jenv->GetMethodID(class_ref_, "<init>", "()V");
  }

  ~JavaClass() {
  }

  std::string getName() const {
    return name_;
  }

  jclass getReference() const {
    return class_ref_;
  }

  /**
   * Though this is implicitly created, we'd like to make this aspect
   * clear that we're likely to copy JavaClasses around.
   */
  JavaClass &operator=(const JavaClass &o) = default;

  /**
   * Call empty constructor
   * @param env if supplied
   * @return jobject that is a global reference.
   */
  JNIEXPORT
  jobject newInstance(JNIEnv *env) {
    std::string instanceName = "(L" + name_ + ";)V";
    JNIEnv *lenv = env;

    ThrowIf(lenv);

    auto rezobj = lenv->NewObject(class_ref_, cnstrctr);

    ThrowIf(lenv);

    return lenv->NewGlobalRef(rezobj);
  }

  jmethodID getClassMethod(JNIEnv *env, const std::string &methodName, const std::string &type) {
    jmethodID mid = env->GetMethodID(class_ref_, methodName.c_str(), type.c_str());
    return mid;
  }

  void registerMethods(JNIEnv *env, JNINativeMethod *methods, size_t size) {
    env->RegisterNatives(class_ref_, methods, size);
    ThrowIf(env);

  }

  void registerMethods(JNIEnv *env, JavaSignatures &signatures) {
    auto methods = signatures.getSignatures();
    env->RegisterNatives(class_ref_, methods, signatures.getSize());
    ThrowIf(env);

  }

  template<typename ... Args>
  void callVoidMethod(JNIEnv* env, jobject obj, const std::string &methodName, const std::string &type, Args ... args) {
    jmethodID method = getClassMethod(env, methodName, type);
    ThrowIf(env);
    env->CallVoidMethod(obj, method, std::forward<Args>(args)...);
    ThrowIf(env);
  }

 private:
  jmethodID cnstrctr;
  std::string name_;
  jclass class_ref_;
};

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_JAVACLASS_H */
