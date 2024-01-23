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
a * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PyProcessContext.h"
#include "PyStateManager.h"
#include "PyScriptFlowFile.h"
#include <string>
#include "PyException.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyProcessContext_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"getProperty", (PyCFunction) PyProcessContext::getProperty, METH_VARARGS, nullptr},
    {"getStateManager", (PyCFunction) PyProcessContext::getStateManager, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyProcessContextTypeSpecSlots[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PyProcessContext>)},
    {Py_tp_init, reinterpret_cast<void*>(PyProcessContext::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyProcessContext_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PyProcessContext>)},
    {}  /* Sentinel */
};

static PyType_Spec PyProcessContextTypeSpec{
    .name = "minifi_native.ProcessContext",
    .basicsize = sizeof(PyProcessContext),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyProcessContextTypeSpecSlots
};

int PyProcessContext::init(PyProcessContext* self, PyObject* args, PyObject*) {
  PyObject* weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto process_context = PyCapsule_GetPointer(weak_ptr_capsule, HeldTypeName);
  if (!process_context)
    throw PyException();
  self->process_context_ = *static_cast<HeldType*>(process_context);
  return 0;
}

PyObject* PyProcessContext::getProperty(PyProcessContext* self, PyObject* args) {
  auto context = self->process_context_.lock();
  if (!context) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process context outside 'on_trigger'");
    return nullptr;
  }

  const char* property_name = nullptr;
  PyObject* script_flow_file = nullptr;
  if (!PyArg_ParseTuple(args, "s|O", &property_name, &script_flow_file)) {
    throw PyException();
  }

  std::string value;
  if (!script_flow_file) {
    if (!context->getProperty(property_name, value)) {
      Py_RETURN_NONE;
    }
  } else {
    auto py_flow = reinterpret_cast<PyScriptFlowFile*>(script_flow_file);
    const auto flow_file = py_flow->script_flow_file_.lock();
    if (!flow_file) {
      PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
      return nullptr;
    }
    core::Property property{property_name, ""};
    property.setSupportsExpressionLanguage(true);
    if (!context->getProperty(property, value, flow_file)) {
      Py_RETURN_NONE;
    }
  }

  return object::returnReference(value);
}

PyObject* PyProcessContext::getStateManager(PyProcessContext* self, PyObject*) {
  auto context = self->process_context_.lock();
  if (!context) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process context outside 'on_trigger'");
    return nullptr;
  }

  return object::returnReference(context->getStateManager());
}

PyTypeObject* PyProcessContext::typeObject() {
  static OwnedObject PyProcessContextType{PyType_FromSpec(&PyProcessContextTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyProcessContextType.get());
}

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
