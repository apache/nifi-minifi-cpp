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

#include "PyLogger.h"
#include "minifi-cpp/Exception.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyLogger_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"error", (PyCFunction) PyLogger::error, METH_VARARGS, nullptr},
    {"warn", (PyCFunction) PyLogger::warn, METH_VARARGS, nullptr},
    {"info", (PyCFunction) PyLogger::info, METH_VARARGS, nullptr},
    {"debug", (PyCFunction) PyLogger::debug, METH_VARARGS, nullptr},
    {"trace", (PyCFunction) PyLogger::trace, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyLoggerTypeSpecSlots[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PyLogger>)},
    {Py_tp_init, reinterpret_cast<void*>(PyLogger::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyLogger_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PyLogger>)},
    {}  /* Sentinel */
};

static PyType_Spec PyLoggerTypeSpec{
    .name = "minifi_native.Logger",
    .basicsize = sizeof(PyLogger),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyLoggerTypeSpecSlots
};

int PyLogger::init(PyLogger* self, PyObject* args, PyObject*) {
  PyObject* weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto weak_ptr = PyCapsule_GetPointer(weak_ptr_capsule, HeldTypeName);
  if (!weak_ptr)
    return -1;
  self->logger_ = *static_cast<HeldType*>(weak_ptr);
  return 0;
}

PyObject* PyLogger::error(PyLogger* self, PyObject* args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    return nullptr;
  }

  const char* message = nullptr;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    return nullptr;
  }
  logger->log_error("{}", message);
  Py_RETURN_NONE;
}

PyObject* PyLogger::warn(PyLogger* self, PyObject* args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    return nullptr;
  }

  const char* message = nullptr;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    return nullptr;
  }
  logger->log_warn("{}", message);
  Py_RETURN_NONE;
}

PyObject* PyLogger::info(PyLogger* self, PyObject* args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    return nullptr;
  }

  const char* message = nullptr;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    return nullptr;
  }
  logger->log_info("{}", message);
  Py_RETURN_NONE;
}

PyObject* PyLogger::debug(PyLogger* self, PyObject* args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    return nullptr;
  }

  const char* message = nullptr;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    return nullptr;
  }
  logger->log_debug("{}", message);
  Py_RETURN_NONE;
}

PyObject* PyLogger::trace(PyLogger* self, PyObject* args) {
  auto logger = self->logger_.lock();
  if (logger == nullptr) {
    PyErr_SetString(PyExc_AttributeError, "internal 'logger' instance is null");
    return nullptr;
  }

  const char* message = nullptr;
  if (!PyArg_ParseTuple(args, "s", &message)) {
    return nullptr;
  }
  logger->log_trace("{}", message);
  Py_RETURN_NONE;
}

PyTypeObject* PyLogger::typeObject() {
  static OwnedObject PyLoggerType{PyType_FromSpec(&PyLoggerTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyLoggerType.get());
}
}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
