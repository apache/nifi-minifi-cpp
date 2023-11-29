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
#include "PyException.h"

#include <mutex>
#include <memory>
#include <utility>
#include <exception>
#include <string>
#include <vector>

#include "core/ProcessSession.h"
#include "core/Processor.h"

#include "PythonProcessor.h"
#include "types/PyProcessSession.h"
#include "PythonScriptException.h"

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC visibility push(hidden)
#endif

namespace org::apache::nifi::minifi::extensions::python {

#if defined(__GNUC__) || defined(__GNUG__)
class __attribute__((visibility("default"))) GlobalInterpreterLock {
#else
class GlobalInterpreterLock {
#endif
 public:
  GlobalInterpreterLock();
  ~GlobalInterpreterLock();

 private:
  PyGILState_STATE gil_state_;
};

class Interpreter {
  Interpreter();
  ~Interpreter();

 public:
  static Interpreter* getInterpreter();

  Interpreter(const Interpreter& other) = delete;
  Interpreter(Interpreter&& other) = delete;
  Interpreter& operator=(const Interpreter& other) = delete;
  Interpreter& operator=(Interpreter&& other) = delete;

 public:
  PyThreadState* saved_thread_state_ = nullptr;
};


#if defined(__GNUC__) || defined(__GNUG__)
class __attribute__((visibility("default"))) PythonScriptEngine {
#else
class PythonScriptEngine {
#endif
 public:
  PythonScriptEngine();
  ~PythonScriptEngine();

  PythonScriptEngine(const PythonScriptEngine& other) = delete;
  PythonScriptEngine(PythonScriptEngine&& other) = delete;
  PythonScriptEngine& operator=(const PythonScriptEngine& other) = delete;
  PythonScriptEngine& operator=(PythonScriptEngine&& other) = delete;

  static void initialize() {}

  void eval(const std::string& script);
  void evalFile(const std::filesystem::path& file_name);

  void appendModulePaths(const std::vector<std::filesystem::path>& module_paths) {
    module_paths_.insert(module_paths_.end(), module_paths.begin(), module_paths.end());
  }

  template<typename... Args>
  void call(std::string_view fn_name, Args&& ...args) {
    GlobalInterpreterLock gil_lock;
    try {
      if (auto item = bindings_[fn_name]) {
        auto result = BorrowedCallable(*item)(std::forward<Args>(args)...);
        if (!result) {
          throw PyException();
        }
      }
    } catch (const std::exception& e) {
      throw PythonScriptException(e.what());
    }
  }

  template<typename ... Args>
  void callRequiredFunction(const std::string& fn_name, Args&& ...args) {
    GlobalInterpreterLock gil_lock;
    if (auto item = bindings_[fn_name]) {
      auto result = BorrowedCallable(*item)(std::forward<Args>(args)...);
      if (!result) {
        throw PyException();
      }
    } else {
      throw std::runtime_error("Required Function '" + fn_name + "' is not found within Python bindings");
    }
  }

  template<typename ... Args>
  void callProcessorObjectMethod(const std::string& fn_name, Args&& ...args) {
    GlobalInterpreterLock gil_lock;
    if (processor_instance_.get() == nullptr) {
      throw std::runtime_error("No python processor instance is set!");
    }

    try {
      auto callable_method = OwnedCallable(PyObject_GetAttrString(processor_instance_.get(), fn_name.c_str()));
      if (callable_method.get() == nullptr) {
        return;
      }

      auto result = callable_method(std::forward<Args>(args)...);
      if (!result) {
        throw PyException();
      }
    } catch (const std::exception& e) {
      throw PythonScriptException(e.what());
    }
  }

  template<typename ... Args>
  void callRequiredProcessorObjectMethod(const std::string& fn_name, Args&& ...args) {
    GlobalInterpreterLock gil_lock;
    if (processor_instance_.get() == nullptr) {
      throw std::runtime_error("No python processor instance is set!");
    }

    auto callable_method = OwnedCallable(PyObject_GetAttrString(processor_instance_.get(), fn_name.c_str()));
    if (callable_method.get() == nullptr) {
      throw std::runtime_error("Required method '" + fn_name + "' is not found in python processor object!");
    }

    auto result = callable_method(std::forward<Args>(args)...);
    if (!result) {
      throw PyException();
    }
  }

  template<object::convertible T>
  void bind(const std::string& name, const T& value) {
    GlobalInterpreterLock gil_lock;
    bindings_.put(name, value);
  }

  void onInitialize(core::Processor* proc);
  void describe(core::Processor* proc);
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context);
  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session);
  void initialize(const core::Relationship& success, const core::Relationship& failure, const core::Relationship& original, const std::shared_ptr<core::logging::Logger>& logger);
  void initializeProcessorObject(const std::string& python_class_name);
 private:
  void evalInternal(std::string_view script);

  void evaluateModuleImports();
  OwnedDict bindings_;
  OwnedObject processor_instance_;
  std::optional<std::string> processor_class_name_;
  std::vector<std::filesystem::path> module_paths_;
};

}  // namespace org::apache::nifi::minifi::extensions::python

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC visibility pop
#endif
