#include "PyProcessor.h"
#include "Exception.h"

extern "C" {
namespace org::apache::nifi::minifi::python {

static PyMethodDef PyProcessor_methods[] = {
  { "setSupportsDynamicProperties", (PyCFunction) PyProcessor::setSupportsDynamicProperties, METH_VARARGS, nullptr },
  { "setDescription", (PyCFunction) PyProcessor::setDescription, METH_VARARGS, nullptr },
  { "addProperty", (PyCFunction) PyProcessor::addProperty, METH_VARARGS, nullptr },
  { nullptr }  /* Sentinel */
};

static PyTypeObject PyProcessorType = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  .tp_name = "minifi_native.Processor",
  .tp_doc = nullptr,
  .tp_basicsize = sizeof(PyProcessor),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_new = PyProcessor::newInstance,
  .tp_init = reinterpret_cast<initproc>(PyProcessor::init),
  .tp_dealloc = reinterpret_cast<destructor>(PyProcessor::dealloc),
  .tp_methods = PyProcessor_methods
};

PyObject *PyProcessor::newInstance(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  auto self = reinterpret_cast<PyProcessor*>(type->tp_alloc(type, 0));
  if (self == nullptr) {
    return nullptr;
  }

  self->processor_.reset();
  return reinterpret_cast<PyObject*>(self);
}

int PyProcessor::init(PyProcessor *self, PyObject *args, PyObject *kwds) {
  PyObject *weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto processor = static_cast<HeldType*>(PyCapsule_GetPointer(weak_ptr_capsule, nullptr));
  self->processor_ = *processor;
  return 0;
}

void PyProcessor::dealloc(PyProcessor *self) {
  self->processor_.reset();
}

PyObject *PyProcessor::setSupportsDynamicProperties(PyProcessor *self, PyObject *args) {
  auto processor = self->processor_.lock();
  if (!processor) {
    PyErr_SetString(PyExc_AttributeError, "tried reading processor outside 'on_trigger'");
    Py_RETURN_NONE;
  }

  processor->setSupportsDynamicProperties();
  Py_RETURN_NONE;
}

PyObject *PyProcessor::setDescription(PyProcessor *self, PyObject *args) {
  auto processor = self->processor_.lock();
  if (!processor) {
    PyErr_SetString(PyExc_AttributeError, "tried reading processor outside 'on_trigger'");
    Py_RETURN_NONE;
  }

  const char *description;
  if (!PyArg_ParseTuple(args, "s", &description)) {
    throw PyException();
  }
  processor->setDescription(std::string(description));
  Py_RETURN_NONE;
}

PyObject *PyProcessor::addProperty(PyProcessor *self, PyObject *args) {
  auto processor = self->processor_.lock();
  if (!processor) {
    PyErr_SetString(PyExc_AttributeError, "tried reading processor outside 'on_trigger'");
    Py_RETURN_NONE;
  }

  const char *name;
  const char *description;
  const char *defaultValue;
  bool required;
  bool el;
  if (!PyArg_ParseTuple(args, "ssspp", &name, &description, &defaultValue, &required, &el)) {
    throw PyException();
  }

  //const std::string &name, const std::string &description, const std::string &defaultvalue, bool required, bool el
  processor->addProperty(std::string(name), std::string(description), std::string(defaultValue), required, el);
  Py_RETURN_NONE;
}

PyTypeObject *PyProcessor::typeObject() {
  return &PyProcessorType;
}

}  // namespace org::apache::nifi::minifi::python
}  // extern "C"