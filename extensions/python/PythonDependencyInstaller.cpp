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
  std::string python_binary = "python";
#else
  std::string python_binary = "python3";
#endif
  if (auto binary = configuration->get(minifi::Configuration::nifi_python_env_setup_binary)) {
    python_binary = *binary;
  }
  return python_binary;
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
  install_python_packages_automatically_ = (configuration->get(Configuration::nifi_python_install_packages_automatically) | utils::andThen(&utils::string::toBool)).value_or(false);
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
    if (install_python_packages_automatically_) {
      logger_->log_warn("Python virtualenv path was not specified, but automatic python dependency installation was requested. "
                        "Specify python virtualenv path in properties to enable automatic python dependency installation.");
    }
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

void PythonDependencyInstaller::runInstallCommandInVirtualenv(const std::string& install_command) const {
  std::string command_with_virtualenv;
#if WIN32
  command_with_virtualenv.append("\"").append((virtualenv_path_ / "Scripts" / "activate.bat").string()).append("\" && ");
#else
  command_with_virtualenv.append(". \"").append((virtualenv_path_ / "bin" / "activate").string()).append("\" && ");
#endif
  command_with_virtualenv.append(install_command);
  auto return_value = std::system(encapsulateCommandInQuotesIfNeeded(command_with_virtualenv).c_str());
  if (return_value != 0) {
    throw PythonScriptException(fmt::format("The following command to install python packages failed: '{}'", command_with_virtualenv));
  }
}

void PythonDependencyInstaller::installDependenciesFromRequirementsFiles() const {
  if (!isPackageInstallationNeeded()) {
    return;
  }
  auto requirement_file_paths = getRequirementsFilePaths();
  for (const auto& requirements_file_path : requirement_file_paths) {
    logger_->log_info("Installing python packages from the following requirements.txt file: {}", requirements_file_path.string());
    // --no-cache-dir is used to be in line with NiFi's dependency install behavior
    auto install_command = std::string("\"").append(python_binary_).append("\" -m pip install --no-cache-dir -r \"").append(requirements_file_path.string()).append("\"");
    runInstallCommandInVirtualenv(install_command);
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
  bindings.put("__builtins__", OwnedObject(PyImport_ImportModule("builtins")));
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

void PythonDependencyInstaller::installInlinePythonDependencies(const std::filesystem::path& script_file_path) const {
  if (!isPackageInstallationNeeded()) {
    return;
  }
  auto dependency_installer_path = python_processor_dir_ / "nifi_python_processors" / "utils" / "inline_dependency_installer.py";
  if (python_processor_dir_.empty() || !std::filesystem::exists(dependency_installer_path) || !std::filesystem::exists(script_file_path)) {
    return;
  }
  logger_->log_info("Checking and installing inline defined Python dependencies of {}", script_file_path.string());
  auto install_command = std::string("\"").append(python_binary_).append("\" \"").append(dependency_installer_path.string())
    .append("\" \"").append(script_file_path.string()).append("\"");
  runInstallCommandInVirtualenv(install_command);
}

}  // namespace org::apache::nifi::minifi::extensions::python
