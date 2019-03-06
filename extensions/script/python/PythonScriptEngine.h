/**
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

#ifndef NIFI_MINIFI_CPP_PYTHONSCRIPTENGINE_H
#define NIFI_MINIFI_CPP_PYTHONSCRIPTENGINE_H

#include <mutex>
#include <pybind11/embed.h>
#include <core/ProcessSession.h>
#include <core/Processor.h>

#include "../ScriptEngine.h"
#include "../ScriptProcessContext.h"
#include "PythonProcessor.h"
#include "PyProcessSession.h"
#include "../ScriptException.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace python {

namespace py = pybind11;

struct Interpreter {

  Interpreter()
      : guard_(false) {
  }

  ~Interpreter() {
  }

  Interpreter(const Interpreter &other) = delete;

  py::scoped_interpreter guard_;
  py::gil_scoped_release gil_release_;
};

static Interpreter *getInterpreter();

class PythonScriptEngine : public script::ScriptEngine {
 public:
  PythonScriptEngine();
  virtual ~PythonScriptEngine() {
    py::gil_scoped_acquire gil { };
    (*bindings_).dec_ref();
    (*bindings_).release();
  }

  /**
   * Initializes the python interpreter.
   */
  static void initialize();

  /**
   * Evaluates a python expression.
   *
   * @param script
   */
  void eval(const std::string &script) override;

  /**
   * Evaluates the contents of the given python file name.
   *
   * @param file_name
   */
  void evalFile(const std::string &file_name) override;

  /**
   * Calls the given function, forwarding arbitrary provided parameters.
   *
   * @return
   */
  template<typename ... Args>
  void call(const std::string &fn_name, Args &&...args) {
    py::gil_scoped_acquire gil { };
    try {
      if ((*bindings_).contains(fn_name.c_str()))
        (*bindings_)[fn_name.c_str()](convert(args)...);
    } catch (const std::exception &e) {
      throw minifi::script::ScriptException(e.what());
    }
  }

  /**
   * Calls the given function, forwarding arbitrary provided parameters.
   *
   * @return
   */
  template<typename ... Args>
  void callRequiredFunction(const std::string &fn_name, Args &&...args) {
    py::gil_scoped_acquire gil { };
    if (!(*bindings_).contains(fn_name.c_str()))
      throw std::runtime_error("Required Function" + fn_name + " is not found within Python bindings");
    (*bindings_)[fn_name.c_str()](convert(args)...);
  }

  class TriggerSession {
   public:
    TriggerSession(std::shared_ptr<script::ScriptProcessContext> script_context, std::shared_ptr<python::PyProcessSession> py_session)
        : script_context_(std::move(script_context)),
          py_session_(std::move(py_session)) {
    }

    ~TriggerSession() {
      script_context_->releaseProcessContext();
      py_session_->releaseCoreResources();
    }

   private:
    std::shared_ptr<script::ScriptProcessContext> script_context_;
    std::shared_ptr<python::PyProcessSession> py_session_;
  };

  class TriggerProcessor {
   public:
    TriggerProcessor(std::shared_ptr<script::ScriptProcessContext> script_context, std::shared_ptr<python::PyProcessSession> py_session)
        : script_context_(std::move(script_context)),
          py_session_(std::move(py_session)) {
    }

    ~TriggerProcessor() {
      script_context_->releaseProcessContext();
      py_session_->releaseCoreResources();
    }

   private:
    std::shared_ptr<script::ScriptProcessContext> script_context_;
    std::shared_ptr<python::PyProcessSession> py_session_;
  };

  class TriggerSchedule {
   public:
    TriggerSchedule(std::shared_ptr<script::ScriptProcessContext> script_context)
        : script_context_(script_context) {
    }

    ~TriggerSchedule() {
      script_context_->releaseProcessContext();
    }

   private:
    std::shared_ptr<script::ScriptProcessContext> script_context_;
  };

  class TriggerInit {
   public:
    TriggerInit() {
    }

    ~TriggerInit() {
    }

   private:
    std::shared_ptr<script::ScriptProcessContext> script_context_;
  };

  void onInitialize(const std::shared_ptr<core::Processor> &proc) {
    TriggerInit trigger_session;
    auto newproc = convertProcessor(proc);
    call("onInitialize", newproc);
  }

  void describe(const std::shared_ptr<core::Processor> &proc) {
    TriggerInit trigger_session;
    auto newproc = convertProcessor(proc);
    callRequiredFunction("describe", newproc);
  }

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context) {
    auto script_context = convertContext(context);
    TriggerSchedule trigger_session(script_context);
    call("onSchedule", script_context);
  }

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
    auto script_context = convertContext(context);
    auto py_session = convertSession(session);
    TriggerSession trigger_session(script_context, py_session);
    call("onTrigger", script_context, py_session);
  }

  /**
   * Binds an object into the scope of the python interpreter.
   * @tparam T
   * @param name
   * @param value
   */
  template<typename T>
  void bind(const std::string &name, const T &value) {
    py::gil_scoped_acquire gil { };
    (*bindings_)[name.c_str()] = convert(value);
  }

  template<typename T>
  py::object convert(const T &value) {
    py::gil_scoped_acquire gil { };
    return py::cast(value);
  }

  std::shared_ptr<python::PyProcessSession> convertSession(const std::shared_ptr<core::ProcessSession> &session) {
    return std::make_shared<python::PyProcessSession>(session);
  }

  std::shared_ptr<script::ScriptProcessContext> convertContext(const std::shared_ptr<core::ProcessContext> &context) {
    return std::make_shared<script::ScriptProcessContext>(context);
  }

  std::shared_ptr<python::PythonProcessor> convertProcessor(const std::shared_ptr<core::Processor> &proc) {
    return std::make_shared<python::PythonProcessor>(proc);
  }

 private:
  std::unique_ptr<py::dict> bindings_;
};

} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_PYTHONSCRIPTENGINE_H
