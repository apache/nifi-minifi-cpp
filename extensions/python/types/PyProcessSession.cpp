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

#include "PyProcessSession.h"
#include <utility>
#include "PyScriptFlowFile.h"
#include "PyRelationship.h"
#include "types/PyOutputStream.h"
#include "types/PyInputStream.h"
#include "range/v3/algorithm/remove_if.hpp"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::extensions::python {

namespace core = org::apache::nifi::minifi::core;

PyProcessSession::PyProcessSession(core::ProcessSession& session)
    : session_(session) {
}

std::shared_ptr<core::FlowFile> PyProcessSession::get() {
  auto flow_file = session_.get();

  if (flow_file == nullptr) {
    return nullptr;
  }

  flow_files_.push_back(flow_file);

  return flow_file;
}

void PyProcessSession::transfer(const std::shared_ptr<core::FlowFile>& flow_file,
                                const core::Relationship& relationship) {
  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_.transfer(flow_file, relationship);
}

void PyProcessSession::transferToCustomRelationship(const std::shared_ptr<core::FlowFile>& flow_file, const std::string& relationship_name) {
  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_.transferToCustomRelationship(flow_file, relationship_name);
}

void PyProcessSession::read(const std::shared_ptr<core::FlowFile>& flow_file, BorrowedObject input_stream_callback) {
  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_.read(flow_file, [&input_stream_callback](const std::shared_ptr<io::InputStream>& input_stream) -> int64_t {
    return Long(Callable(input_stream_callback.getAttribute("process"))(std::weak_ptr(input_stream))).asInt64();
  });
}

void PyProcessSession::write(const std::shared_ptr<core::FlowFile>& flow_file, BorrowedObject output_stream_callback) {
  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_.write(flow_file, [&output_stream_callback](const std::shared_ptr<io::OutputStream>& output_stream) -> int64_t {
    return Long(Callable(output_stream_callback.getAttribute("process"))(std::weak_ptr(output_stream))).asInt64();
  });
}

std::shared_ptr<core::FlowFile> PyProcessSession::create(const std::shared_ptr<core::FlowFile>& flow_file) {
  auto result = session_.create(flow_file.get());

  flow_files_.push_back(result);
  return result;
}

std::shared_ptr<core::FlowFile> PyProcessSession::clone(const std::shared_ptr<core::FlowFile>& flow_file) {
  if (!flow_file) {
    throw std::runtime_error("Flow file to clone is nullptr");
  }

  auto result = session_.clone(*flow_file);

  flow_files_.push_back(result);
  return result;
}

void PyProcessSession::remove(const std::shared_ptr<core::FlowFile>& flow_file) {
  session_.remove(flow_file);
  flow_files_.erase(ranges::remove_if(flow_files_, [&flow_file](const auto& ff)-> bool { return ff == flow_file; }), flow_files_.end());
}

std::string PyProcessSession::getContentsAsString(const std::shared_ptr<core::FlowFile>& flow_file) {
  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  std::string content;
  session_.read(flow_file, [&content](const std::shared_ptr<io::InputStream>& input_stream) -> int64_t {
    content.resize(input_stream->size());
    return gsl::narrow<int64_t>(input_stream->read(as_writable_bytes(std::span(content))));
  });
  return content;
}

void PyProcessSession::putAttribute(const std::shared_ptr<core::FlowFile>& flow_file, std::string_view key, const std::string& value) {
  session_.putAttribute(*flow_file, key, value);
}

