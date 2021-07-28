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
#pragma once

#include <memory>
#include <vector>
#include <string>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
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
#include "ClassRegistrar.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {
namespace processors {

/**
 * Purpose and Justification: Executes a java NiFi Processor
 *
 * Design: Extends Processor to provide basic processor support capabilities.
 */
class ExecuteJavaProcessor : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit ExecuteJavaProcessor(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid),
        logger_instance_(nullptr),
        context_instance_(nullptr),
        logger_(logging::LoggerFactory<ExecuteJavaProcessor>::getLogger()),
        nifi_logger_(nullptr) {
  }
  // Destructor
  virtual ~ExecuteJavaProcessor();
  // Processor Name
  static const char *ProcessorName;
  static core::Property JVMControllerService;
  static core::Property NiFiProcessor;
  // Supported Relationships
  static core::Relationship Success;

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  bool supportsDynamicProperties() override {
    return true;
  }

 protected:
  static JavaSignatures &getLoggerSignatures() {
    static JavaSignatures loggersignatures;
    if (loggersignatures.empty()) {
      loggersignatures.addSignature({ "isWarnEnabled", "()Z", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniLogger_isWarnEnabled) });
      loggersignatures.addSignature({ "isTraceEnabled", "()Z", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniLogger_isTraceEnabled) });
      loggersignatures.addSignature({ "isInfoEnabled", "()Z", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniLogger_isInfoEnabled) });
      loggersignatures.addSignature({ "isErrorEnabled", "()Z", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniLogger_isErrorEnabled) });
      loggersignatures.addSignature({ "isDebugEnabled", "()Z", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniLogger_isDebugEnabled) });

      loggersignatures.addSignature({ "info", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniLogger_info) });
      loggersignatures.addSignature({ "warn", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniLogger_warn) });
      loggersignatures.addSignature({ "error", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniLogger_error) });
      loggersignatures.addSignature({ "debug", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniLogger_debug) });
      loggersignatures.addSignature({ "trace", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniLogger_trace) });
    }
    return loggersignatures;
  }

  static JavaSignatures &getProcessContextSignatures() {
    static JavaSignatures methodSignatures;
    if (methodSignatures.empty()) {
      methodSignatures.addSignature({ "getComponent",
                                      "()Lorg/apache/nifi/components/AbstractConfigurableComponent;",
                                      reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessContext_getComponent) });
      methodSignatures.addSignature({ "getPropertyNames",
                                      "()Ljava/util/List;",
                                      reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessContext_getPropertyNames) });
      methodSignatures.addSignature({ "getPropertyValue",
                                      "(Ljava/lang/String;)Ljava/lang/String;",
                                      reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessContext_getPropertyValue) });
      methodSignatures.addSignature({ "getName",
                                      "()Ljava/lang/String;",
                                      reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessContext_getName) });
      methodSignatures.addSignature({ "getControllerServiceLookup",
                                      "()Lorg/apache/nifi/controller/ControllerServiceLookup;",
                                      reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessContext_getControllerServiceLookup) });
    }
    return methodSignatures;
  }

  static JavaSignatures &getInputStreamSignatures() {
    static JavaSignatures methodSignatures;
    if (methodSignatures.empty()) {
      methodSignatures.addSignature({ "read", "()I", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniInputStream_read) });
      methodSignatures.addSignature({ "readWithOffset", "([BII)I", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniInputStream_readWithOffset) });
    }
    return methodSignatures;
  }

  static JavaSignatures &getFlowFileSignatures() {
    static JavaSignatures methodSignatures;
    if (methodSignatures.empty()) {
      methodSignatures.addSignature({ "getAttributes", "()Ljava/util/Map;", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniFlowFile_getAttributes) });
      methodSignatures.addSignature({ "getAttribute", "(Ljava/lang/String;)Ljava/lang/String;", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniFlowFile_getAttribute) });
      methodSignatures.addSignature({ "getSize", "()J", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniFlowFile_getSize) });
      methodSignatures.addSignature({ "getEntryDate", "()J", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniFlowFile_getEntryDate) });
      methodSignatures.addSignature({ "getLineageStartDate", "()J", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniFlowFile_getLineageStartDate) });
      methodSignatures.addSignature({ "getLastQueueDatePrim", "()J", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniFlowFile_getLastQueueDatePrim) });
      methodSignatures.addSignature({ "getQueueDateIndex", "()J", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniFlowFile_getQueueDateIndex) });
      methodSignatures.addSignature({ "getId", "()J", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniFlowFile_getId) });
      methodSignatures.addSignature({ "getUUIDStr", "()Ljava/lang/String;", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniFlowFile_getUUIDStr) });
    }
    return methodSignatures;
  }

  static JavaSignatures &getProcessSessionSignatures() {
    static JavaSignatures methodSignatures;
    if (methodSignatures.empty()) {
      methodSignatures.addSignature({ "remove", "(Lorg/apache/nifi/flowfile/FlowFile;)V", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_remove) });
      methodSignatures.addSignature({ "create", "()Lorg/apache/nifi/flowfile/FlowFile;", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_create) });
      methodSignatures.addSignature({ "penalize", "(Lorg/apache/nifi/flowfile/FlowFile;)Lorg/apache/nifi/flowfile/FlowFile;",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_penalize) });
      methodSignatures.addSignature({ "createWithParent", "(Lorg/apache/nifi/flowfile/FlowFile;)Lorg/apache/nifi/flowfile/FlowFile;",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_createWithParent) });
      methodSignatures.addSignature({ "rollback", "()V", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_rollback) });
      methodSignatures.addSignature({ "commit", "()V", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_commit) });
      methodSignatures.addSignature({ "get", "()Lorg/apache/nifi/flowfile/FlowFile;", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_get) });
      methodSignatures.addSignature({ "write", "(Lorg/apache/nifi/flowfile/FlowFile;[B)Z", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_write) });
      methodSignatures.addSignature({ "append", "(Lorg/apache/nifi/flowfile/FlowFile;[B)Z", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_append) });
      methodSignatures.addSignature({ "putAttribute", "(Lorg/apache/nifi/flowfile/FlowFile;Ljava/lang/String;Ljava/lang/String;)Lorg/apache/nifi/flowfile/FlowFile;",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_putAttribute) });
      methodSignatures.addSignature({ "removeAttribute", "(Lorg/apache/nifi/flowfile/FlowFile;Ljava/lang/String;)Lorg/apache/nifi/flowfile/FlowFile;",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_removeAttribute) });
      methodSignatures.addSignature({ "clone", "(Lorg/apache/nifi/flowfile/FlowFile;)Lorg/apache/nifi/flowfile/FlowFile;",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_clone) });
      methodSignatures.addSignature({ "clonePortion", "(Lorg/apache/nifi/flowfile/FlowFile;JJ)Lorg/apache/nifi/flowfile/FlowFile;",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_clonePortion) });
      methodSignatures.addSignature({ "readFlowFile", "(Lorg/apache/nifi/flowfile/FlowFile;)Lorg/apache/nifi/processor/JniInputStream;",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_readFlowFile) });
      methodSignatures.addSignature({ "transfer", "(Lorg/apache/nifi/flowfile/FlowFile;Ljava/lang/String;)V", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSession_transfer) });
    }
    return methodSignatures;
  }

  static JavaSignatures &getJniInitializationContextSignatures() {
    static JavaSignatures methodSignatures;
    if (methodSignatures.empty()) {
      methodSignatures.addSignature({ "getControllerServiceLookup", "()Lorg/apache/nifi/controller/ControllerServiceLookup;",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniInitializationContext_getControllerServiceLookup) });
      methodSignatures.addSignature({ "getIdentifier", "()Ljava/lang/String;", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniInitializationContext_getIdentifier) });
    }
    return methodSignatures;
  }

  static JavaSignatures &getJniControllerServiceLookupSignatures() {
    static JavaSignatures methodSignatures;
    if (methodSignatures.empty()) {
      methodSignatures.addSignature({ "getControllerService", "(Ljava/lang/String;)Lorg/apache/nifi/controller/ControllerService;",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniControllerServiceLookup_getControllerService) });
      methodSignatures.addSignature({ "isControllerServiceEnabled", "(Ljava/lang/String;)Z",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniControllerServiceLookup_isControllerServiceEnabled) });
      methodSignatures.addSignature({ "isControllerServiceEnabling", "(Ljava/lang/String;)Z",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniControllerServiceLookup_isControllerServiceEnabling) });
      methodSignatures.addSignature({ "getControllerServiceName", "(Ljava/lang/String;)Ljava/lang/String;",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniControllerServiceLookup_getControllerServiceName) });
    }
    return methodSignatures;
  }

  static JavaSignatures &getProcessSessionFactorySignatures() {
    static JavaSignatures methodSignatures;
    if (methodSignatures.empty()) {
      methodSignatures.addSignature(
          { "createSession", "()Lorg/apache/nifi/processor/ProcessSession;", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniProcessSessionFactory_createSession) });
    }
    return methodSignatures;
  }

  void notifyStop() override {
    auto localEnv = java_servicer_->attach();

    auto onStoppedName = java_servicer_->getAnnotation(class_name_, "OnStopped");

    try {
      if (!onStoppedName.first.empty() && !onStoppedName.second.empty())
        current_processor_class.callVoidMethod(localEnv, clazzInstance, onStoppedName.first.c_str(), onStoppedName.second);
    } catch (std::runtime_error &re) {
      // this is something that we can ignore.
    }

    std::lock_guard<std::mutex> lock(local_mutex_);

    for (auto &factory : session_factories_) {
      factory->remove();
      delete factory;
    }

    // delete the reference to the jni process session

    if (logger_instance_) {
      localEnv->DeleteGlobalRef(logger_instance_);
      logger_instance_ = nullptr;
    }

    if (init_context_.lookup_ref_) {
      localEnv->DeleteGlobalRef(init_context_.lookup_ref_);
    }
  }

 private:
  JniSessionFactory *getFactory(const std::shared_ptr<core::ProcessSessionFactory> &ptr) {
    std::lock_guard<std::mutex> lock(local_mutex_);
    for (const auto &factory : session_factories_) {
      if (factory->getFactory() == ptr) {
        return factory;
      }
    }
    return nullptr;
  }

  JniSessionFactory *setFactory(const std::shared_ptr<core::ProcessSessionFactory> &ptr, jobject obj) {
    std::lock_guard<std::mutex> lock(local_mutex_);
    JniSessionFactory *factory = new JniSessionFactory(ptr, java_servicer_, obj);
    session_factories_.push_back(factory);
    return factory;
  }

  JNINativeMethod registerNativeMethod(const std::string &name, const std::string &params, const void *ptr);

  JavaClass jni_logger_class_;

  jobject logger_instance_;

  std::mutex local_mutex_;

  std::vector<JniSessionFactory*> session_factories_;

  minifi::jni::JniLogger jni_logger_ref_;

  JavaClass spn;

  JavaClass init;

  minifi::jni::JniProcessContext jpc;

  JavaClass current_processor_class;

  jobject context_instance_;

  jobject clazzInstance;

  std::shared_ptr<controllers::JavaControllerService> java_servicer_;

  std::string class_name_;

  std::shared_ptr<logging::Logger> logger_;

  std::shared_ptr<logging::Logger> nifi_logger_;

  JniControllerServiceLookup csl_;

  JniInitializationContext init_context_;
};

} /* namespace processors */
} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
