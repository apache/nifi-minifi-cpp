#include "PyScriptFlowFile.h"
#include "Exception.h"

extern "C" {
namespace org::apache::nifi::minifi::python {

static PyMethodDef PyScriptFlowFile_methods[] = {
  { "getAttribute", (PyCFunction) PyScriptFlowFile::getAttribute, METH_VARARGS, nullptr, },
  { "addAttribute", (PyCFunction) PyScriptFlowFile::addAttribute, METH_VARARGS, nullptr, },
  { "updateAttribute", (PyCFunction) PyScriptFlowFile::updateAttribute, METH_VARARGS, nullptr, },
  { "removeAttribute", (PyCFunction) PyScriptFlowFile::removeAttribute, METH_VARARGS, nullptr, },
  { "setAttribute", (PyCFunction) PyScriptFlowFile::setAttribute, METH_VARARGS, nullptr, },
  { nullptr } /* Sentinel */
};

static PyTypeObject PyScriptFlowFileType = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  .tp_name = "minifi_native.FlowFile",
  .tp_doc = nullptr,
  .tp_basicsize = sizeof(PyScriptFlowFile),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_new = PyScriptFlowFile::newInstance,
  .tp_init = reinterpret_cast<initproc>(PyScriptFlowFile::init),
  .tp_dealloc = reinterpret_cast<destructor>(PyScriptFlowFile::dealloc),
  .tp_methods = PyScriptFlowFile_methods
};


PyObject *PyScriptFlowFile::newInstance(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  auto self = reinterpret_cast<PyScriptFlowFile*>(type->tp_alloc(type, 0));
  if (self == nullptr) {
    return nullptr;
  }

  self->script_flow_file_.reset();
  return reinterpret_cast<PyObject*>(self);
}

int PyScriptFlowFile::init(PyScriptFlowFile *self, PyObject *args, PyObject *kwds) {
  PyObject *weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto script_flow_file = static_cast<std::weak_ptr<ScriptFlowFile>*>(PyCapsule_GetPointer(weak_ptr_capsule, nullptr));
//   Py_DECREF(weak_ptr_capsule);
  self->script_flow_file_ = *script_flow_file;
  return 0;
}

void PyScriptFlowFile::dealloc(PyScriptFlowFile *self) {
  self->script_flow_file_.reset();
}

PyObject *PyScriptFlowFile::getAttribute(PyScriptFlowFile *self, PyObject *args) {
  auto flow_file = self->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  const char *attribute;
  if (!PyArg_ParseTuple(args, "s", &attribute)) {
    throw PyException();
  }
  return object::returnReference(flow_file->getAttribute(std::string(attribute)));
}

PyObject *PyScriptFlowFile::addAttribute(PyScriptFlowFile *self, PyObject *args) {
  auto flow_file = self->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  const char *key;
  const char *value;
  if (!PyArg_ParseTuple(args, "ss", &key, &value)) {
    throw PyException();
  }

  return object::returnReference(flow_file->addAttribute(std::string(key), std::string(value)));
}

PyObject *PyScriptFlowFile::updateAttribute(PyScriptFlowFile *self, PyObject *args) {
  auto flow_file = self->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  const char *key;
  const char *value;
  if (!PyArg_ParseTuple(args, "ss", &key, &value)) {
    throw PyException();
  }

  return object::returnReference(flow_file->updateAttribute(std::string(key), std::string(value)));
}

PyObject *PyScriptFlowFile::removeAttribute(PyScriptFlowFile *self, PyObject *args) {
  auto flow_file = self->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  const char *attribute;
  if (!PyArg_ParseTuple(args, "s", &attribute)) {
    throw PyException();
  }
  return object::returnReference(flow_file->removeAttribute(std::string(attribute)));
}

PyObject *PyScriptFlowFile::setAttribute(PyScriptFlowFile *self, PyObject *args) {
  auto flow_file = self->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  const char *key;
  const char *value;
  if (!PyArg_ParseTuple(args, "ss", &key, &value)) {
    throw PyException();
  }

  return object::returnReference(flow_file->setAttribute(key, value));
}

PyTypeObject *PyScriptFlowFile::typeObject() {
  return &PyScriptFlowFileType;
}
} // org::apache::nifi::minifi::python
} // extern "C"