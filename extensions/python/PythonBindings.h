// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "types/Types.h"

namespace org::apache::nifi::minifi::extensions::python {

PyMODINIT_FUNC
PyInit_minifi_native(void);

template<auto Fn>
PyObject* safePyFunction(PyObject* self, PyObject* args) {
  try {
    return ((PyCFunction)Fn)(self, args);
  } catch (const std::exception& e) { \
    PyErr_Format(PyExc_RuntimeError, "C++ exception: %s", e.what()); \
    return nullptr; \
  } catch (...) { \
    PyErr_SetString(PyExc_Exception, "Unknown C++ exception"); \
    return nullptr; \
  }
}

}  // namespace org::apache::nifi::minifi::extensions::python
