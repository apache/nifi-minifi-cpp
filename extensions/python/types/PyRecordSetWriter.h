// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "minifi-cpp/controllers/RecordSetWriter.h"
#include "../PythonBindings.h"

namespace org::apache::nifi::minifi::extensions::python {

struct PyRecordSetWriter {
  PyRecordSetWriter() {}
  using HeldType = std::weak_ptr<core::RecordSetWriter>;
  static constexpr const char* HeldTypeName = "PyRecordSetWriter::HeldType";

  PyObject_HEAD
  HeldType record_set_writer_;

  static int init(PyRecordSetWriter* self, PyObject* args, PyObject* kwds);

  static PyObject* write(PyRecordSetWriter* self, PyObject* args);

  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyRecordSetWriter::HeldType> : public HolderTypeConverter<PyRecordSetWriter> {};
}  // namespace object
}  // namespace org::apache::nifi::minifi::extensions::python
