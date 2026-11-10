// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "../PythonBindings.h"
#include "core/Relationship.h"

namespace org::apache::nifi::minifi::extensions::python {

struct PyRelationship {
  PyRelationship() {}

  using HeldType = core::Relationship;
  static constexpr const char* HeldTypeName = "PyRelationship::HeldType";

  PyObject_HEAD
  HeldType relationship_;

  static int init(PyRelationship* self, PyObject* args, PyObject* kwds);

  static PyObject* getName(PyRelationship* self, PyObject* args);
  static PyObject* getDescription(PyRelationship* self, PyObject* args);

  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyRelationship::HeldType> : public HolderTypeConverter<PyRelationship> {};
}
}  // namespace org::apache::nifi::minifi::extensions::python
