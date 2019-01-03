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

#include "PythonBindings.h"
#include "PythonScriptEngine.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace python {

std::unique_ptr<py::scoped_interpreter> PythonScriptEngine::guard_;
std::unique_ptr<py::gil_scoped_release> PythonScriptEngine::gil_release_;
std::mutex PythonScriptEngine::init_mutex_;
bool PythonScriptEngine::initialized_;

PythonScriptEngine::PythonScriptEngine() {
  {
    std::lock_guard<std::mutex> lock(init_mutex_);

    if (!initialized_) {
      throw std::runtime_error("Python is not yet initialized");
    }
  }

  {
    py::gil_scoped_acquire gil{};
    py::module::import("minifi_native");
    bindings_.reset(new py::dict());
    (*bindings_) = py::globals().attr("copy")();
  }
}

void PythonScriptEngine::eval(const std::string &script) {
  py::gil_scoped_acquire gil{};

  if (script[0] == '\n') {
    py::eval<py::eval_statements>(py::module::import("textwrap").attr("dedent")(script), *bindings_, *bindings_);
  } else {
     py::eval<py::eval_statements>(script, *bindings_, *bindings_);
  }
}

void PythonScriptEngine::evalFile(const std::string &file_name) {
  py::gil_scoped_acquire gil{};
  py::eval_file(file_name, *bindings_, *bindings_);
}

void PythonScriptEngine::initialize() {
  std::lock_guard<std::mutex> lock(init_mutex_);

  if (initialized_) {
    return;
  }

  initialized_ = true;
  guard_.reset(new py::scoped_interpreter(false));
  gil_release_.reset(new py::gil_scoped_release());
}

} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

