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

#include "PythonScriptEngine.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::python {

Interpreter *getInterpreter() {
  static Interpreter interpreter;
  return &interpreter;
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
    if (std::filesystem::is_regular_file(std::filesystem::status(module_path))) {
      std::string path;
      std::string filename;
      utils::file::getFileNameAndPath(module_path, path, filename);
      py::eval<py::eval_statements>("sys.path.append('" + path + "')", *bindings_, *bindings_);
    } else {
      py::eval<py::eval_statements>("sys.path.append('" + module_path + "')", *bindings_, *bindings_);
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
  getInterpreter();
}

}  // namespace org::apache::nifi::minifi::python
