/**
 * @file SQLProcessor.h
 * SQLProcessor class declaration
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

#include "core/Core.h"
#include "FlowFileRecord.h"
#include "concurrentqueue.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

template <typename T>
class SQLProcessor: public core::Processor {
 protected:
  SQLProcessor(const std::string& name, utils::Identifier uuid)
    : core::Processor(name, uuid), logger_(logging::LoggerFactory<T>::getLogger()) {
  }

  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override {
    std::string controllerService;
    context->getProperty(dbControllerService().getName(), controllerService);

    dbService_ = std::dynamic_pointer_cast<sql::controllers::DatabaseService>(context->getControllerService(controllerService));
    if (!dbService_)
      throw minifi::Exception(PROCESSOR_EXCEPTION, "'DB Controller Service' must be defined");

    static_cast<T*>(this)->processOnSchedule(context, sessionFactory);
  }

  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override {
    std::unique_lock<std::mutex> lock(onTriggerMutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
      logger_->log_warn("'onTrigger' is called before previous 'onTrigger' call is finished.");
      context->yield();
      return;
    }

    try {
      if (!connection_) {
        connection_ = dbService_->getConnection();
      }
      static_cast<T*>(this)->processOnTrigger(context, session);
    } catch (std::exception& e) {
      logger_->log_error("SQLProcessor: '%s'", e.what());
      if (connection_) {
        std::string exp;
        if (!connection_->connected(exp)) {
          logger_->log_error("SQLProcessor: Connection exception: %s", exp.c_str());
          connection_.reset();
        }
      }
      context->yield();
    }
  }

  void notifyStop() override {
    connection_.reset();
  }

 protected:
   static const core::Property& dbControllerService() {
     static const core::Property s_dbControllerService = 
       core::PropertyBuilder::createProperty("DB Controller Service")->
       isRequired(true)->
       withDescription("Database Controller Service.")->
       supportsExpressionLanguage(true)->
       build();
     return s_dbControllerService;
   }

   std::shared_ptr<logging::Logger> logger_;
   std::shared_ptr<sql::controllers::DatabaseService> dbService_;
   std::unique_ptr<sql::Connection> connection_;
   std::mutex onTriggerMutex_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

