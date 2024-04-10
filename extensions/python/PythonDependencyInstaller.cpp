/**
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
#include "PythonDependencyInstaller.h"

#include "PythonScriptException.h"
#include "PythonInterpreter.h"
#include "PyException.h"
#include "types/Types.h"

namespace org::apache::nifi::minifi::extensions::python {

namespace {

std::string getPythonBinary(const std::shared_ptr<Configure> &configuration) {
#if WIN32
  std::string python_binary_ = "python";
#else
  std::string python_binary_ = "python3";
#endif
  if (auto binary = configuration->get(minifi::Configuration::nifi_python_env_setup_binary)) {
    python_binary_ = *binary;
  }
  return python_binary_;
}

// On Windows when calling a system command using std::system, the whole command needs to be encapsulated in additional quotes,
// due to the std::system passing the command to 'cmd.exe /C' which needs the additional quotes to handle the command as a single argument
std::string encapsulateCommandInQuotesIfNeeded(const std::string& command) {
#if WIN32
    return "\"" + command + "\"";
#else
    return command;
#endif
}

}  // namespace

PythonDependencyInstaller::PythonDependencyInstaller(const std::shared_ptr<Configure> &configuration) {
  python_binary_ = getPythonBinary(configuration);
  std::string automatic_install_str;
  install_python_packages_automatically_ =
    configuration->get(Configuration::nifi_python_install_packages_automatically, automatic_install_str) && utils::string::toBool(automatic_install_str).value_or(false);
  if (auto path = configuration->get(minifi::Configuration::nifi_python_virtualenv_directory)) {
    virtualenv_path_ = *path;
    logger_->log_debug("Python virtualenv path was specified at: {}", virtualenv_path_.string());
  } else {
    logger_->log_debug("No valid python virtualenv path was specified");
  }
  if (auto python_processor_dir = configuration->get(minifi::Configuration::nifi_python_processor_dir)) {
    python_processor_dir_ = *python_processor_dir;
    logger_->log_debug("Python processor dir was specified at: {}", python_processor_dir_.string());
  } else {
    logger_->log_debug("No valid python processor dir was not specified in properties");
  }
  createVirtualEnvIfSpecified();
  addVirtualenvToPath();
}

std::vector<std::filesystem::path> PythonDependencyInstaller::getRequirementsFilePaths() const {
  if (!std::filesystem::exists(python_processor_dir_)) {
    return {};
  }
  std::vector<std::filesystem::path> paths;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path{python_processor_dir_})) {
    if (std::filesystem::is_regular_file(entry.path()) && entry.path().filename() == "requirements.txt") {
      paths.push_back(entry.path());
    }
  }
  return paths;
}

void PythonDependencyInstaller::createVirtualEnvIfSpecified() const {
  if (virtualenv_path_.empty()) {
    return;
  }
  if (!std::filesystem::exists(virtualenv_path_) || std::filesystem::is_empty(virtualenv_path_)) {
    logger_->log_info("Creating python virtual env at: {}", virtualenv_path_.string());
    auto venv_command = "\"" + python_binary_ + "\" -m venv \"" + virtualenv_path_.string() + "\"";
    auto return_value = std::system(encapsulateCommandInQuotesIfNeeded(venv_command).c_str());
    if (return_value != 0) {
      throw PythonScriptException(fmt::format("The following command creating python virtual env failed: '{}'", venv_command));
    }
  }
}

void PythonDependencyInstaller::installDependenciesFromRequirementsFiles() const {
  if (!isPackageInstallationNeeded()) {
    return;
  }
  auto requirement_file_paths = getRequirementsFilePaths();
  for (const auto& requirements_file_path : requirement_file_paths) {
    logger_->log_info("Installing python packages from the following requirements.txt file: {}", requirements_file_path.string());
    std::string pip_command;
#if WIN32
    pip_command.append("\"").append((virtualenv_path_ / "Scripts" / "activate.bat").string()).append("\" && ");
#else
    pip_command.append(". \"").append((virtualenv_path_ / "bin" / "activate").string()).append("\" && ");
#endif
    pip_command.append("\"").append(python_binary_).append("\" -m pip install --no-cache-dir -r \"").append(requirements_file_path.string()).append("\"");
    auto return_value = std::system(encapsulateCommandInQuotesIfNeeded(pip_command).c_str());
    if (return_value != 0) {
      throw PythonScriptException(fmt::format("The following command to install python packages failed: '{}'", pip_command));
    }
  }
}

void PythonDependencyInstaller::evalScript(std::string_view script) {
  GlobalInterpreterLock gil;
  const auto script_file = minifi::utils::string::join_pack("# -*- coding: utf-8 -*-\n", script);
  auto compiled_string = OwnedObject(Py_CompileString(script_file.c_str(), "<string>", Py_file_input));
  if (!compiled_string.get()) {
    throw PyException();
  }

  OwnedDict bindings = OwnedDict::create();
  const auto result = OwnedObject(PyEval_EvalCode(compiled_string.get(), bindings.get(), bindings.get()));
  if (!result.get()) {
    throw PyException();
  }
}

void PythonDependencyInstaller::addVirtualenvToPath() const {
  if (virtualenv_path_.empty()) {
    return;
  }
  Interpreter::getInterpreter();
  if (!virtualenv_path_.empty()) {
#if WIN32
    std::filesystem::path site_package_path = virtualenv_path_ / "Lib" / "site-packages";
#else
    std::string python_dir_name;
    auto lib_path = virtualenv_path_ / "lib";
    for (auto const& dir_entry : std::filesystem::directory_iterator{lib_path}) {
      if (minifi::utils::string::startsWith(dir_entry.path().filename().string(), "python")) {
        python_dir_name = dir_entry.path().filename().string();
        break;
      }
    }
    if (python_dir_name.empty()) {
      throw PythonScriptException("Could not find python directory under virtualenv lib dir: " + lib_path.string());
    }
    std::filesystem::path site_package_path = virtualenv_path_ / "lib" / python_dir_name / "site-packages";
#endif
    if (!std::filesystem::exists(site_package_path)) {
      throw PythonScriptException("Could not find python site package path: " + site_package_path.string());
    }
    evalScript("import sys\nsys.path.append(r'" + site_package_path.string() + "')");
  }
}

}  // namespace org::apache::nifi::minifi::extensions::python
