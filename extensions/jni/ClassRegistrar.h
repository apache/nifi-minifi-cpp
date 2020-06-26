/**
 * ExecuteJavaClass class declaration
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
#ifndef EXTENSIONS_JNI_CLASSREGISTRAR_H_
#define EXTENSIONS_JNI_CLASSREGISTRAR_H_

#include <memory>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/Resource.h"
#include "concurrentqueue.h"
#include "core/logging/LoggerConfiguration.h"
#include "jvm/JavaControllerService.h"
#include "jvm/JniProcessContext.h"
#include "utils/Id.h"
#include "jvm/NarClassLoader.h"
#include "jvm/JniLogger.h"
#include "jvm/JniReferenceObjects.h"
#include "jvm/JniControllerServiceLookup.h"
#include "jvm/JniInitializationContext.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

class ClassRegistrar {
 public:
  static ClassRegistrar &getRegistrar() {
    static ClassRegistrar registrar;
    // do nothing.
    return registrar;
  }

  bool registerClasses(JNIEnv *env, std::shared_ptr<controllers::JavaControllerService> servicer, const std::string &className, JavaSignatures &signatures) {
    std::lock_guard<std::mutex> lock(mutex_);
    // load class before insertion.
    if (registered_classes_.find(className) == std::end(registered_classes_)) {
      auto cls = servicer->loadClass(className);
      cls.registerMethods(env, signatures);
      registered_classes_.insert(className);
      return true;
    }
    return false;
  }
 private:
  ClassRegistrar() = default;

  std::mutex mutex_;
  std::set<std::string> registered_classes_;
};

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_JNI_CLASSREGISTRAR_H_ */