extern "C" {

static PyMethodDef PyProcessSessionObject_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"get", (PyCFunction) PyProcessSessionObject::get, METH_NOARGS, nullptr},
    {"create", (PyCFunction) PyProcessSessionObject::create, METH_VARARGS, nullptr},
    {"clone", (PyCFunction) PyProcessSessionObject::clone, METH_VARARGS, nullptr},
    {"read", (PyCFunction) PyProcessSessionObject::read, METH_VARARGS, nullptr},
    {"write", (PyCFunction) PyProcessSessionObject::write, METH_VARARGS, nullptr},
    {"transfer", (PyCFunction) PyProcessSessionObject::transfer, METH_VARARGS, nullptr},
    {"transferToCustomRelationship", (PyCFunction) PyProcessSessionObject::transferToCustomRelationship, METH_VARARGS, nullptr},
    {"remove", (PyCFunction) PyProcessSessionObject::remove, METH_VARARGS, nullptr},
    {"getContentsAsBytes", (PyCFunction) PyProcessSessionObject::getContentsAsBytes, METH_VARARGS, nullptr},
    {"putAttribute", (PyCFunction) PyProcessSessionObject::putAttribute, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyProcessTypeSpecSlots[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PyProcessSessionObject>)},
    {Py_tp_init, reinterpret_cast<void*>(PyProcessSessionObject::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyProcessSessionObject_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PyProcessSessionObject>)},
    {}  /* Sentinel */
};

static PyType_Spec PyProcessSessionObjectTypeSpec{
    .name = "minifi_native.ProcessSession",
    .basicsize = sizeof(PyProcessSessionObject),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyProcessTypeSpecSlots
};

int PyProcessSessionObject::init(PyProcessSessionObject* self, PyObject* args, PyObject*) {
  PyObject* weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto process_session = PyCapsule_GetPointer(weak_ptr_capsule, HeldTypeName);
  if (!process_session)
    return -1;
  self->process_session_ = *static_cast<std::weak_ptr<PyProcessSession>*>(process_session);
  return 0;
}

PyObject* PyProcessSessionObject::get(PyProcessSessionObject* self, PyObject*) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    return nullptr;
  }
  if (auto flow_file = session->get())
    return object::returnReference(std::weak_ptr(flow_file));
  return object::returnReference(nullptr);
}

PyObject* PyProcessSessionObject::create(PyProcessSessionObject* self, PyObject* args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    return nullptr;
  }

  std::shared_ptr<core::FlowFile> parent_flow_file;
  auto arg_size = PyTuple_Size(args);
  if (arg_size > 0) {
    PyObject* script_flow_file = nullptr;
    if (!PyArg_ParseTuple(args, "O!", PyScriptFlowFile::typeObject(), &script_flow_file)) {
      return nullptr;
    }
    parent_flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  }

  if (auto flow_file = session->create(parent_flow_file))
    return object::returnReference(std::weak_ptr(flow_file));
  return object::returnReference(nullptr);
}

PyObject* PyProcessSessionObject::clone(PyProcessSessionObject* self, PyObject* args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    return nullptr;
  }
  PyObject* script_flow_file = nullptr;
  if (!PyArg_ParseTuple(args, "O!", PyScriptFlowFile::typeObject(), &script_flow_file)) {
    return nullptr;
  }
  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();

  if (auto cloned_flow_file = session->clone(flow_file))
    return object::returnReference(std::weak_ptr(cloned_flow_file));
  return object::returnReference(nullptr);
}

PyObject* PyProcessSessionObject::remove(PyProcessSessionObject* self, PyObject* args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    return nullptr;
  }
  PyObject* script_flow_file = nullptr;
  if (!PyArg_ParseTuple(args, "O!", PyScriptFlowFile::typeObject(), &script_flow_file)) {
    return nullptr;
  }
  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  session->remove(flow_file);
  Py_RETURN_NONE;
}

PyObject* PyProcessSessionObject::read(PyProcessSessionObject* self, PyObject* args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    return nullptr;
  }

  PyObject* script_flow_file = nullptr;
  PyObject* callback = nullptr;
  if (!PyArg_ParseTuple(args, "O!O", PyScriptFlowFile::typeObject(), &script_flow_file, &callback)) {
    return nullptr;
  }

  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }
  session->read(flow_file, BorrowedObject(callback));
  Py_RETURN_NONE;
}

