/**
 *
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

#include "PyInputStream.h"
#include "Exception.h"
#include "utils/gsl.h"

extern "C" {
namespace org::apache::nifi::minifi::python {

static PyMethodDef PyInputStream_methods[] = {
  { "read", (PyCFunction) PyInputStream::read, METH_VARARGS, nullptr, },
  { nullptr } /* Sentinel */
};

static PyTypeObject PyInputStreamType = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  .tp_name = "minifi_native.InputStream",
  .tp_doc = nullptr,
  .tp_basicsize = sizeof(PyInputStream),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_new = PyInputStream::newInstance,
  .tp_init = reinterpret_cast<initproc>(PyInputStream::init),
  .tp_dealloc = reinterpret_cast<destructor>(PyInputStream::dealloc),
  .tp_methods = PyInputStream_methods
};

PyObject *PyInputStream::newInstance(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  auto self = reinterpret_cast<PyInputStream*>(type->tp_alloc(type, 0));
  if (self == nullptr) {
    return nullptr;
  }

  self->input_stream_.reset();
  return reinterpret_cast<PyObject*>(self);
}

int PyInputStream::init(PyInputStream *self, PyObject *args, PyObject *kwds) {
  PyObject *weak_ptr_capsule = nullptr;
    if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto input_stream = static_cast<HeldType*>(PyCapsule_GetPointer(weak_ptr_capsule, nullptr));
  // Py_DECREF(weak_ptr_capsule);
  self->input_stream_ = *input_stream;
  return 0;
}

void PyInputStream::dealloc(PyInputStream *self) {
  self->input_stream_.reset();
}

PyObject *PyInputStream::read(PyInputStream *self, PyObject *args) {
  auto input_stream = self->input_stream_.lock();
  if (!input_stream) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  size_t len = 0;
  if (!PyArg_ParseTuple(args, "|K")) {
    throw PyException();
  }

  if (len == 0) {
    len = input_stream->size();
  }

  if (len == 0) {
    return nullptr;
  }

  std::vector<std::byte> buffer(len);

  const auto read = input_stream->read(buffer);
  return object::returnReference(OwnedBytes::fromStringAndSize(std::string_view(reinterpret_cast<const char*>(buffer.data()), read)));
}

PyTypeObject *PyInputStream::typeObject() {
  return &PyInputStreamType;
}

}  // namespace org::apache::nifi::minifi::python
}  // extern "C"
