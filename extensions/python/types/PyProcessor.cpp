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
#include "Types.h"

namespace org::apache::nifi::minifi::extensions::python {
namespace {
bool getBoolFromTuple(PyObject* tuple, Py_ssize_t location) {
  auto object = PyTuple_GetItem(tuple, location);

  if (!object) {
    throw PyException();
  }

  if (object == Py_True)
    return true;
  if (object == Py_False)
    return false;

  PyErr_SetString(PyExc_AttributeError, "Expected to get boolean parameter, but got something else");
  throw PyException();
}
}  // namespace

extern "C" {

static PyMethodDef PyProcessor_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"setSupportsDynamicProperties", (PyCFunction) PyProcessor::setSupportsDynamicProperties, METH_VARARGS, nullptr},
    {"setDescription", (PyCFunction) PyProcessor::setDescription, METH_VARARGS, nullptr},
    {"setVersion", (PyCFunction) PyProcessor::setVersion, METH_VARARGS, nullptr},
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
    return -1;
  self->processor_ = *static_cast<HeldType*>(processor);
  return 0;
}

PyObject* PyProcessor::setSupportsDynamicProperties(PyProcessor* self, PyObject*) {
  auto processor = self->processor_.lock();
  if (!processor) {
    PyErr_SetString(PyExc_AttributeError, "tried reading processor outside 'on_trigger'");
    return nullptr;
  }

  processor->setSupportsDynamicProperties();
  Py_RETURN_NONE;
}

PyObject* PyProcessor::setDescription(PyProcessor* self, PyObject* args) {
  auto processor = self->processor_.lock();
  if (!processor) {
    PyErr_SetString(PyExc_AttributeError, "tried reading processor outside 'on_trigger'");
    return nullptr;
  }

  const char* description = nullptr;
  if (!PyArg_ParseTuple(args, "s", &description)) {
    return nullptr;
  }
  processor->setDescription(std::string(description));
  Py_RETURN_NONE;
}

PyObject* PyProcessor::setVersion(PyProcessor* self, PyObject* args) {
  auto processor = self->processor_.lock();
  if (!processor) {
    PyErr_SetString(PyExc_AttributeError, "tried reading processor outside 'on_trigger'");
    return nullptr;
  }

  const char* version = nullptr;
  if (!PyArg_ParseTuple(args, "s", &version)) {
    return nullptr;
  }
  processor->setVersion(std::string(version));
  Py_RETURN_NONE;
}

PyObject* PyProcessor::addProperty(PyProcessor* self, PyObject* args) {
  auto processor = self->processor_.lock();
  if (!processor) {
    PyErr_SetString(PyExc_AttributeError, "tried reading processor outside 'on_trigger'");
    return nullptr;
  }

  static constexpr Py_ssize_t ExpectedNumArgs = 2;
  auto arg_size = PyTuple_Size(args);
  if (arg_size < ExpectedNumArgs) {
    PyErr_SetString(PyExc_AttributeError, fmt::format("addProperty was called with too few arguments: need {}, got {}", ExpectedNumArgs, arg_size).c_str());
    return nullptr;
  }

  BorrowedStr name = BorrowedStr::fromTuple(args, 0);
  BorrowedStr description = BorrowedStr::fromTuple(args, 1);
  std::optional<std::string> default_value;
  if (arg_size > 2) {
    auto default_value_pystr = BorrowedStr::fromTuple(args, 2);
    if (default_value_pystr.get() && default_value_pystr.get() != Py_None) {
      default_value = default_value_pystr.toUtf8String();
    }
  }

  bool is_required = false;
  bool supports_expression_language = false;
  bool sensitive = false;

  if (arg_size > 3) {
    is_required = getBoolFromTuple(args, 3);
  }

  if (arg_size > 4) {
    supports_expression_language = getBoolFromTuple(args, 4);
  }

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

  std::vector<std::string> allowable_values_str;
  if (arg_size > 7) {
    auto allowable_values_pylist = BorrowedList::fromTuple(args, 7);
    if (allowable_values_pylist.get() && allowable_values_pylist.get() != Py_None) {
      for (size_t i = 0; i < allowable_values_pylist.length(); ++i) {
        auto value = BorrowedStr{allowable_values_pylist[i]};
        allowable_values_str.push_back(value.toUtf8String());
      }
    }
  }
  std::vector<std::string_view> allowable_values(begin(allowable_values_str), end(allowable_values_str));

  std::optional<std::string> controller_service_type_name;
  if (arg_size > 8) {
    auto controller_service_type_name_pystr = BorrowedStr::fromTuple(args, 8);
    if (controller_service_type_name_pystr.get() && controller_service_type_name_pystr.get() != Py_None) {
      controller_service_type_name = controller_service_type_name_pystr.toUtf8String();
    }
  }

  processor->addProperty(name.toUtf8String(), description.toUtf8String(), default_value, is_required, supports_expression_language, sensitive,
      validator_value, allowable_values, controller_service_type_name);
  Py_RETURN_NONE;
}

PyTypeObject* PyProcessor::typeObject() {
  static OwnedObject PyProcessorType{PyType_FromSpec(&PyProcessorTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyProcessorType.get());
}

}  // extern "C"
}  // namespace org::apache::nifi::minifi::extensions::python
