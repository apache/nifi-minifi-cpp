// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
