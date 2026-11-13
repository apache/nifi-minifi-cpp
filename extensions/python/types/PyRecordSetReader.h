// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "minifi-cpp/controllers/RecordSetReader.h"
#include "../PythonBindings.h"

namespace org::apache::nifi::minifi::extensions::python {

struct PyRecordSetReader {
  PyRecordSetReader() {}
  using HeldType = std::weak_ptr<core::RecordSetReader>;
  static constexpr const char* HeldTypeName = "PyRecordSetReader::HeldType";

  PyObject_HEAD
  HeldType record_set_reader_;

  static int init(PyRecordSetReader* self, PyObject* args, PyObject* kwds);

  static PyObject* read(PyRecordSetReader* self, PyObject* args);

  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyRecordSetReader::HeldType> : public HolderTypeConverter<PyRecordSetReader> {};
}  // namespace object
}  // namespace org::apache::nifi::minifi::extensions::python
