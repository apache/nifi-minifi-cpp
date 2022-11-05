#include "PyRelationship.h"
#include "PyScriptFlowFile.h"

extern "C" {
namespace org::apache::nifi::minifi::python {

static PyMethodDef PyRelationship_methods[] = {
  { "getName", (PyCFunction) PyRelationship::getName, METH_VARARGS, nullptr, },
  { "getDescription", (PyCFunction) PyRelationship::getDescription, METH_VARARGS, nullptr, },
  { nullptr } /* Sentinel */
};

static PyTypeObject PyRelationshipType = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  .tp_name = "minifi_native.Relationship",
  .tp_doc = nullptr,
  .tp_basicsize = sizeof(PyRelationship),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_new = PyRelationship::newInstance,
  .tp_init = reinterpret_cast<initproc>(PyRelationship::init),
  .tp_dealloc = reinterpret_cast<destructor>(PyRelationship::dealloc),
  .tp_methods = PyRelationship_methods
};

PyObject *PyRelationship::newInstance(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  auto self = reinterpret_cast<PyScriptFlowFile*>(type->tp_alloc(type, 0));
  if (self == nullptr) {
    return nullptr;
  }

  return reinterpret_cast<PyObject*>(self);
}

int PyRelationship::init(PyRelationship *self, PyObject *args, PyObject *kwds) {
  PyObject *capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &capsule)) {
    return -1;
  }

  auto relationship = static_cast<HeldType*>(PyCapsule_GetPointer(capsule, nullptr));
  // Py_DECREF(capsule);
  self->relationship_ = *relationship;
  return 0;
}

void PyRelationship::dealloc(PyRelationship *self) {

}

PyObject *PyRelationship::getName(PyRelationship *self, PyObject *args) {
  return object::returnReference(self->relationship_.getName());
}

PyObject *PyRelationship::getDescription(PyRelationship *self, PyObject *args) {
  return object::returnReference(self->relationship_.getDescription());
}

PyTypeObject *PyRelationship::typeObject() {
  return &PyRelationshipType;
}

}  // namespace org::apache::nifi::minifi::python
}  // extern "C"