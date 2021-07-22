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

#include "ExecuteJavaProcessor.h"

#include <regex>
#include <memory>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/FlowFile.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "ResourceClaim.h"
#include "utils/StringUtils.h"
#include "utils/ByteArrayCallback.h"
#include "jvm/JniMethod.h"
#include "jvm/JniLogger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {
namespace processors {

core::Property ExecuteJavaProcessor::JVMControllerService(core::PropertyBuilder::createProperty("JVM Controller Service")
    ->withDescription("Name of controller service defined within this flow")
    ->isRequired(false)->withDefaultValue<std::string>("")->build());

core::Property ExecuteJavaProcessor::NiFiProcessor(core::PropertyBuilder::createProperty("NiFi Processor")
    ->withDescription("Name of NiFi processor to load and run")
    ->isRequired(true)->withDefaultValue<std::string>("")->build());

const char *ExecuteJavaProcessor::ProcessorName = "ExecuteJavaClass";

core::Relationship ExecuteJavaProcessor::Success("success", "All files are routed to success");
void ExecuteJavaProcessor::initialize() {
  logger_->log_info("Initializing ExecuteJavaClass");
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(JVMControllerService);
  properties.insert(NiFiProcessor);
  setSupportedProperties(properties);
  setAcceptAllProperties();
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void ExecuteJavaProcessor::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  std::string controller_service_name;
  if (getProperty(JVMControllerService.getName(), controller_service_name)) {
    auto cs = context->getControllerService(controller_service_name);
    if (cs == nullptr) {
      auto serv_cs = JVMLoader::getInstance()->getBaseServicer();
      java_servicer_ = std::static_pointer_cast<controllers::JavaControllerService>(serv_cs);
      if (serv_cs == nullptr)
        throw std::runtime_error("Could not load controller service");
    } else {
      java_servicer_ = std::static_pointer_cast<controllers::JavaControllerService>(cs);
    }

  } else {
    auto serv_cs = JVMLoader::getInstance()->getBaseServicer();
    java_servicer_ = std::static_pointer_cast<controllers::JavaControllerService>(serv_cs);
    if (serv_cs == nullptr)
      throw std::runtime_error("Could not load controller service");
  }

  if (!getProperty(NiFiProcessor.getName(), class_name_)) {
    throw std::runtime_error("NiFi Processor must be defined");
  }

  nifi_logger_ = logging::LoggerFactory<ExecuteJavaProcessor>::getAliasedLogger(class_name_);

  jni_logger_ref_.logger_reference_ = nifi_logger_;

  jni_logger_class_ = java_servicer_->loadClass("org/apache/nifi/processor/JniLogger");

  spn = java_servicer_->loadClass("org/apache/nifi/processor/JniProcessContext");
  auto env = java_servicer_->attach();
  java_servicer_->putNativeFunctionMapping<minifi::jni::JniProcessContext>(env, spn);

  ClassRegistrar::getRegistrar().registerClasses(env, java_servicer_, "org/apache/nifi/processor/JniInitializationContext", getJniInitializationContextSignatures());

  init = java_servicer_->loadClass("org/apache/nifi/processor/JniInitializationContext");

  ClassRegistrar::getRegistrar().registerClasses(env, java_servicer_, "org/apache/nifi/processor/JniControllerServiceLookup", getJniControllerServiceLookupSignatures());

  if (context_instance_ != nullptr) {
    java_servicer_->attach()->DeleteGlobalRef(context_instance_);
  } else {
    init_context_.identifier_ = getUUIDStr();
    init_context_.lookup_ = &csl_;
    csl_.cs_lookup_reference_ = context;

    init_context_.lookup_ref_ = java_servicer_->newInstance("org.apache.nifi.processor.JniControllerServiceLookup");

    java_servicer_->setReference<minifi::jni::JniControllerServiceLookup>(env, init_context_.lookup_ref_, &csl_);
  }
  context_instance_ = spn.newInstance(env);

  auto initializer = init.newInstance(env);

  java_servicer_->setReference<minifi::jni::JniInitializationContext>(env, initializer, &init_context_);

  ClassRegistrar::getRegistrar().registerClasses(env, java_servicer_, "org/apache/nifi/processor/JniLogger", getLoggerSignatures());

  jni_logger_ref_.clazz_ = jni_logger_class_.getReference();

  logger_instance_ = jni_logger_class_.newInstance(env);
  java_servicer_->setReference<minifi::jni::JniLogger>(env, logger_instance_, &jni_logger_ref_);
  java_servicer_->putNativeFunctionMapping<minifi::jni::JniLogger>(env, jni_logger_class_);
  // create provided class

  clazzInstance = java_servicer_->newInstance(class_name_);
  auto onScheduledNames = java_servicer_->getAnnotations(class_name_, "OnScheduled");
  current_processor_class = java_servicer_->getObjectClass(class_name_, clazzInstance);
  // attempt to schedule here

  ClassRegistrar::getRegistrar().registerClasses(env, java_servicer_, "org/apache/nifi/processor/JniProcessContext", getProcessContextSignatures());

  init.callVoidMethod(env, initializer, "setLogger", "(Lorg/apache/nifi/processor/JniLogger;)V", logger_instance_);

  current_processor_class.callVoidMethod(env, clazzInstance, "initialize", "(Lorg/apache/nifi/processor/ProcessorInitializationContext;)V", initializer);

  jpc.nifi_processor_ = clazzInstance;
  jpc.context_ = context;
  jpc.clazz_ = spn.getReference();
  jpc.processor_ = shared_from_this();
  jpc.cslookup_ = init_context_.lookup_ref_;

  java_servicer_->setReference<minifi::jni::JniProcessContext>(env, context_instance_, &jpc);

  try {
    for (const auto &onScheduledName : onScheduledNames) {
      current_processor_class.callVoidMethod(env, clazzInstance, onScheduledName.first.c_str(), onScheduledName.second, context_instance_);
    }
  } catch (std::runtime_error &re) {
    // this can be ignored.
  }

  java_servicer_->attach()->DeleteGlobalRef(initializer);
}

ExecuteJavaProcessor::~ExecuteJavaProcessor() = default;

JNINativeMethod ExecuteJavaProcessor::registerNativeMethod(const std::string& /*name*/, const std::string& /*params*/, const void* /*ptr*/) {
  JNINativeMethod mthd;

  return mthd;
}

void ExecuteJavaProcessor::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  (void)context;  // unused in release builds
  assert(context == jpc.context_);
  auto env = java_servicer_->attach();

