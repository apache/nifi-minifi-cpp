#include "PyProcessContext.h"
#include "Exception.h"

extern "C" {
namespace org::apache::nifi::minifi::python {

static PyMethodDef PyProcessContext_methods[] = {
  { "getProperty", (PyCFunction) PyProcessContext::getProperty, METH_VARARGS, nullptr },
  { nullptr }  /* Sentinel */
};

static PyTypeObject PyProcessContextType = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  .tp_name = "minifi_native.ProcessContext",
  .tp_doc = nullptr,
  .tp_basicsize = sizeof(PyProcessContext),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_new = PyProcessContext::newInstance,
  .tp_init = reinterpret_cast<initproc>(PyProcessContext::init),
  .tp_dealloc = reinterpret_cast<destructor>(PyProcessContext::dealloc),
  .tp_methods = PyProcessContext_methods
};

PyObject *PyProcessContext::newInstance(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  auto self = reinterpret_cast<PyProcessContext*>(type->tp_alloc(type, 0));
  if (self == nullptr) {
    return nullptr;
  }

  self->process_context_.reset();
  return reinterpret_cast<PyObject*>(self);
}

int PyProcessContext::init(PyProcessContext *self, PyObject *args, PyObject *kwds) {
  PyObject *weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto process_context = static_cast<HeldType*>(PyCapsule_GetPointer(weak_ptr_capsule, nullptr));
  // Py_DECREF(weak_ptr_capsule);
  self->process_context_ = *process_context;
  return 0;
}

void PyProcessContext::dealloc(PyProcessContext *self) {
  self->process_context_.reset();
}

PyObject *PyProcessContext::getProperty(PyProcessContext *self, PyObject *args) {
  auto context = self->process_context_.lock();
  if (!context) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process context outside 'on_trigger'");
    return nullptr;
  }

  const char *property;
  if (!PyArg_ParseTuple(args, "s", &property)) {
    throw PyException();
  }
  return object::returnReference(context->getProperty(std::string(property)));
}

PyTypeObject *PyProcessContext::typeObject() {
  return &PyProcessContextType;
}

}  // namespace org::apache::nifi::minifi::python
}  // extern "C"
