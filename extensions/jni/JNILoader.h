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
#ifndef EXTENSIONS_JNILOADER_H
#define EXTENSIONS_JNILOADER_H

#include "core/ClassLoader.h"
#include "ExecuteJavaProcessor.h"
#include "ExecuteJavaControllerService.h"
#include "JVMCreator.h"
#include "jvm/JavaControllerService.h"
#include "utils/StringUtils.h"

class JNIFactory : public core::ObjectFactory {
 public:
  JNIFactory() {

  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() {
    return "JNIFactory";
  }

  virtual std::string getClassName() {
    return "JNIFactory";
  }
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() {
    static std::vector<std::string> class_names;
    if (class_names.empty()) {
      class_names.push_back("ExecuteJavaClass");
      class_names.push_back("JavaControllerService");
      class_names.push_back("JVMCreator");
    }
    return class_names;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) {
    if (utils::StringUtils::equalsIgnoreCase(class_name, "ExecuteJavaClass")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::jni::processors::ExecuteJavaProcessor>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "JavaControllerService")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::jni::controllers::JavaControllerService>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "ExecuteJavaControllerService")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::jni::controllers::ExecuteJavaControllerService>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "JVMCreator")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::jni::JVMCreator>());
    }
    return nullptr;
  }

  static minifi::jni::JVMLoader jvm;

  static bool added;

}
;

extern "C" {
DLL_EXPORT void *createJNIFactory(void);
}
#endif /* EXTENSIONS_JNILOADER_H */
