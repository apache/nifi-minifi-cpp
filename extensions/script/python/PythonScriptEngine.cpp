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

#include <memory>
#include <string>
#include <filesystem>
#include <cstdio>

#include "PythonScriptEngine.h"
#include "PythonBindings.h"
#include "PyProcessSession.h"
#include "PyProcessContext.h"
#include "PyProcessor.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::python {

NewInterpreter* getNewInterpreter() {
  static NewInterpreter interpreter;
  return &interpreter;
}

GilScopedRelease::GilScopedRelease()
    : thread_state_(PyEval_SaveThread()) {
}

GilScopedRelease::~GilScopedRelease() {
  if (!thread_state_) {
    return;
  }
  PyEval_RestoreThread(thread_state_);
}

GilScopedAcquire::GilScopedAcquire() {
  thread_state_ = reinterpret_cast<PyThreadState*>(PyThread_tss_get(getNewInterpreter()->main_tss_));
  if (!thread_state_) {
    thread_state_ = PyGILState_GetThisThreadState();
  }

  if (!thread_state_) {
    thread_state_ = PyThreadState_New(getNewInterpreter()->interpreter_state_);
    thread_state_->gilstate_counter = 0;
    PyThread_tss_set(getNewInterpreter()->main_tss_, thread_state_);
  } else {
    auto* threadDict = PyThreadState_GetDict();
    if (threadDict != nullptr) {
      release_ = (PyThreadState_Get() != thread_state_);
    }
  }

  if (release_) {
    PyEval_AcquireThread(thread_state_);
  }

  ++thread_state_->gilstate_counter;
}

GilScopedAcquire::~GilScopedAcquire() {
  --thread_state_->gilstate_counter;
  if (thread_state_->gilstate_counter == 0) {
    PyThreadState_Clear(thread_state_);
    PyThreadState_DeleteCurrent();
    PyThread_tss_delete(getNewInterpreter()->main_tss_);
    release_ = false;
  }
  if (release_) {
    PyEval_SaveThread();
  }
}

NewInterpreter::NewInterpreter() {
  Py_Initialize();
  PyInit_minifi_native();

  // Create GIL/enable threads
  PyEval_InitThreads();
  
  // Get the default thread state
  thread_state_ = PyThreadState_Get();
  interpreter_state_ = thread_state_->interp;
  main_tss_ = PyThread_tss_alloc();
  if (!main_tss_ || PyThread_tss_create(main_tss_)) {
    throw std::runtime_error("Couldn't create Python interpreter's main thread's TSS");
  }

  PyThread_tss_set(main_tss_, thread_state_);

  saved_thread_state_ = PyEval_SaveThread();
}

NewInterpreter::~NewInterpreter() {
  PyEval_RestoreThread(saved_thread_state_);
  PyThread_tss_free(main_tss_);
  Py_Finalize();
}

Interpreter *getInterpreter() {
  static Interpreter interpreter;
  return &interpreter;
}

NewPythonScriptEngine::NewPythonScriptEngine() {
  getNewInterpreter();

  GilScopedAcquire lock;
  bindings_ = OwnedDict::create();
}

NewPythonScriptEngine::~NewPythonScriptEngine() {
  GilScopedAcquire lock;
  bindings_.resetReference();
}

void NewPythonScriptEngine::eval(const std::string& script) {
  GilScopedAcquire gil;
  try {
    evaluateModuleImports();
    evalInternal(script);
  } catch (const std::exception& e) {
    throw minifi::script::ScriptException(e.what());
  }
}

void NewPythonScriptEngine::evalFile(const std::string& fileName) {
  GilScopedAcquire gil;
  try {
    evaluateModuleImports();
    auto file = fopen(fileName.c_str(), "r");
    const auto result = OwnedObject(PyRun_FileEx(file, fileName.c_str(), Py_file_input, bindings_.get(), bindings_.get(), 1));
    if (!result.get()) {
      throw PyException();
    }
  } catch (const std::exception &e) {
    throw minifi::script::ScriptException(e.what());
  }
}

void NewPythonScriptEngine::onInitialize(core::Processor* proc) {
  auto newproc = std::make_shared<python::PythonProcessor>(proc);
  call("onInitialize", std::weak_ptr(newproc));
}

void NewPythonScriptEngine::describe(core::Processor* proc) {
  auto newproc = std::make_shared<python::PythonProcessor>(proc);
  callRequiredFunction("describe", std::weak_ptr(newproc));
}

void NewPythonScriptEngine::onSchedule(const std::shared_ptr<core::ProcessContext> &context) {
  auto script_context = std::make_shared<script::ScriptProcessContext>(context);
  call("onSchedule", std::weak_ptr(script_context));
}

void NewPythonScriptEngine::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  auto py_session = std::make_shared<python::PyProcessSession>(session);
  auto script_context = std::make_shared<script::ScriptProcessContext>(context);
  call("onTrigger", std::weak_ptr(script_context), std::weak_ptr(py_session));
}

void NewPythonScriptEngine::evalInternal(std::string_view script) {
  const auto script_file = "# -*- coding: utf-8 -*-\n" + std::string(script);
  const auto result = OwnedObject(PyRun_String(script_file.c_str(), Py_file_input, bindings_.get(), bindings_.get()));
  if (!result.get()) {
    throw PyException();
  }
}

void NewPythonScriptEngine::evaluateModuleImports() {
  if (module_paths_.empty()) {
    return;
  }

  evalInternal("import sys");
  for (const auto& module_path : module_paths_) {
    if (std::filesystem::is_regular_file(std::filesystem::status(module_path))) {
      std::string path;
      std::string filename;
      utils::file::getFileNameAndPath(module_path, path, filename);
      evalInternal("sys.path.append(r'" + path + "')");
    } else {
      evalInternal("sys.path.append(r'" + module_path + "')");
    }
  }
}


PythonScriptEngine::PythonScriptEngine() {
  getInterpreter();
  py::gil_scoped_acquire gil { };
  py::module::import("minifi_native");
  bindings_ = std::make_unique<py::dict>();
  (*bindings_) = py::globals().attr("copy")();
}

void PythonScriptEngine::evaluateModuleImports() {
  if (module_paths_.empty()) {
    return;
  }

  py::eval<py::eval_statements>("import sys", *bindings_, *bindings_);
  for (const auto& module_path : module_paths_) {
    if (std::filesystem::is_regular_file(module_path)) {
      py::eval<py::eval_statements>("sys.path.append(r'" + module_path.parent_path().string() + "')", *bindings_, *bindings_);
    } else {
      py::eval<py::eval_statements>("sys.path.append(r'" + module_path.string() + "')", *bindings_, *bindings_);
    }
  }
}

void PythonScriptEngine::eval(const std::string &script) {
  py::gil_scoped_acquire gil { };
  try {
    evaluateModuleImports();
    if (script[0] == '\n') {
      py::eval<py::eval_statements>(py::module::import("textwrap").attr("dedent")(script), *bindings_, *bindings_);
    } else {
      py::eval<py::eval_statements>(script, *bindings_, *bindings_);
    }
  } catch (std::exception& e) {
     throw minifi::script::ScriptException(e.what());
  }
}

void PythonScriptEngine::evalFile(const std::string &file_name) {
  py::gil_scoped_acquire gil { };
  try {
    evaluateModuleImports();
    py::eval_file(file_name, *bindings_, *bindings_);
  } catch (const std::exception &e) {
    throw minifi::script::ScriptException(e.what());
  }
}

void PythonScriptEngine::initialize() {
  // getInterpreter();
}

}  // namespace org::apache::nifi::minifi::python
