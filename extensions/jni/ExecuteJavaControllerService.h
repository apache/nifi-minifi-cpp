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
#ifndef __EXECUTE_JAVA_CS_
#define __EXECUTE_JAVA_CS_

#include <memory>
#include <regex>

#include "FlowFileRecord.h"
#include "core/controller/ControllerService.h"
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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {
namespace controllers {

/**
 * Purpose: Enables isolated java loading through the use of controller services
 *
 * While we do allow the nifi properties to define the classes, we also allow run
 * time  loading of java classes
 *
 * In the case where we load via properties, we effectively instantiate this
 * controller service within the execute java process.
 *
 */
class ExecuteJavaControllerService : public core::controller::ControllerService {
 public:

  // Constructor
  /*!
   * Create a new processor
   */
  explicit ExecuteJavaControllerService(std::string name, utils::Identifier uuid = utils::Identifier())
      : core::controller::ControllerService(name, uuid),
        clazzInstance(nullptr),
        logger_(logging::LoggerFactory<ExecuteJavaControllerService>::getLogger()) {
  }

  explicit ExecuteJavaControllerService(const std::string &name, const std::string &id)
      : core::controller::ControllerService(name, id),
        clazzInstance(nullptr),
        logger_(logging::LoggerFactory<ExecuteJavaControllerService>::getLogger()) {
  }
  // Destructor
  virtual ~ExecuteJavaControllerService();
  // Processor Name
  static const char *ProcessorName;
  static core::Property JVMControllerService;
  static core::Property NiFiControllerService;
  // Supported Relationships

  virtual void onEnable() override;
  virtual void initialize() override;
  virtual bool supportsDynamicProperties() override {
    return true;
  }

  virtual void yield() override {
  }

  virtual bool isRunning() override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  virtual bool isWorkAvailable() override {
    return false;
  }

  virtual void notifyStop() override {
    auto env = java_servicer_->attach();
    auto onEnabledName = java_servicer_->getAnnotation(class_name_, "OnDisabled");
    current_cs_class = java_servicer_->getObjectClass(class_name_, clazzInstance);
    // attempt to schedule here

    try {
      current_cs_class.callVoidMethod(env, clazzInstance, onEnabledName.first.c_str(), onEnabledName.second);
    } catch (std::runtime_error &re) {
      // this is avoidable.
    }
  }

 protected:

 private:

  JavaClass current_cs_class;

  jobject clazzInstance;

  std::shared_ptr<controllers::JavaControllerService> java_servicer_;

  std::string class_name_;

  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(ExecuteJavaControllerService, "ExecuteJavaClass runs NiFi Controller services given a provided system path ")

} /* namespace controllers */
} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
