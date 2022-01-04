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

#pragma once

#include <string>
#include <memory>

#include "concurrentqueue.h"
#include "core/Processor.h"

#include "ScriptEngine.h"
#include "ScriptProcessContext.h"
#include "PythonScriptEngine.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class ScriptEngineFactory {
 public:
  ScriptEngineFactory(core::Relationship& success, core::Relationship& failure, std::shared_ptr<core::logging::Logger> logger);

  template<typename T>
  std::shared_ptr<T> createEngine() const {
    auto engine = std::make_shared<T>();

    engine->bind("log", logger_);
    engine->bind("REL_SUCCESS", success_);
    engine->bind("REL_FAILURE", failure_);

    return engine;
  }

 private:
  core::Relationship& success_;
  core::Relationship& failure_;
  std::shared_ptr<core::logging::Logger> logger_;
};

class ScriptEngineQueue {
 public:
  ScriptEngineQueue(uint8_t max_engine_count, ScriptEngineFactory& engine_factory, std::shared_ptr<core::logging::Logger> logger);

  template<typename T>
  std::shared_ptr<T> getScriptEngine() {
    std::shared_ptr<T> engine;
    // Use an existing engine, if one is available
    if (engine_queue_.try_dequeue(engine)) {
      logger_->log_debug("Using available [%p] script engine instance", engine.get());
      return engine;
    } else {
      const std::lock_guard<std::mutex> lock(counter_mutex_);
      if (engine_instance_count_ < max_engine_count_) {
        ++engine_instance_count_;
        engine = engine_factory_.createEngine<T>();
        logger_->log_info("Created new [%p] script engine instance. Number of instances: %d / %d.", engine.get(), engine_instance_count_, max_engine_count_);
        return engine;
      }
    }

    std::unique_lock<std::mutex> lock(queue_mutex_);
    logger_->log_debug("Waiting for available script engine instance...");
    queue_cv_.wait(lock, [this](){ return engine_queue_.size_approx() > 0; });
    if (!engine_queue_.try_dequeue(engine)) {
      throw std::runtime_error("No script engine available");
    }
    return engine;
  }

  void returnScriptEngine(std::shared_ptr<script::ScriptEngine>&& engine);

 private:
  const uint8_t max_engine_count_;
  ScriptEngineFactory& engine_factory_;
  std::shared_ptr<core::logging::Logger> logger_;
  moodycamel::ConcurrentQueue<std::shared_ptr<script::ScriptEngine>> engine_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  uint8_t engine_instance_count_ = 0;
  std::mutex counter_mutex_;
};

class ExecuteScript : public core::Processor {
 public:
  explicit ExecuteScript(const std::string &name, const utils::Identifier &uuid = {})
      : Processor(name, uuid),
        engine_factory_(Success, Failure, logger_) {
  }

  static core::Property ScriptEngine;
  static core::Property ScriptFile;
  static core::Property ScriptBody;
  static core::Property ModuleDirectory;

  static core::Relationship Success;
  static core::Relationship Failure;

  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* /*session*/) override {
    logger_->log_error("onTrigger invocation with raw pointers is not implemented");
  }
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                 const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ExecuteScript>::getLogger();

  std::string script_engine_;
  std::string script_file_;
  std::string script_body_;
  std::string module_directory_;

  ScriptEngineFactory engine_factory_;
  std::unique_ptr<ScriptEngineQueue> script_engine_q_;
  std::shared_ptr<python::PythonScriptEngine> python_script_engine_;

  template<typename T>
  void triggerEngineProcessor(const std::shared_ptr<script::ScriptEngine> &engine,
                              const std::shared_ptr<core::ProcessContext> &context,
                              const std::shared_ptr<core::ProcessSession> &session) const {
    auto typed_engine = std::static_pointer_cast<T>(engine);
    typed_engine->onTrigger(context, session);
  }
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
