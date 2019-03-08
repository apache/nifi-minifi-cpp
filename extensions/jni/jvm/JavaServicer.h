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

#ifndef EXTENSIONS_JNI_JVM_JAVASERVICER_H_
#define EXTENSIONS_JNI_JVM_JAVASERVICER_H_

#include <jni.h>
#include "JavaClass.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

/**
 * Purpose: Java servicer provides a shared construct to be used by classes
 * within the JNI extension to attach the JNI environment, get the class loader
 * and load a class.
 */
class JavaServicer {
 public:
  virtual ~JavaServicer() {

  }
  virtual JNIEnv *attach() = 0;
  virtual void detach()  = 0;
  virtual jobject getClassLoader() = 0;
  virtual JavaClass loadClass(const std::string &class_name_) = 0;
};

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_JNI_JVM_JAVASERVICER_H_ */
