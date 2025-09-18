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

#include <string>

#include "PyStateManager.h"
#include "PyScriptFlowFile.h"
#include "core/ProcessorImpl.h"
#include "minifi-cpp/controllers/RecordSetReader.h"
#include "minifi-cpp/controllers/RecordSetWriter.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyProcessContext_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"getProperty", (PyCFunction) PyProcessContext::getProperty, METH_VARARGS, nullptr},
    {"getStateManager", (PyCFunction) PyProcessContext::getStateManager, METH_VARARGS, nullptr},
    {"getControllerService", (PyCFunction) PyProcessContext::getControllerService, METH_VARARGS, nullptr},
    {"getName", (PyCFunction) PyProcessContext::getName, METH_VARARGS, nullptr},
    {"getProperties", (PyCFunction) PyProcessContext::getProperties, METH_VARARGS, nullptr},
    {"yieldResources", (PyCFunction) PyProcessContext::getProperties, METH_VARARGS, nullptr},
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
    return -1;
  self->process_context_ = *static_cast<HeldType*>(process_context);
  return 0;
}

PyObject* PyProcessContext::getProperty(PyProcessContext* self, PyObject* args) {
  auto context = self->process_context_;
  if (!context) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process context outside 'on_trigger'");
    return nullptr;
  }

  const char* property_name = nullptr;
  PyObject* script_flow_file = nullptr;
  if (!PyArg_ParseTuple(args, "s|O", &property_name, &script_flow_file)) {
    return nullptr;
  }

  if (!script_flow_file) {
    if (const auto property_value = context->getProperty(property_name)) {
      return object::returnReference(*property_value);
    }
    Py_RETURN_NONE;
  }
  auto py_flow = reinterpret_cast<PyScriptFlowFile*>(script_flow_file);
  const auto flow_file = py_flow->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }
  core::Property property{property_name, ""};
  property.setSupportsExpressionLanguage(true);
  if (const auto property_value = context->getProperty(property, flow_file.get())) {
    return object::returnReference(*property_value);
  }
  Py_RETURN_NONE;
}

PyObject* PyProcessContext::getStateManager(PyProcessContext* self, PyObject*) {
  auto context = self->process_context_;
  if (!context) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process context outside 'on_trigger'");
    return nullptr;
  }

  return object::returnReference(context->getStateManager());
}

PyObject* PyProcessContext::getControllerService(PyProcessContext* self, PyObject* args) {
  auto context = self->process_context_;
  if (!context) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process context outside 'on_trigger'");
    return nullptr;
  }

  const char* controller_service_name = nullptr;
  const char* controller_service_type = nullptr;
  if (!PyArg_ParseTuple(args, "s|s", &controller_service_name, &controller_service_type)) {
    return nullptr;
  }

  if (auto controller_service = context->getControllerService(controller_service_name, context->getProcessorInfo().getUUID())) {
    std::string controller_service_type_str = controller_service_type;
    if (controller_service_type_str == "SSLContextService") {
      auto ssl_ctx_service = std::dynamic_pointer_cast<controllers::SSLContextServiceInterface>(controller_service);
      return object::returnReference(std::weak_ptr(ssl_ctx_service));
    } else if (controller_service_type_str == "RecordSetReader") {
      auto record_set_reader = std::dynamic_pointer_cast<core::RecordSetReader>(controller_service);
      return object::returnReference(std::weak_ptr(record_set_reader));
    } else if (controller_service_type_str == "RecordSetWriter") {
      auto record_set_writer = std::dynamic_pointer_cast<core::RecordSetWriter>(controller_service);
      return object::returnReference(std::weak_ptr(record_set_writer));
    }
  }

  Py_RETURN_NONE;
}

PyObject* PyProcessContext::getName(PyProcessContext* self, PyObject*) {
  auto context = self->process_context_;
  if (!context) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process context outside 'on_trigger'");
    return nullptr;
  }

  return object::returnReference(context->getProcessorInfo().getName());
}

PyObject* PyProcessContext::getProperties(PyProcessContext* self, PyObject*) {
  auto context = self->process_context_;
  if (!context) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process context outside 'on_trigger'");
    return nullptr;
  }

  auto properties = context->getProcessorInfo().getSupportedProperties();
  auto py_properties = OwnedDict::create();
  for (const auto& [property_name, property] : properties) {
    if (const auto value = context->getProperty(property_name)) {
      py_properties.put(property_name, *value);
    }
  }

  return object::returnReference(py_properties);
}

PyObject* PyProcessContext::yieldResources(PyProcessContext* self, PyObject*) {
  auto context = self->process_context_;
  if (!context) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process context outside 'on_trigger'");
    return nullptr;
  }

  context->yield();

  Py_RETURN_NONE;
}

PyTypeObject* PyProcessContext::typeObject() {
  static OwnedObject PyProcessContextType{PyType_FromSpec(&PyProcessContextTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyProcessContextType.get());
}

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
