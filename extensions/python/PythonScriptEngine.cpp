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

#include "PythonScriptEngine.h"
#include "PythonConfigState.h"
#include "types/PyProcessSession.h"
#include "types/PyProcessContext.h"
#include "types/PyProcessor.h"
#include "types/PyLogger.h"
#include "types/PyRelationship.h"

namespace org::apache::nifi::minifi::extensions::python {

PythonScriptEngine::PythonScriptEngine() {
  Interpreter::getInterpreter();

  GlobalInterpreterLock lock;
  bindings_ = OwnedDict::create();
}

PythonScriptEngine::~PythonScriptEngine() {
  GlobalInterpreterLock lock;
  bindings_.resetReference();
  processor_instance_.resetReference();
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
  if (processor_instance_.get() != nullptr) {
    callProcessorObjectMethod("onInitialize", std::weak_ptr(newproc));
  } else {
    call("onInitialize", std::weak_ptr(newproc));
  }
}

void PythonScriptEngine::describe(core::Processor* proc) {
  auto newproc = std::make_shared<python::PythonProcessor>(proc);
  if (processor_instance_.get() != nullptr) {
    callRequiredProcessorObjectMethod("describe", std::weak_ptr(newproc));
  } else {
    callRequiredFunction("describe", std::weak_ptr(newproc));
  }
}

void PythonScriptEngine::onSchedule(core::ProcessContext* context) {
  if (processor_instance_.get() != nullptr) {
    callProcessorObjectMethod("onSchedule", context);
  } else {
    call("onSchedule", context);
  }
}

void PythonScriptEngine::onTrigger(core::ProcessContext* context, core::ProcessSession* session) {
  auto py_session = std::make_shared<python::PyProcessSession>(gsl::make_not_null(session));
  if (processor_instance_.get() != nullptr) {
    callProcessorObjectMethod("onTrigger", context, std::weak_ptr(py_session));
  } else {
    call("onTrigger", context, std::weak_ptr(py_session));
  }
}

void PythonScriptEngine::initialize(const core::Relationship& success, const core::Relationship& failure, const core::Relationship& original, const std::shared_ptr<core::logging::Logger>& logger) {
  bind("log", std::weak_ptr<core::logging::Logger>(logger));
  bind("REL_SUCCESS", success);
  bind("REL_FAILURE", failure);
  bind("REL_ORIGINAL", original);
}

void PythonScriptEngine::evalInternal(std::string_view script) {
  const auto script_file = minifi::utils::string::join_pack("# -*- coding: utf-8 -*-\n", script);
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
  evalInternal("import sys");
  if (module_paths_.empty()) {
    return;
  }

  for (const auto& module_path : module_paths_) {
    if (std::filesystem::is_regular_file(module_path)) {
      evalInternal("sys.path.insert(0, r'" + module_path.parent_path().string() + "')");
    } else {
      evalInternal("sys.path.insert(0, r'" + module_path.string() + "')");
    }
  }
}

void PythonScriptEngine::initializeProcessorObject(const std::string& python_class_name) {
  GlobalInterpreterLock gil;
  if (auto python_class = bindings_[python_class_name]) {
    auto num_args = [&]() -> size_t {
      auto class_init = OwnedObject(PyObject_GetAttrString(python_class->get(), "__init__"));
      if (!class_init.get()) {
        return 0;
      }

      auto inspect_module = OwnedObject(PyImport_ImportModule("inspect"));
      if (!inspect_module.get()) {
        return 0;
      }

      auto inspect_args = OwnedObject(PyObject_CallMethod(inspect_module.get(), "getfullargspec", "O", class_init.get()));
      if (!inspect_args.get()) {
        return 0;
      }

      auto arg_list = OwnedObject(PyObject_GetAttrString(inspect_args.get(), "args"));
      if (!arg_list.get()) {
        return 0;
      }

      return PyList_Size(arg_list.get());
    }();

    if (num_args > 1) {
      auto kwargs = OwnedDict::create();
      auto value = OwnedObject(Py_None);
      kwargs.put("jvm", value);
      auto args = OwnedObject(PyTuple_New(0));
      processor_instance_ = OwnedObject(PyObject_Call(python_class->get(), args.get(), kwargs.get()));
    } else {
      processor_instance_ = OwnedObject(PyObject_CallObject(python_class->get(), nullptr));
    }

    if (processor_instance_.get() == nullptr) {
      throw PythonScriptException(PyException().what());
    }

    auto result = PyObject_SetAttrString(processor_instance_.get(), "logger", bindings_["log"]->get());
    if (result < 0) {
      throw PythonScriptException("Could not bind 'logger' object to '" + python_class_name + "' python processor object");
    }
    result = PyObject_SetAttrString(processor_instance_.get(), "REL_SUCCESS", bindings_["REL_SUCCESS"]->get());
    if (result < 0) {
      throw PythonScriptException("Could not bind 'REL_SUCCESS' object to '" + python_class_name + "' python processor object");
    }
    result = PyObject_SetAttrString(processor_instance_.get(), "REL_FAILURE", bindings_["REL_FAILURE"]->get());
    if (result < 0) {
      throw PythonScriptException("Could not bind 'REL_FAILURE' object to '" + python_class_name + "' python processor object");
    }
    result = PyObject_SetAttrString(processor_instance_.get(), "REL_ORIGINAL", bindings_["REL_ORIGINAL"]->get());
    if (result < 0) {
      throw PythonScriptException("Could not bind 'REL_ORIGINAL' object to '" + python_class_name + "' python processor object");
    }
  } else {
    throw PythonScriptException("No python class '" + python_class_name + "' was found!");
  }
}

std::vector<core::Relationship> PythonScriptEngine::getCustomPythonRelationships() {
  GlobalInterpreterLock gil;
  std::vector<core::Relationship> relationships;
  if (processor_instance_.get() != nullptr) {
    auto python_list_of_relationships = OwnedList(callProcessorObjectMethod("getRelationships"));
    if (python_list_of_relationships.get() == Py_None) {
      return relationships;
    }
    try {
      for (size_t i = 0; i < python_list_of_relationships.length(); ++i) {
        if (PyObject_HasAttrString(python_list_of_relationships[i].get(), "name") == 0) {
          logger_->log_error("Error in python processor getRelationships method: Custom relationship object has no name attribute!");
          continue;
        }
        auto name = OwnedStr(PyObject_GetAttrString(python_list_of_relationships[i].get(), "name"));
        if (name.get() == nullptr) {
          logger_->log_error("Error in python processor getRelationships method: Custom relationship object's name attribute could not be read!");
          continue;
        }

        if (PyObject_HasAttrString(python_list_of_relationships[i].get(), "description") == 0) {
          logger_->log_error("Error in python processor getRelationships method: Custom relationship object has no description attribute!");
          continue;
        }
        auto description = OwnedStr(PyObject_GetAttrString(python_list_of_relationships[i].get(), "description"));
        if (description.get() == nullptr) {
          logger_->log_error("Error in python processor getRelationships method: Custom relationship object's description attribute could not be read!");
          continue;
        }

        relationships.push_back(core::Relationship(name.toUtf8String(), description.toUtf8String()));
      }
    } catch (const std::exception& e) {
      throw PythonScriptException(e.what());
    }
  }
  return relationships;
}

}  // namespace org::apache::nifi::minifi::extensions::python
