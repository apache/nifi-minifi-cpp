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

#include "minifi-cpp/core/ProcessContext.h"
#include "PySSLContextService.h"
#include "PyRecordSetReader.h"
#include "PyRecordSetWriter.h"
#include "../PythonBindings.h"

namespace org::apache::nifi::minifi::extensions::python {

struct PyProcessContext {
  PyProcessContext() {}
  using HeldType = core::ProcessContext*;
  static constexpr const char* HeldTypeName = "PyProcessContext::HeldType";

  PyObject_HEAD
  HeldType process_context_;

  static int init(PyProcessContext* self, PyObject* args, PyObject* kwds);

  static PyObject* getProperty(PyProcessContext* self, PyObject* args);
  static PyObject* getRawProperty(PyProcessContext* self, PyObject* args);
  static PyObject* getDynamicProperty(PyProcessContext* self, PyObject* args);
  static PyObject* getRawDynamicProperty(PyProcessContext* self, PyObject* args);
  static PyObject* getDynamicPropertyKeys(PyProcessContext* self, PyObject* args);
  static PyObject* getStateManager(PyProcessContext* self, PyObject* args);
  static PyObject* getControllerService(PyProcessContext* self, PyObject* args);
  static PyObject* getName(PyProcessContext* self, PyObject* args);
  static PyObject* getProperties(PyProcessContext* self, PyObject* args);
  static PyObject* yieldResources(PyProcessContext* self, PyObject* args);

  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyProcessContext::HeldType> : public HolderTypeConverter<PyProcessContext> {};
}  // namespace object
}  // namespace org::apache::nifi::minifi::extensions::python
