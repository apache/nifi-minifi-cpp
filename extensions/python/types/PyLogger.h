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

#include <memory>

#include "../PythonBindings.h"
#include "core/logging/Logger.h"

namespace org::apache::nifi::minifi::extensions::python {

struct PyLogger {
  PyLogger() {}
  using HeldType = std::weak_ptr<minifi::core::logging::Logger>;
  static constexpr const char* HeldTypeName = "PyLogger::HeldType";

  PyObject_HEAD
  HeldType logger_;

  static int init(PyLogger* self, PyObject* args, PyObject* kwds);

  static PyObject* error(PyLogger* self, PyObject* args);
  static PyObject* warn(PyLogger* self, PyObject* args);
  static PyObject* info(PyLogger* self, PyObject* args);
  static PyObject* debug(PyLogger* self, PyObject* args);
  static PyObject* trace(PyLogger* self, PyObject* args);

  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyLogger::HeldType> : public HolderTypeConverter<PyLogger> {};
}  // namespace object
}  // namespace org::apache::nifi::minifi::extensions::python
