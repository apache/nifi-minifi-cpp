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
#include "PythonProcessor.h"

namespace org::apache::nifi::minifi::extensions::python {

struct PyProcessor {
  PyProcessor() {}
  using HeldType = std::weak_ptr<PythonProcessor>;
  static constexpr const char* HeldTypeName = "PythonProcessor::HeldType";

  PyObject_HEAD
  HeldType processor_;

  static int init(PyProcessor* self, PyObject* args, PyObject* kwds);

  static PyObject* setSupportsDynamicProperties(PyProcessor* self, PyObject* args);
  static PyObject* setDescription(PyProcessor* self, PyObject* args);
  static PyObject* addProperty(PyProcessor* self, PyObject* args);

  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyProcessor::HeldType> : public HolderTypeConverter<PyProcessor> {};
}  // namespace object
}  //  namespace org::apache::nifi::minifi::extensions::python
