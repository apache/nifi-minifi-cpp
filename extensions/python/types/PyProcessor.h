// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
  static PyObject* setSingleThreaded(PyProcessor* self, PyObject* args);
  static PyObject* setDescription(PyProcessor* self, PyObject* args);
  static PyObject* setVersion(PyProcessor* self, PyObject* args);
  static PyObject* addProperty(PyProcessor* self, PyObject* args);

  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyProcessor::HeldType> : public HolderTypeConverter<PyProcessor> {};
}  // namespace object
}  //  namespace org::apache::nifi::minifi::extensions::python
