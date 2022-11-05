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

#pragma once

#include "PythonBindings.h"
#include "Exception.h"

#include <mutex>
#include <memory>
#include <utility>
#include <exception>

#include "pybind11/embed.h"
#include "core/ProcessSession.h"
#include "core/Processor.h"

#include "../ScriptEngine.h"
#include "../ScriptProcessContext.h"
#include "PythonProcessor.h"
#include "PyProcessSession.h"
#include "../ScriptException.h"


#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC visibility push(hidden)
#endif

namespace org::apache::nifi::minifi::python {

namespace py = pybind11;

class GilScopedRelease {
 public:
  GilScopedRelease();
  ~GilScopedRelease();

 private:
  PyThreadState *thread_state_ = nullptr;
};

#if defined(__GNUC__) || defined(__GNUG__)
class __attribute__((visibility("default"))) GilScopedAcquire {
#else
class GilScopedAcquire {
#endif
 public:
  GilScopedAcquire();
  ~GilScopedAcquire();

 private:
  PyThreadState *thread_state_ = nullptr;
  bool release_ = true;
};

struct Interpreter {
  Interpreter()
      : guard_(false) {
  }

  ~Interpreter() = default;

  Interpreter(const Interpreter &other) = delete;

  py::scoped_interpreter guard_;
  py::gil_scoped_release gil_release_;
};

struct NewInterpreter {
  NewInterpreter();
  ~NewInterpreter();

  NewInterpreter(const Interpreter &other) = delete;
  NewInterpreter(Interpreter &&other) = delete;
  NewInterpreter& operator=(const Interpreter &other) = delete;
  NewInterpreter& operator=(Interpreter &&other) = delete;

  PyThreadState* thread_state_ = nullptr;
  PyThreadState* saved_thread_state_ = nullptr;
  PyInterpreterState* interpreter_state_ = nullptr;
  Py_tss_t* main_tss_ = nullptr;
};

Interpreter *getInterpreter();
NewInterpreter* getNewInterpreter();

#if defined(__GNUC__) || defined(__GNUG__)
class __attribute__((visibility("default"))) NewPythonScriptEngine : public script::ScriptEngine {
#else
class NewPythonScriptEngine : public script::ScriptEngine {
#endif
 public:
  NewPythonScriptEngine();
  virtual ~NewPythonScriptEngine();

  NewPythonScriptEngine(const NewPythonScriptEngine &other) = delete;
  NewPythonScriptEngine(NewPythonScriptEngine &&other) = delete;
  NewPythonScriptEngine& operator=(const NewPythonScriptEngine &other) = delete;
  NewPythonScriptEngine& operator=(NewPythonScriptEngine &&other) = delete;

  static void initialize();

  void eval(const std::string &script) override;
  void evalFile(const std::string &file_name) override;

  template<typename... Args>
  void call(std::string_view fn_name, Args &&...args) {
    GilScopedAcquire gil_lock;
    try {
      if (auto item = bindings_[fn_name]) {
        auto result = BorrowedCallable(*item)(std::forward<Args>(args)...);
        if (!result) {
          throw PyException();
        }
      }
    } catch (const std::exception &e) {
      throw minifi::script::ScriptException(e.what());
    }
  }

  template<typename ... Args>
  void callRequiredFunction(const std::string &fn_name, Args &&...args) {
    GilScopedAcquire gil_lock;
    if (auto item = bindings_[fn_name]) {
      auto result = BorrowedCallable(*item)(std::forward<Args>(args)...);
      if (!result) {
        throw PyException();
      }
    } else {
      throw std::runtime_error("Required Function" + fn_name + " is not found within Python bindings");
    }
  }

  template<object::convertible T>
  void bind(const std::string &name, const T &value) {
    GilScopedAcquire gil_lock;
    bindings_.put(name, value);
    // (*bindings_)[name.c_str()] = convert(value);
  }

  void onInitialize(core::Processor* proc);
  void describe(core::Processor* proc);
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context);
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
 private:
  void evalInternal(std::string_view script);

  void evaluateModuleImports();
  OwnedDict bindings_;
};

#if defined(__GNUC__) || defined(__GNUG__)
class __attribute__((visibility("default"))) PythonScriptEngine : public script::ScriptEngine {
#else
class PythonScriptEngine : public script::ScriptEngine {
#endif
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
      throw std::runtime_error("Required Function " + fn_name + " is not found within Python bindings");
    (*bindings_)[fn_name.c_str()](convert(args)...);
  }

  class TriggerSession {
   public:
    TriggerSession(script::ScriptProcessContext script_context, python::PyProcessSession py_session)
        : script_context_(std::move(script_context)),
          py_session_(std::move(py_session)) {
    }

   private:
    script::ScriptProcessContext script_context_;
    python::PyProcessSession py_session_;
  };

  class TriggerSchedule {
   public:
    explicit TriggerSchedule(script::ScriptProcessContext script_context)
        : script_context_(std::move(script_context)) {
    }

   private:
    script::ScriptProcessContext script_context_;
  };

  void onInitialize(core::Processor* proc) {
    auto newproc = convertProcessor(proc);
    call("onInitialize", newproc);
  }

  void describe(core::Processor* proc) {
    auto newproc = convertProcessor(proc);
    callRequiredFunction("describe", newproc);
  }

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context) {
    auto script_context = convertContext(context);
    // TriggerSchedule trigger_session(script_context);
    call("onSchedule", std::weak_ptr(script_context));
  }

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override {
    auto script_context = convertContext(context);
    auto py_session = convertSession(session);
    // TriggerSession trigger_session(script_context, py_session);
    call("onTrigger", std::weak_ptr(script_context), std::weak_ptr(py_session));
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

  std::shared_ptr<python::PythonProcessor> convertProcessor(core::Processor* proc) {
    return std::make_shared<python::PythonProcessor>(proc);
  }

 private:
  void evaluateModuleImports();

  std::unique_ptr<py::dict> bindings_;
};

}  // namespace org::apache::nifi::minifi::python

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC visibility pop
#endif
