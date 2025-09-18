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
#include "minifi-cpp/core/FlowFile.h"

namespace org::apache::nifi::minifi::extensions::python {

struct PyScriptFlowFile {
  PyScriptFlowFile() {}
  using HeldType = std::weak_ptr<core::FlowFile>;
  static constexpr const char* HeldTypeName = "PyScriptFlowFile::HeldType";

  PyObject_HEAD
  HeldType script_flow_file_;

  static int init(PyScriptFlowFile* self, PyObject* args, PyObject* kwds);

  static PyObject* getAttribute(PyScriptFlowFile* self, PyObject* args);
  static PyObject* addAttribute(PyScriptFlowFile* self, PyObject* args);
  static PyObject* updateAttribute(PyScriptFlowFile* self, PyObject* args);
  static PyObject* removeAttribute(PyScriptFlowFile* self, PyObject* args);
  static PyObject* setAttribute(PyScriptFlowFile* self, PyObject* args);
  static PyObject* getSize(PyScriptFlowFile* self, PyObject* args);
  static PyObject* getAttributes(PyScriptFlowFile* self, PyObject* args);

  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyScriptFlowFile::HeldType> : public HolderTypeConverter<PyScriptFlowFile> {};
}  // namespace object
}  // namespace org::apache::nifi::minifi::extensions::python
