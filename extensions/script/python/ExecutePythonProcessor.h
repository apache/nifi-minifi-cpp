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

#ifndef EXTENSIONS_SCRIPT_PYTHON_EXECUTEPYTHONPROCESSOR_H_
#define EXTENSIONS_SCRIPT_PYTHON_EXECUTEPYTHONPROCESSOR_H_

#include <memory>
#include <string>
#include <vector>

#include "concurrentqueue.h"
#include "core/Resource.h"
#include "core/Processor.h"

#include "../ScriptEngine.h"
#include "../ScriptProcessContext.h"
#include "PythonScriptEngine.h"
#include "core/Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace python {
namespace processors {

class ExecutePythonProcessor : public core::Processor {
 public:
  explicit ExecutePythonProcessor(const std::string &name, utils::Identifier uuid = utils::Identifier())
      : Processor(name, uuid),
        python_dynamic_(false),
        valid_init_(false),
        logger_(logging::LoggerFactory<ExecutePythonProcessor>::getLogger()),
        script_engine_q_() {
  }

  static core::Property ScriptFile;
  static core::Property ScriptBody;
  static core::Property ModuleDirectory;

  static core::Relationship Success;
  static core::Relationship Failure;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

  void setSupportsDynamicProperties() {
    python_dynamic_ = true;
  }

  void addProperty(const std::string &name, const std::string &description, const std::string &defaultvalue, bool required, bool el) {
    python_properties_.emplace_back(
        core::PropertyBuilder::createProperty(name)->withDefaultValue(defaultvalue)->withDescription(description)->isRequired(required)->supportsExpressionLanguage(el)->build());
  }

  std::vector<core::Property> getPythonProperties() const {
    return python_properties_;
  }

  bool getPythonSupportDynamicProperties() {
    return python_dynamic_;
  }

  void setDescription(const std::string &description) {
    description_ = description;
  }

  const std::string &getDescription() const {
    return description_;
  }

  bool supportsDynamicProperties() override {
    return false;
  }

 private:
  std::vector<core::Property> python_properties_;

  std::string description_;

  bool python_dynamic_;

  bool valid_init_;

  std::shared_ptr<logging::Logger> logger_;
  std::shared_ptr<logging::Logger> python_logger_;

  std::string script_to_exec_;
  std::string module_directory_;

  moodycamel::ConcurrentQueue<std::shared_ptr<python::PythonScriptEngine>> script_engine_q_;

  std::shared_ptr<python::PythonScriptEngine> getScriptEngine();
  void handleEngineNoLongerInUse(std::shared_ptr<python::PythonScriptEngine>&& engine);
  void appendPathForImportModules();
  void loadScriptFromFile(const std::string& file_path);
  void loadScript();
  void reloadScriptIfUsingScriptFileProperty();


  template<typename T>
  std::shared_ptr<T> createEngine() const {
    auto engine = std::make_shared<T>();

    engine->bind("log", python_logger_);
    engine->bind("REL_SUCCESS", Success);
    engine->bind("REL_FAILURE", Failure);

    return engine;
  }
};

REGISTER_RESOURCE(
    ExecutePythonProcessor, "Executes a script given the flow file and a process session. The script is responsible for handling the incoming flow file (transfer to SUCCESS or remove, e.g.) "
    "as well as any flow files created by the script. If the handling is incomplete or incorrect, the session will be rolled back.Scripts must define an onTrigger function which accepts NiFi Context"
    " and Property objects. For efficiency, scripts are executed once when the processor is run, then the onTrigger method is called for each incoming flowfile. This enables scripts to keep state "
    "if they wish, although there will be a script context per concurrent task of the processor. In order to, e.g., compute an arithmetic sum based on incoming flow file information, set the "
    "concurrent tasks to 1.");  // NOLINT

} /* namespace processors */
} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // EXTENSIONS_SCRIPT_PYTHON_EXECUTEPYTHONPROCESSOR_H_
