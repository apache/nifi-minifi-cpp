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

#include "PyProcessor.h"
#include <string>
#include <optional>
#include "PyException.h"
#include "Types.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyProcessor_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"setSupportsDynamicProperties", (PyCFunction) PyProcessor::setSupportsDynamicProperties, METH_VARARGS, nullptr},
    {"setDescription", (PyCFunction) PyProcessor::setDescription, METH_VARARGS, nullptr},
    {"addProperty", (PyCFunction) PyProcessor::addProperty, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyProcessorTypeSpecSlots[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PyProcessor>)},
    {Py_tp_init, reinterpret_cast<void*>(PyProcessor::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyProcessor_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PyProcessor>)},
    {}  /* Sentinel */
};

static PyType_Spec PyProcessorTypeSpec{
    .name = "minifi_native.Processor",
    .basicsize = sizeof(PyProcessor),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyProcessorTypeSpecSlots
};

int PyProcessor::init(PyProcessor* self, PyObject* args, PyObject*) {
  PyObject* weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto processor = PyCapsule_GetPointer(weak_ptr_capsule, HeldTypeName);
  if (!processor)
    throw PyException();
  self->processor_ = *static_cast<HeldType*>(processor);
  return 0;
}

PyObject* PyProcessor::setSupportsDynamicProperties(PyProcessor* self, PyObject*) {
  auto processor = self->processor_.lock();
  if (!processor) {
    PyErr_SetString(PyExc_AttributeError, "tried reading processor outside 'on_trigger'");
    Py_RETURN_NONE;
  }

  processor->setSupportsDynamicProperties();
  Py_RETURN_NONE;
}

PyObject* PyProcessor::setDescription(PyProcessor* self, PyObject* args) {
  auto processor = self->processor_.lock();
  if (!processor) {
    PyErr_SetString(PyExc_AttributeError, "tried reading processor outside 'on_trigger'");
    Py_RETURN_NONE;
  }

  const char* description = nullptr;
  if (!PyArg_ParseTuple(args, "s", &description)) {
    throw PyException();
  }
  processor->setDescription(std::string(description));
  Py_RETURN_NONE;
}

namespace {
bool getBoolFromTuple(PyObject* tuple, Py_ssize_t location) {
  auto object = PyTuple_GetItem(tuple, location);

  if (!object)
    throw PyException();

  if (object == Py_True)
    return true;
  if (object == Py_False)
    return false;
  throw std::invalid_argument("Expected to get Py_True or Py_False, but got something else");
}
}  // namespace


PyObject* PyProcessor::addProperty(PyProcessor* self, PyObject* args) {
  auto processor = self->processor_.lock();
  if (!processor) {
    PyErr_SetString(PyExc_AttributeError, "tried reading processor outside 'on_trigger'");
    Py_RETURN_NONE;
  }

  BorrowedStr name = BorrowedStr::fromTuple(args, 0);
  BorrowedStr description = BorrowedStr::fromTuple(args, 1);
  std::optional<std::string> default_value;
  auto default_value_pystr = BorrowedStr::fromTuple(args, 2);
  if (default_value_pystr.get() && default_value_pystr.get() != Py_None) {
    default_value = default_value_pystr.toUtf8String();
  }
  bool is_required = getBoolFromTuple(args, 3);
  bool supports_expression_language = getBoolFromTuple(args, 4);

  bool sensitive = false;
  auto arg_size = PyTuple_Size(args);
  if (arg_size > 5) {
    sensitive = getBoolFromTuple(args, 5);
  }

  std::optional<int64_t> validator_value;
  if (arg_size > 6) {
    auto validator_value_pyint = BorrowedLong::fromTuple(args, 6);
    if (validator_value_pyint.get() && validator_value_pyint.get() != Py_None) {
      validator_value = validator_value_pyint.asInt64();
    }
  }

  processor->addProperty(name.toUtf8String(), description.toUtf8String(), default_value, is_required, supports_expression_language, sensitive, validator_value);
  Py_RETURN_NONE;
}

PyTypeObject* PyProcessor::typeObject() {
  static OwnedObject PyProcessorType{PyType_FromSpec(&PyProcessorTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyProcessorType.get());
}

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
