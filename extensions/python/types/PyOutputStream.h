// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>

#include "../PythonBindings.h"
#include "minifi-cpp/io/OutputStream.h"

namespace org::apache::nifi::minifi::extensions::python {

struct PyOutputStream {
  PyOutputStream() {}
  using HeldType = std::weak_ptr<org::apache::nifi::minifi::io::OutputStream>;
  static constexpr const char* HeldTypeName = "PyOutputStream::HeldType";

  PyObject_HEAD
  HeldType output_stream_;

  static int init(PyOutputStream* self, PyObject* args, PyObject* kwds);

  static PyObject* write(PyOutputStream* self, PyObject* args);

  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyOutputStream::HeldType> : public HolderTypeConverter<PyOutputStream> {};
}  // namespace object
}  // namespace org::apache::nifi::minifi::extensions::python