PyObject* PyProcessSessionObject::write(PyProcessSessionObject* self, PyObject* args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    return nullptr;
  }

  PyObject* script_flow_file = nullptr;
  PyObject* callback = nullptr;
  if (!PyArg_ParseTuple(args, "O!O", PyScriptFlowFile::typeObject(), &script_flow_file, &callback)) {
    return nullptr;
  }

  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }
  session->write(flow_file, BorrowedObject(callback));
  Py_RETURN_NONE;
}

PyObject* PyProcessSessionObject::transfer(PyProcessSessionObject* self, PyObject* args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    return nullptr;
  }

  PyObject* script_flow_file = nullptr;
  PyObject* relationship = nullptr;
  if (!PyArg_ParseTuple(args, "O!O!", PyScriptFlowFile::typeObject(), &script_flow_file, PyRelationship::typeObject(), &relationship)) {
    return nullptr;
  }

  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }
  session->transfer(flow_file, reinterpret_cast<PyRelationship*>(relationship)->relationship_);
  Py_RETURN_NONE;
}

PyObject* PyProcessSessionObject::transferToCustomRelationship(PyProcessSessionObject* self, PyObject* args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    return nullptr;
  }

  PyObject* script_flow_file = nullptr;
  const char* relationship_name = nullptr;
  if (!PyArg_ParseTuple(args, "O!s", PyScriptFlowFile::typeObject(), &script_flow_file, &relationship_name)) {
    return nullptr;
  }

  if (!relationship_name) {
    PyErr_SetString(PyExc_AttributeError, "Custom relationship name is invalid!");
    return nullptr;
  }

  std::string relationship_name_str(relationship_name);
  if (relationship_name_str.empty()) {
    PyErr_SetString(PyExc_AttributeError, "Custom relationship name is empty!");
    return nullptr;
  }

  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  BorrowedStr name = BorrowedStr::fromTuple(args, 0);
  session->transferToCustomRelationship(flow_file, relationship_name_str);
  Py_RETURN_NONE;
}

PyObject* PyProcessSessionObject::getContentsAsBytes(PyProcessSessionObject* self, PyObject* args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    return nullptr;
  }
  PyObject* script_flow_file = nullptr;
  if (!PyArg_ParseTuple(args, "O!", PyScriptFlowFile::typeObject(), &script_flow_file)) {
    return nullptr;
  }
  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  auto content = session->getContentsAsString(flow_file);

  return PyBytes_FromStringAndSize(content.c_str(), gsl::narrow<Py_ssize_t>(content.size()));
}

PyObject* PyProcessSessionObject::putAttribute(PyProcessSessionObject* self, PyObject* args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    return nullptr;
  }
  PyObject* script_flow_file = nullptr;
  const char* attribute_key = nullptr;
  const char* attribute_value = nullptr;
  if (!PyArg_ParseTuple(args, "O!ss", PyScriptFlowFile::typeObject(), &script_flow_file, &attribute_key, &attribute_value)) {
    return nullptr;
  }

  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  if (!attribute_key) {
    PyErr_SetString(PyExc_AttributeError, "Attribute key is invalid!");
    return nullptr;
  }

  std::string attribute_key_str(attribute_key);
  if (attribute_key_str.empty()) {
    PyErr_SetString(PyExc_AttributeError, "Attribute key is empty!");
    return nullptr;
  }

  if (!attribute_value) {
    PyErr_SetString(PyExc_AttributeError, "Attribute value is invalid!");
    return nullptr;
  }

  std::string attribute_value_str(attribute_value);

  session->putAttribute(flow_file, attribute_key_str, attribute_value_str);
  Py_RETURN_NONE;
}

PyTypeObject* PyProcessSessionObject::typeObject() {
  static OwnedObject PyProcessSessionObjectType{PyType_FromSpec(&PyProcessSessionObjectTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyProcessSessionObjectType.get());
}
}  // extern "C"

}  // namespace org::apache::nifi::minifi::extensions::python
