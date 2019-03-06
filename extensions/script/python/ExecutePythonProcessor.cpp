/**
 * @file ExecuteScript.cpp

 * ExecuteScript class implementation
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

#include <memory>
#include <set>
#include <utility>
#include <exception>
#include <stdexcept>

#include "ExecutePythonProcessor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace python {
namespace processors {

core::Property ExecutePythonProcessor::ScriptFile("Script File",  // NOLINT
    R"(Path to script file to execute.
                                            Only one of Script File or Script Body may be used)", "");
core::Property ExecutePythonProcessor::ModuleDirectory("Module Directory",  // NOLINT
    R"(Comma-separated list of paths to files and/or directories which
                                                 contain modules required by the script)", "");

core::Relationship ExecutePythonProcessor::Success("success", "Script successes");  // NOLINT
core::Relationship ExecutePythonProcessor::Failure("failure", "Script failures");  // NOLINT

void ExecutePythonProcessor::initialize() {
  // initialization requires that we do a little leg work prior to onSchedule
  // so that we can provide manifest our processor identity
  std::set<core::Property> properties;

  std::string prop;
  getProperty(ScriptFile.getName(), prop);

  properties.insert(ScriptFile);
  properties.insert(ModuleDirectory);
  setSupportedProperties(properties);
  setSupportedProperties(std::move(properties));

  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Failure);
  setSupportedRelationships(std::move(relationships));

  if (!prop.empty()) {
    setProperty(ScriptFile, prop);
    std::shared_ptr<script::ScriptEngine> engine;
    python_logger_ = logging::LoggerFactory<ExecutePythonProcessor>::getAliasedLogger(getName());

    engine = createEngine<python::PythonScriptEngine>();

    if (engine == nullptr) {
      throw std::runtime_error("No script engine available");
    }

    try {
      engine->evalFile(prop);
      auto me = shared_from_this();
      triggerDescribe(engine, me);
      triggerInitialize(engine, me);
      valid_init_ = true;
    } catch (std::exception &exception) {
      logger_->log_error("Caught Exception %s", exception.what());
      engine = nullptr;
      std::rethrow_exception(std::current_exception());
      valid_init_ = false;
    } catch (...) {
      logger_->log_error("Caught Exception");
      engine = nullptr;
      std::rethrow_exception(std::current_exception());
      valid_init_ = false;
    }

  }
}

void ExecutePythonProcessor::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  if (!valid_init_) {
    throw std::runtime_error("Could not correctly in initialize " + getName());
  }
  context->getProperty(ScriptFile.getName(), script_file_);
  context->getProperty(ModuleDirectory.getName(), module_directory_);
  if (script_file_.empty() && script_engine_.empty()) {
    logger_->log_error("Either Script Body or Script File must be defined");
    return;
  }

  try {
    std::shared_ptr<script::ScriptEngine> engine;

    // Use an existing engine, if one is available
    if (script_engine_q_.try_dequeue(engine)) {
      logger_->log_debug("Using available %s script engine instance", script_engine_);
    } else {
      logger_->log_info("Creating new %s script instance", script_engine_);
      logger_->log_info("Approximately %d %s script instances created for this processor", script_engine_q_.size_approx(), script_engine_);

      engine = createEngine<python::PythonScriptEngine>();

      if (engine == nullptr) {
        throw std::runtime_error("No script engine available");
      }

      if (!script_body_.empty()) {
        engine->eval(script_body_);
      } else if (!script_file_.empty()) {
        engine->evalFile(script_file_);
      } else {
        throw std::runtime_error("Neither Script Body nor Script File is available to execute");
      }
    }

    triggerSchedule(engine, context);

    // Make engine available for use again
    if (script_engine_q_.size_approx() < getMaxConcurrentTasks()) {
      logger_->log_debug("Releasing %s script engine", script_engine_);
      script_engine_q_.enqueue(engine);
    } else {
      logger_->log_info("Destroying script engine because it is no longer needed");
    }
  } catch (std::exception &exception) {
    logger_->log_error("Caught Exception %s", exception.what());
  } catch (...) {
    logger_->log_error("Caught Exception");
  }
}

void ExecutePythonProcessor::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  try {
    std::shared_ptr<script::ScriptEngine> engine;

    // Use an existing engine, if one is available
    if (script_engine_q_.try_dequeue(engine)) {
      logger_->log_debug("Using available %s script engine instance", script_engine_);
    } else {
      logger_->log_info("Creating new %s script instance", script_engine_);
      logger_->log_info("Approximately %d %s script instances created for this processor", script_engine_q_.size_approx(), script_engine_);

      engine = createEngine<python::PythonScriptEngine>();

      if (engine == nullptr) {
        throw std::runtime_error("No script engine available");
      }

      if (!script_body_.empty()) {
        engine->eval(script_body_);
      } else if (!script_file_.empty()) {
        engine->evalFile(script_file_);
      } else {
        throw std::runtime_error("Neither Script Body nor Script File is available to execute");
      }
    }

    triggerEngineProcessor(engine, context, session);

    // Make engine available for use again
    if (script_engine_q_.size_approx() < getMaxConcurrentTasks()) {
      logger_->log_debug("Releasing %s script engine", script_engine_);
      script_engine_q_.enqueue(engine);
    } else {
      logger_->log_info("Destroying script engine because it is no longer needed");
    }
  } catch (std::exception &exception) {
    logger_->log_error("Caught Exception %s", exception.what());
    this->yield();
  } catch (...) {
    logger_->log_error("Caught Exception");
    this->yield();
  }
}

} /* namespace processors */
} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