  if (ClassRegistrar::getRegistrar().registerClasses(env, java_servicer_, "org/apache/nifi/processor/JniFlowFile", getFlowFileSignatures())) {
    static auto ffc = java_servicer_->loadClass("org/apache/nifi/processor/JniFlowFile");
    java_servicer_->putNativeFunctionMapping<JniFlowFile>(env, ffc);
  }
  if (ClassRegistrar::getRegistrar().registerClasses(env, java_servicer_, "org/apache/nifi/processor/JniInputStream", getInputStreamSignatures())) {
    static auto jin = java_servicer_->loadClass("org/apache/nifi/processor/JniInputStream");
    java_servicer_->putNativeFunctionMapping<JniInputStream>(env, jin);
  }
  static auto sessioncls = java_servicer_->loadClass("org/apache/nifi/processor/JniProcessSession");

  if (ClassRegistrar::getRegistrar().registerClasses(env, java_servicer_, "org/apache/nifi/processor/JniProcessSession", getProcessSessionSignatures())) {
    java_servicer_->putNativeFunctionMapping<minifi::jni::JniSession>(env, sessioncls);
  }
  ClassRegistrar::getRegistrar().registerClasses(env, java_servicer_, "org/apache/nifi/processor/JniProcessSessionFactory", getProcessSessionFactorySignatures());

  static auto sessionFactoryCls = java_servicer_->loadClass("org/apache/nifi/processor/JniProcessSessionFactory");

  try {
    // it is possible that java classes will maintain a reference to the session factory, so we cannot remove the global references
    // in this function.
    jobject java_process_session_factory = nullptr;

    JniSessionFactory *jniSessionFactory = getFactory(sessionFactory);

    if (!jniSessionFactory) {
      java_process_session_factory = sessionFactoryCls.newInstance(env);
      jniSessionFactory = setFactory(sessionFactory, java_process_session_factory);
      java_servicer_->putNativeFunctionMapping<JniSessionFactory>(env, sessionFactoryCls);
      java_servicer_->setReference<JniSessionFactory>(env, java_process_session_factory, jniSessionFactory);
    } else {
      java_process_session_factory = jniSessionFactory->getJavaReference();
    }
    current_processor_class.callVoidMethod(env, clazzInstance, "onTrigger", "(Lorg/apache/nifi/processor/ProcessContext;Lorg/apache/nifi/processor/ProcessSessionFactory;)V", context_instance_,
                                           java_process_session_factory);
  } catch (const JavaException &je) {
    // clear the java exception so we don't continually wrap it
    env->ExceptionClear();
    logger_->log_error(" Java Exception occurred during onTrigger, reason: %s", je.what());
  } catch (const std::exception &e) {
    // clear the java exception so we don't continually wrap it
    env->ExceptionClear();
    logger_->log_error(" Exception occurred during onTrigger, reason: %s", e.what());
  }
}

void ExecuteJavaProcessor::onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession>& /*session*/) {
  // do nothing.
}

REGISTER_RESOURCE_AS(ExecuteJavaProcessor, "ExecuteJavaClass runs NiFi processors given a provided system path ", ("ExecuteJavaClass"));

} /* namespace processors */
} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

