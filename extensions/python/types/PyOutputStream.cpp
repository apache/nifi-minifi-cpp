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

#include "PyOutputStream.h"

#include <string>

#include "PyException.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyOutputStream_methods[] = {
    {"write", (PyCFunction) PyOutputStream::write, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyOutputStreamTypeSpecSlots[] = {
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PyOutputStream>)},
    {Py_tp_init, reinterpret_cast<void*>(PyOutputStream::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyOutputStream_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PyOutputStream>)},
    {}  /* Sentinel */
};

static PyType_Spec PyOutputStreamTypeSpec{
    .name = "minifi_native.OutputStream",
    .basicsize = sizeof(PyOutputStream),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyOutputStreamTypeSpecSlots
};

int PyOutputStream::init(PyOutputStream* self, PyObject* args, PyObject*) {
  PyObject* weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto output_stream = PyCapsule_GetPointer(weak_ptr_capsule, HeldTypeName);
  if (!output_stream)
    throw PyException();
  self->output_stream_ = *static_cast<HeldType*>(output_stream);
  return 0;
}

PyObject* PyOutputStream::write(PyOutputStream* self, PyObject* args) {
  auto output_stream = self->output_stream_.lock();
  if (!output_stream) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  PyObject* bytes;
  if (!PyArg_ParseTuple(args, "S", &bytes)) {
    throw PyException();
  }

  char* buffer = nullptr;
  Py_ssize_t length = 0;
  if (PyBytes_AsStringAndSize(bytes, &buffer, &length) == -1) {
    throw PyException();
  }
  return object::returnReference(output_stream->write(gsl::make_span(buffer, length).as_span<const std::byte>()));
}

PyTypeObject* PyOutputStream::typeObject() {
  static OwnedObject PyOutputStreamType{PyType_FromSpec(&PyOutputStreamTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyOutputStreamType.get());
}

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
