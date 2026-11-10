// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
