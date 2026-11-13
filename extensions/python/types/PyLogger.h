// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "../PythonBindings.h"
#include "minifi-cpp/core/logging/Logger.h"

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
