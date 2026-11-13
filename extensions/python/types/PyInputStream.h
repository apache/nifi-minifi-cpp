// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once
#include <memory>

#include "../PythonBindings.h"
#include "minifi-cpp/io/InputStream.h"

namespace org::apache::nifi::minifi::extensions::python {

struct PyInputStream {
  PyInputStream() {}
  using HeldType = std::weak_ptr<io::InputStream>;
  static constexpr const char* HeldTypeName = "PyInputStream::HeldType";

  PyObject_HEAD
  HeldType input_stream_;

  static int init(PyInputStream* self, PyObject* args, PyObject* kwds);
  static PyObject* read(PyInputStream* self, PyObject* args);
  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyInputStream::HeldType> : public HolderTypeConverter<PyInputStream> {};
}
}  // namespace org::apache::nifi::minifi::extensions::python
