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

#include <string>
#include <filesystem>

#include "PythonScriptEngine.h"
#include "PythonBindings.h"
#include "types/PyProcessSession.h"
#include "types/PyProcessContext.h"
#include "types/PyProcessor.h"
#include "types/PyLogger.h"
#include "types/PyRelationship.h"

namespace org::apache::nifi::minifi::extensions::python {

Interpreter* Interpreter::getInterpreter() {
  static Interpreter interpreter;
  return &interpreter;
}

GlobalInterpreterLock::GlobalInterpreterLock()
    : gil_state_(PyGILState_Ensure()) {
}

GlobalInterpreterLock::~GlobalInterpreterLock() {
  PyGILState_Release(gil_state_);
}

namespace {
// PyEval_InitThreads might be marked deprecated (depending on the version of Python.h)
// Python <= 3.6: This needs to be called manually after Py_Initialize to initialize threads
// Python >= 3.7: Noop function since its functionality is included in Py_Initialize
// Python >= 3.9: Marked as deprecated (still noop)
// This can be removed if we drop the support for Python 3.6
void initThreads() {
#if defined(__clang__)
  #pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(WIN32)
  #pragma warning(push)
#pragma warning(disable: 4996)
#endif
  if (!PyEval_ThreadsInitialized())
    PyEval_InitThreads();
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(WIN32)
#pragma warning(pop)
#endif
}
}  // namespace

Interpreter::Interpreter() {
  Py_Initialize();
  initThreads();
  PyInit_minifi_native();
  saved_thread_state_ = PyEval_SaveThread();  // NOLINT(cppcoreguidelines-prefer-member-initializer)
}

Interpreter::~Interpreter() {
  PyEval_RestoreThread(saved_thread_state_);
  Py_Finalize();
}

PythonScriptEngine::PythonScriptEngine() {
  Interpreter::getInterpreter();

  GlobalInterpreterLock lock;
  bindings_ = OwnedDict::create();
}

PythonScriptEngine::~PythonScriptEngine() {
  GlobalInterpreterLock lock;
  bindings_.resetReference();
}

void PythonScriptEngine::eval(const std::string& script) {
  GlobalInterpreterLock gil;
  try {
    evaluateModuleImports();
    evalInternal(script);
  } catch (const std::exception& e) {
    throw PythonScriptException(e.what());
  }
}

void PythonScriptEngine::evalFile(const std::filesystem::path& file_name) {
  GlobalInterpreterLock gil;
  try {
    evaluateModuleImports();
    std::ifstream file(file_name, std::ios::in);
    if (!file.is_open()) {
      throw PythonScriptException(fmt::format("Couldn't open {}", file_name.string()));
    }
    std::string content{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

    auto compiled_string = OwnedObject(Py_CompileString(content.c_str(), file_name.string().c_str(), Py_file_input));
    if (!compiled_string.get()) {
      throw PyException();
    }
    const auto result = OwnedObject(PyEval_EvalCode(compiled_string.get(), bindings_.get(), bindings_.get()));
    if (!result.get()) {
      throw PyException();
    }
  } catch (const std::exception &e) {
    throw PythonScriptException(e.what());
  }
}

void PythonScriptEngine::onInitialize(core::Processor* proc) {
  auto newproc = std::make_shared<python::PythonProcessor>(proc);
  call("onInitialize", std::weak_ptr(newproc));
}

void PythonScriptEngine::describe(core::Processor* proc) {
  auto newproc = std::make_shared<python::PythonProcessor>(proc);
  callRequiredFunction("describe", std::weak_ptr(newproc));
}

void PythonScriptEngine::onSchedule(const std::shared_ptr<core::ProcessContext> &context) {
  call("onSchedule", std::weak_ptr(context));
}

void PythonScriptEngine::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  auto py_session = std::make_shared<python::PyProcessSession>(session);
  call("onTrigger", std::weak_ptr(context), std::weak_ptr(py_session));
}

void PythonScriptEngine::initialize(const core::Relationship& success, const core::Relationship& failure, const std::shared_ptr<core::logging::Logger>& logger) {
  bind("log", std::weak_ptr<core::logging::Logger>(logger));
  bind("REL_SUCCESS", success);
  bind("REL_FAILURE", failure);
}

void PythonScriptEngine::evalInternal(std::string_view script) {
  const auto script_file = "# -*- coding: utf-8 -*-\n" + std::string(script);
  auto compiled_string = OwnedObject(Py_CompileString(script_file.c_str(), "<string>", Py_file_input));
  if (!compiled_string.get()) {
    throw PyException();
  }
  const auto result = OwnedObject(PyEval_EvalCode(compiled_string.get(), bindings_.get(), bindings_.get()));
  if (!result.get()) {
    throw PyException();
  }
}

void PythonScriptEngine::evaluateModuleImports() {
  bindings_.put("__builtins__", OwnedObject(PyImport_ImportModule("builtins")));
  if (module_paths_.empty()) {
    return;
  }

  evalInternal("import sys");
  for (const auto& module_path : module_paths_) {
    if (std::filesystem::is_regular_file(module_path)) {
      evalInternal("sys.path.append(r'" + module_path.parent_path().string() + "')");
    } else {
      evalInternal("sys.path.append(r'" + module_path.string() + "')");
    }
  }
}

}  // namespace org::apache::nifi::minifi::extensions::python
