// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include "../PythonBindings.h"
#include "minifi-cpp/core/FlowFile.h"

namespace org::apache::nifi::minifi::extensions::python {

struct PyScriptFlowFile {
  PyScriptFlowFile() {}
  using HeldType = std::weak_ptr<core::FlowFile>;
  static constexpr const char* HeldTypeName = "PyScriptFlowFile::HeldType";

  PyObject_HEAD
  HeldType script_flow_file_;

  static int init(PyScriptFlowFile* self, PyObject* args, PyObject* kwds);

  static PyObject* getAttribute(PyScriptFlowFile* self, PyObject* args);
  static PyObject* addAttribute(PyScriptFlowFile* self, PyObject* args);
  static PyObject* updateAttribute(PyScriptFlowFile* self, PyObject* args);
  static PyObject* removeAttribute(PyScriptFlowFile* self, PyObject* args);
  static PyObject* setAttribute(PyScriptFlowFile* self, PyObject* args);
  static PyObject* getSize(PyScriptFlowFile* self, PyObject* args);
  static PyObject* getAttributes(PyScriptFlowFile* self, PyObject* args);

  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyScriptFlowFile::HeldType> : public HolderTypeConverter<PyScriptFlowFile> {};
}  // namespace object
}  // namespace org::apache::nifi::minifi::extensions::python
