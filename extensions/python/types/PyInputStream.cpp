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
#include <vector>

#include "Types.h"
#include "minifi-cpp/utils/gsl.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyInputStream_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"read", (PyCFunction) PyInputStream::read, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyInputStreamTypeSpecSlots[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PyInputStream>)},
    {Py_tp_init, reinterpret_cast<void*>(PyInputStream::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyInputStream_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PyInputStream>)},
    {}  /* Sentinel */
};

static PyType_Spec PyInputStreamTypeSpec{
    .name = "minifi_native.InputStream",
    .basicsize = sizeof(PyInputStream),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyInputStreamTypeSpecSlots
};

int PyInputStream::init(PyInputStream* self, PyObject* args, PyObject*) {
  PyObject* weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto input_stream = PyCapsule_GetPointer(weak_ptr_capsule, HeldTypeName);
  if (!input_stream)
    return -1;
  self->input_stream_ = *static_cast<HeldType*>(input_stream);
  return 0;
}

PyObject* PyInputStream::read(PyInputStream* self, PyObject* args) {
  auto input_stream = self->input_stream_.lock();
  if (!input_stream) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  size_t len = input_stream->size();
  if (!PyArg_ParseTuple(args, "|K")) {
    return nullptr;
  }

  if (len == 0) {
    return object::returnReference(OwnedBytes::fromStringAndSize(""));
  }

  std::vector<std::byte> buffer(len);

  const auto read = input_stream->read(buffer);
  return object::returnReference(OwnedBytes::fromStringAndSize(std::string_view(reinterpret_cast<const char*>(buffer.data()), read)));
}

PyTypeObject* PyInputStream::typeObject() {
  static OwnedObject PyInputStreamType{PyType_FromSpec(&PyInputStreamTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyInputStreamType.get());
}

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
