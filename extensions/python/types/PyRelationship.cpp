/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "PyRelationship.h"
#include "PyScriptFlowFile.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyRelationship_methods[] = {
    {"getName", (PyCFunction) PyRelationship::getName, METH_VARARGS, nullptr},
    {"getDescription", (PyCFunction) PyRelationship::getDescription, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyRelationshipTypeSpecSlots[] = {
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
    throw PyException();
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
