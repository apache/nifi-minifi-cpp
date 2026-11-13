// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "PyRelationship.h"
#include "PyScriptFlowFile.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyRelationship_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"getName", safePyFunction<PyRelationship::getName>, METH_VARARGS, nullptr},
    {"getDescription", safePyFunction<PyRelationship::getDescription>, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyRelationshipTypeSpecSlots[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PyRelationship>)},
    {Py_tp_init, reinterpret_cast<void*>(PyRelationship::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyRelationship_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PyRelationship>)},
    {}  /* Sentinel */
};

static PyType_Spec PyRelationshipTypeSpec{
    .name = "minifi_native.Relationship",
    .basicsize = sizeof(PyRelationship),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyRelationshipTypeSpecSlots
};

int PyRelationship::init(PyRelationship* self, PyObject* args, PyObject*) {
  PyObject* capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &capsule)) {
    return -1;
  }

  auto relationship = PyCapsule_GetPointer(capsule, HeldTypeName);
  if (!relationship)
    return -1;
  self->relationship_ = *static_cast<HeldType*>(relationship);
  return 0;
}

PyObject* PyRelationship::getName(PyRelationship* self, PyObject*) {
  return object::returnReference(self->relationship_.getName());
}

PyObject* PyRelationship::getDescription(PyRelationship* self, PyObject*) {
  return object::returnReference(self->relationship_.getDescription());
}

PyTypeObject* PyRelationship::typeObject() {
  static OwnedObject PyRelationshipType{PyType_FromSpec(&PyRelationshipTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyRelationshipType.get());
}

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
