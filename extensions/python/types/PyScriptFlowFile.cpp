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

#include "PyScriptFlowFile.h"
#include <string>

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyScriptFlowFile_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"getAttribute", (PyCFunction) PyScriptFlowFile::getAttribute, METH_VARARGS, nullptr},
    {"addAttribute", (PyCFunction) PyScriptFlowFile::addAttribute, METH_VARARGS, nullptr},
    {"updateAttribute", (PyCFunction) PyScriptFlowFile::updateAttribute, METH_VARARGS, nullptr},
    {"removeAttribute", (PyCFunction) PyScriptFlowFile::removeAttribute, METH_VARARGS, nullptr},
    {"setAttribute", (PyCFunction) PyScriptFlowFile::setAttribute, METH_VARARGS, nullptr},
    {"getSize", (PyCFunction) PyScriptFlowFile::getSize, METH_VARARGS, nullptr},
    {"getAttributes", (PyCFunction) PyScriptFlowFile::getAttributes, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyScriptFlowFileTypeSpecSlots[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PyScriptFlowFile>)},
    {Py_tp_init, reinterpret_cast<void*>(PyScriptFlowFile::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyScriptFlowFile_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PyScriptFlowFile>)},
    {}  /* Sentinel */
};

static PyType_Spec PyScriptFlowFileTypeSpec{
    .name = "minifi_native.FlowFile",
    .basicsize = sizeof(PyScriptFlowFile),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyScriptFlowFileTypeSpecSlots
};

int PyScriptFlowFile::init(PyScriptFlowFile* self, PyObject* args, PyObject*) {
  PyObject* weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto script_flow_file = PyCapsule_GetPointer(weak_ptr_capsule, HeldTypeName);
  if (!script_flow_file)
    return -1;
  self->script_flow_file_ = *static_cast<HeldType*>(script_flow_file);

  return 0;
}

PyObject* PyScriptFlowFile::getAttribute(PyScriptFlowFile* self, PyObject* args) {
  auto flow_file = self->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  const char* attribute = nullptr;
  if (!PyArg_ParseTuple(args, "s", &attribute)) {
    return nullptr;
  }
  return object::returnReference(flow_file->getAttribute(attribute).value_or(""));
}

PyObject* PyScriptFlowFile::addAttribute(PyScriptFlowFile* self, PyObject* args) {
  auto flow_file = self->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  const char* key = nullptr;
  const char* value = nullptr;
  if (!PyArg_ParseTuple(args, "ss", &key, &value)) {
    return nullptr;
  }

  return object::returnReference(flow_file->addAttribute(key, std::string(value)));
}

PyObject* PyScriptFlowFile::updateAttribute(PyScriptFlowFile* self, PyObject* args) {
  auto flow_file = self->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  const char* key = nullptr;
  const char* value = nullptr;
  if (!PyArg_ParseTuple(args, "ss", &key, &value)) {
    return nullptr;
  }

  return object::returnReference(flow_file->updateAttribute(key, std::string(value)));
}

PyObject* PyScriptFlowFile::removeAttribute(PyScriptFlowFile* self, PyObject* args) {
  auto flow_file = self->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  const char* attribute = nullptr;
  if (!PyArg_ParseTuple(args, "s", &attribute)) {
    return nullptr;
  }
  return object::returnReference(flow_file->removeAttribute(attribute));
}

PyObject* PyScriptFlowFile::setAttribute(PyScriptFlowFile* self, PyObject* args) {
  auto flow_file = self->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  const char* key = nullptr;
  const char* value = nullptr;
  if (!PyArg_ParseTuple(args, "ss", &key, &value)) {
    return nullptr;
  }

  flow_file->setAttribute(key, value);
  Py_RETURN_NONE;
}

PyObject* PyScriptFlowFile::getSize(PyScriptFlowFile* self, PyObject* /*args*/) {
  auto flow_file = self->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  return object::returnReference(flow_file->getSize());
}

PyObject* PyScriptFlowFile::getAttributes(PyScriptFlowFile* self, PyObject* /*args*/) {
  auto flow_file = self->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  auto attributes = OwnedDict::create();
  for (const auto& [key, value] : flow_file->getAttributes()) {
    attributes.put(key, value);
  }

  return object::returnReference(attributes);
}

PyTypeObject* PyScriptFlowFile::typeObject() {
  static OwnedObject PyScriptFlowFileType{PyType_FromSpec(&PyScriptFlowFileTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyScriptFlowFileType.get());
}
}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
