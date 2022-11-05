#include "PyLogger.h"
#include "Exception.h"

extern "C" {
namespace org::apache::nifi::minifi::python {

static PyMethodDef PyLogger_methods[] = {
  { "error", (PyCFunction) PyLogger::error, METH_VARARGS, nullptr },
  { "warn", (PyCFunction) PyLogger::warn, METH_VARARGS, nullptr },
  { "info", (PyCFunction) PyLogger::info, METH_VARARGS, nullptr },
  { "debug", (PyCFunction) PyLogger::debug, METH_VARARGS, nullptr },
  { "trace", (PyCFunction) PyLogger::trace, METH_VARARGS, nullptr },
  { nullptr }  /* Sentinel */
};

static PyTypeObject PyLoggerType = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  .tp_name = "minifi_native.Logger",
  .tp_doc = nullptr,
  .tp_basicsize = sizeof(PyLogger),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_new = PyLogger::newInstance,
  .tp_init = reinterpret_cast<initproc>(PyLogger::init),
  .tp_dealloc = reinterpret_cast<destructor>(PyLogger::dealloc),
  .tp_methods = PyLogger_methods
};

PyObject *PyLogger::newInstance(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  auto self = reinterpret_cast<PyLogger*>(type->tp_alloc(type, 0));
  if (self == nullptr) {
    return nullptr;
  }

  self->logger_.reset();
  return reinterpret_cast<PyObject*>(self);
}

int PyLogger::init(PyLogger *self, PyObject *args, PyObject *kwds) {
  PyObject *weak_ptr_capsule = nullptr;
  // char* keywordArgs[]{ nullptr };
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto weak_ptr = static_cast<std::weak_ptr<Logger>*>(PyCapsule_GetPointer(weak_ptr_capsule, nullptr));
//   Py_DECREF(weak_ptr_capsule);
  self->logger_ = *weak_ptr;
  return 0;
}

void PyLogger::dealloc(PyLogger *self) {
  self->logger_.reset();
}

PyObject *PyLogger::error(PyLogger *self, PyObject *args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    Py_RETURN_NONE;
  }

  const char *message;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    throw PyException();
  }
  logger->log_error(message);
  Py_RETURN_NONE;
}

PyObject *PyLogger::warn(PyLogger *self, PyObject *args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    Py_RETURN_NONE;
  }

  const char *message;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    throw PyException();
  }
  logger->log_warn(message);
  Py_RETURN_NONE;
}

PyObject *PyLogger::info(PyLogger *self, PyObject *args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    Py_RETURN_NONE;
  }

  const char *message;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    throw PyException();
  }
  logger->log_info(message);
  Py_RETURN_NONE;
}

PyObject *PyLogger::debug(PyLogger *self, PyObject *args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    Py_RETURN_NONE;
  }

  const char *message;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    throw PyException();
  }
  logger->log_debug(message);
  Py_RETURN_NONE;
}

PyObject *PyLogger::trace(PyLogger *self, PyObject *args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    Py_RETURN_NONE;
  }

  const char *message;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    throw PyException();
  }
  logger->log_trace(message);
  Py_RETURN_NONE;
}

PyTypeObject *PyLogger::typeObject() {
  return &PyLoggerType;
}
} // namespace org::apache::nifi::minifi::python
} // extern "C"