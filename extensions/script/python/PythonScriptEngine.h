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

#ifndef NIFI_MINIFI_CPP_PYTHONSCRIPTENGINE_H
#define NIFI_MINIFI_CPP_PYTHONSCRIPTENGINE_H

#include <mutex>
#include <pybind11/embed.h>
#include <core/ProcessSession.h>

#include "../ScriptEngine.h"
#include "../ScriptProcessContext.h"

#include "PyProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace python {

namespace py = pybind11;

class __attribute__((visibility("default"))) PythonScriptEngine : public script::ScriptEngine {
 public:
  PythonScriptEngine();
  ~PythonScriptEngine() {
    py::gil_scoped_acquire gil{};
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
  template<typename... Args>
  void call(const std::string &fn_name, Args &&...args) {
    py::gil_scoped_acquire gil{};
    (*bindings_)[fn_name.c_str()](convert(args)...);
  }

  /**
   * Binds an object into the scope of the python interpreter.
   * @tparam T
   * @param name
   * @param value
   */
  template<typename T>
  void bind(const std::string &name, const T &value) {
    py::gil_scoped_acquire gil{};
    (*bindings_)[name.c_str()] = convert(value);
  }

  template<typename T>
  py::object convert(const T &value) {
    py::gil_scoped_acquire gil{};
    return py::cast(value);
  }

  py::object convert(const std::shared_ptr<core::ProcessSession> &session) {
    return py::cast(std::make_shared<PyProcessSession>(session));
  }

  py::object convert(const std::shared_ptr<core::ProcessContext> &context) {
    return py::cast(std::make_shared<script::ScriptProcessContext>(context));
  }

 private:
  static std::unique_ptr<py::scoped_interpreter> guard_;
  static std::unique_ptr<py::gil_scoped_release> gil_release_;

  static std::mutex init_mutex_;
  static bool initialized_;
  std::unique_ptr<py::dict> bindings_;
};

} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_PYTHONSCRIPTENGINE_H
