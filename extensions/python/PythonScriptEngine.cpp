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
#include "PythonBindings.h"
#include "PythonConfigState.h"
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

std::string encapsulateCommandInQuotesIfNeeded(const std::string& command) {
#if WIN32
    return "\"" + command + "\"";
#else
    return command;
#endif
}

std::vector<std::filesystem::path> getRequirementsFilePaths(const std::shared_ptr<Configure> &configuration) {
  std::vector<std::filesystem::path> paths;
  if (auto python_processor_path = configuration->get(minifi::Configuration::nifi_python_processor_dir)) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path{*python_processor_path})) {
      if (std::filesystem::is_regular_file(entry.path()) && entry.path().filename() == "requirements.txt") {
        paths.push_back(entry.path());
      }
    }
  }
  return paths;
}

std::string getPythonBinary(const std::shared_ptr<Configure> &configuration) {
#if WIN32
  std::string python_binary = "python";
#else
  std::string python_binary = "python3";
#endif
  if (auto binary = configuration->get(minifi::Configuration::nifi_python_env_setup_binary)) {
    python_binary = *binary;
  }
  return python_binary;
}

void createVirtualEnvIfSpecified(const std::shared_ptr<Configure> &configuration) {
  if (auto path = configuration->get(minifi::Configuration::nifi_python_virtualenv_directory)) {
    PythonConfigState::getInstance().virtualenv_path = *path;
    if (!std::filesystem::exists(PythonConfigState::getInstance().virtualenv_path) || !std::filesystem::is_empty(PythonConfigState::getInstance().virtualenv_path)) {
      auto venv_command = "\"" + PythonConfigState::getInstance().python_binary + "\" -m venv \"" + PythonConfigState::getInstance().virtualenv_path.string() + "\"";
      auto return_value = std::system(encapsulateCommandInQuotesIfNeeded(venv_command).c_str());
      if (return_value != 0) {
        throw PythonScriptException(fmt::format("The following command creating python virtual env failed: '{}'", venv_command));
      }
    }
  }
}

void installPythonPackagesIfRequested(const std::shared_ptr<Configure> &configuration, const std::shared_ptr<core::logging::Logger>& logger) {
  std::string automatic_install_str;
  if (!PythonConfigState::getInstance().isPackageInstallationNeeded()) {
    return;
  }
  auto requirement_file_paths = getRequirementsFilePaths(configuration);
  for (const auto& requirements_file_path : requirement_file_paths) {
    logger->log_info("Installing python packages from the following requirements.txt file: {}", requirements_file_path.string());
    std::string pip_command;
#if WIN32
    pip_command.append("\"").append((PythonConfigState::getInstance().virtualenv_path / "Scripts" / "activate.bat").string()).append("\" && ");
#else
    pip_command.append(". \"").append((PythonConfigState::getInstance().virtualenv_path / "bin" / "activate").string()).append("\" && ");
#endif
    pip_command.append("\"").append(PythonConfigState::getInstance().python_binary).append("\" -m pip install --no-cache-dir -r \"").append(requirements_file_path.string()).append("\"");
    auto return_value = std::system(encapsulateCommandInQuotesIfNeeded(pip_command).c_str());
    if (return_value != 0) {
      throw PythonScriptException(fmt::format("The following command to install python packages failed: '{}'", pip_command));
    }
  }
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

void PythonScriptEngine::initialize(const std::shared_ptr<Configure> &configuration, const std::shared_ptr<core::logging::Logger>& logger) {
  PythonConfigState::getInstance().python_binary = getPythonBinary(configuration);
  std::string automatic_install_str;
  PythonConfigState::getInstance().install_python_packages_automatically =
    configuration->get(Configuration::nifi_python_install_packages_automatically, automatic_install_str) && utils::string::toBool(automatic_install_str).value_or(false);
  createVirtualEnvIfSpecified(configuration);
  installPythonPackagesIfRequested(configuration, logger);
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

void PythonScriptEngine::onSchedule(const std::shared_ptr<core::ProcessContext> &context) {
  if (processor_instance_.get() != nullptr) {
    callProcessorObjectMethod("onSchedule", std::weak_ptr(context));
  } else {
    call("onSchedule", std::weak_ptr(context));
  }
}

void PythonScriptEngine::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  auto py_session = std::make_shared<python::PyProcessSession>(session);
  if (processor_instance_.get() != nullptr) {
    callProcessorObjectMethod("onTrigger", std::weak_ptr(context), std::weak_ptr(py_session));
  } else {
    call("onTrigger", std::weak_ptr(context), std::weak_ptr(py_session));
  }
}

void PythonScriptEngine::initialize(const core::Relationship& success, const core::Relationship& failure, const core::Relationship& original, const std::shared_ptr<core::logging::Logger>& logger) {
  bind("log", std::weak_ptr<core::logging::Logger>(logger));
  bind("REL_SUCCESS", success);
  bind("REL_FAILURE", failure);
  bind("REL_ORIGINAL", original);
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
  evalInternal("import sys");
  if (!PythonConfigState::getInstance().virtualenv_path.empty()) {
#if WIN32
    std::filesystem::path site_package_path = PythonConfigState::getInstance().virtualenv_path / "Lib" / "site-packages";
#else
    std::string python_dir_name;
    auto lib_path = PythonConfigState::getInstance().virtualenv_path / "lib";
    for (auto const& dir_entry : std::filesystem::directory_iterator{lib_path}) {
      if (minifi::utils::string::startsWith(dir_entry.path().filename().string(), "python")) {
        python_dir_name = dir_entry.path().filename().string();
        break;
      }
    }
    if (python_dir_name.empty()) {
      throw PythonScriptException("Could not find python directory under virtualenv lib dir: " + lib_path.string());
    }
    std::filesystem::path site_package_path = PythonConfigState::getInstance().virtualenv_path / "lib" / python_dir_name / "site-packages";
#endif
    if (!std::filesystem::exists(site_package_path)) {
      throw PythonScriptException("Could not find python site package path: " + site_package_path.string());
    }
    evalInternal("sys.path.append(r'" + site_package_path.string() + "')");
  }
  if (module_paths_.empty()) {
    return;
  }

  for (const auto& module_path : module_paths_) {
    if (std::filesystem::is_regular_file(module_path)) {
      evalInternal("sys.path.append(r'" + module_path.parent_path().string() + "')");
    } else {
      evalInternal("sys.path.append(r'" + module_path.string() + "')");
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

}  // namespace org::apache::nifi::minifi::extensions::python
