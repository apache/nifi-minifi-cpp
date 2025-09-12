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

#include "minifi-cpp/core/StateManager.h"
#include "../PythonBindings.h"

namespace org::apache::nifi::minifi::extensions::python {

struct PyStateManager {
  PyStateManager() {}
  using HeldType = core::StateManager*;
  static constexpr const char* HeldTypeName = "PyStateManager::HeldType";

  PyObject_HEAD
  HeldType state_manager_;

  static int init(PyStateManager* self, PyObject* args, PyObject* kwds);

  static PyObject* set(PyStateManager* self, PyObject* args);
  static PyObject* get(PyStateManager* self, PyObject* args);
  static PyObject* clear(PyStateManager* self, PyObject* args);
  static PyObject* replace(PyStateManager* self, PyObject* args);

  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyStateManager::HeldType> : public HolderTypeConverter<PyStateManager> {};
}  // namespace object
}  // namespace org::apache::nifi::minifi::extensions::python
