/**
 * @file ExecuteScript.h
 * ExecuteScript class declaration
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

#ifndef NIFI_MINIFI_CPP_EXECUTESCRIPT_H
#define NIFI_MINIFI_CPP_EXECUTESCRIPT_H

#include <concurrentqueue.h>
#include <core/Resource.h>
#include <core/Processor.h>

#include "ScriptEngine.h"
#include "ScriptProcessContext.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class ExecuteScript : public core::Processor {
 public:
  explicit ExecuteScript(const std::string &name, utils::Identifier uuid = utils::Identifier())
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<ExecuteScript>::getLogger()),
        script_engine_q_() {
  }

  static core::Property ScriptEngine;
  static core::Property ScriptFile;
  static core::Property ScriptBody;
  static core::Property ModuleDirectory;

  static core::Relationship Success;
  static core::Relationship Failure;

  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override {
    logger_->log_error("onTrigger invocation with raw pointers is not implemented");
  }
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                 const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  std::shared_ptr<logging::Logger> logger_;

  std::string script_engine_;
  std::string script_file_;
  std::string script_body_;
  std::string module_directory_;

  moodycamel::ConcurrentQueue<std::shared_ptr<script::ScriptEngine>> script_engine_q_;

  template<typename T>
  std::shared_ptr<T> createEngine() const {
    auto engine = std::make_shared<T>();

    engine->bind("log", logger_);
    engine->bind("REL_SUCCESS", Success);
    engine->bind("REL_FAILURE", Failure);

    return std::move(engine);
  }

  template<typename T>
  void triggerEngineProcessor(const std::shared_ptr<script::ScriptEngine> &engine,
                              const std::shared_ptr<core::ProcessContext> &context,
                              const std::shared_ptr<core::ProcessSession> &session) const {
    auto typed_engine = std::static_pointer_cast<T>(engine);
    typed_engine->onTrigger(context, session);
  }
};

REGISTER_RESOURCE(ExecuteScript, "Executes a script given the flow file and a process session. The script is responsible for handling the incoming flow file (transfer to SUCCESS or remove, e.g.) "
    "as well as any flow files created by the script. If the handling is incomplete or incorrect, the session will be rolled back.Scripts must define an onTrigger function which accepts NiFi Context"
    " and Property objects. For efficiency, scripts are executed once when the processor is run, then the onTrigger method is called for each incoming flowfile. This enables scripts to keep state "
    "if they wish, although there will be a script context per concurrent task of the processor. In order to, e.g., compute an arithmetic sum based on incoming flow file information, set the "
    "concurrent tasks to 1."); // NOLINT

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_EXECUTESCRIPT_H
