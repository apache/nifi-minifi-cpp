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
#include "PyException.h"
#include "PyScriptFlowFile.h"
#include "PyRelationship.h"
#include "types/PyOutputStream.h"
#include "types/PyInputStream.h"
#include "range/v3/algorithm/remove_if.hpp"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::extensions::python {

namespace core = org::apache::nifi::minifi::core;

PyProcessSession::PyProcessSession(std::shared_ptr<core::ProcessSession> session)
    : session_(std::move(session)) {
}

std::shared_ptr<core::FlowFile> PyProcessSession::get() {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = session_->get();

  if (flow_file == nullptr) {
    return nullptr;
  }

  flow_files_.push_back(flow_file);

  return flow_file;
}

void PyProcessSession::transfer(const std::shared_ptr<core::FlowFile>& flow_file,
                                const core::Relationship& relationship) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_->transfer(flow_file, relationship);
}

void PyProcessSession::read(const std::shared_ptr<core::FlowFile>& flow_file, BorrowedObject input_stream_callback) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_->read(flow_file, [&input_stream_callback](const std::shared_ptr<io::InputStream>& input_stream) -> int64_t {
    return Long(Callable(input_stream_callback.getAttribute("process"))(std::weak_ptr(input_stream))).asInt64();
  });
}

void PyProcessSession::write(const std::shared_ptr<core::FlowFile>& flow_file, BorrowedObject output_stream_callback) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_->write(flow_file, [&output_stream_callback](const std::shared_ptr<io::OutputStream>& output_stream) -> int64_t {
    return Long(Callable(output_stream_callback.getAttribute("process"))(std::weak_ptr(output_stream))).asInt64();
  });
}

std::shared_ptr<core::FlowFile> PyProcessSession::create(const std::shared_ptr<core::FlowFile>& flow_file) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto result = session_->create(flow_file.get());

  flow_files_.push_back(result);
  return result;
}

std::shared_ptr<core::FlowFile> PyProcessSession::clone(const std::shared_ptr<core::FlowFile>& flow_file) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  if (!flow_file) {
    throw std::runtime_error("Flow file to clone is nullptr");
  }

  auto result = session_->clone(*flow_file);

  flow_files_.push_back(result);
  return result;
}

void PyProcessSession::remove(const std::shared_ptr<core::FlowFile>& flow_file) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }
  std::shared_ptr<core::FlowFile> result;

  session_->remove(flow_file);
  flow_files_.erase(ranges::remove_if(flow_files_, [&flow_file](const auto& ff)-> bool { return ff == flow_file; }), flow_files_.end());
}

std::string PyProcessSession::getContentsAsString(const std::shared_ptr<core::FlowFile>& flow_file) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  std::string content;
  session_->read(flow_file, [&content](const std::shared_ptr<io::InputStream>& input_stream) -> int64_t {
    content.resize(input_stream->size());
    return gsl::narrow<int64_t>(input_stream->read(as_writable_bytes(std::span(content))));
  });
  return content;
}

extern "C" {

static PyMethodDef PyProcessSessionObject_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"get", (PyCFunction) PyProcessSessionObject::get, METH_NOARGS, nullptr},
    {"create", (PyCFunction) PyProcessSessionObject::create, METH_VARARGS, nullptr},
    {"clone", (PyCFunction) PyProcessSessionObject::clone, METH_VARARGS, nullptr},
    {"read", (PyCFunction) PyProcessSessionObject::read, METH_VARARGS, nullptr},
    {"write", (PyCFunction) PyProcessSessionObject::write, METH_VARARGS, nullptr},
    {"transfer", (PyCFunction) PyProcessSessionObject::transfer, METH_VARARGS, nullptr},
    {"remove", (PyCFunction) PyProcessSessionObject::remove, METH_VARARGS, nullptr},
    {"getContentsAsBytes", (PyCFunction) PyProcessSessionObject::getContentsAsBytes, METH_VARARGS, nullptr},
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
    throw PyException();
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

PyObject* PyProcessSessionObject::create(PyProcessSessionObject* self, PyObject*) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    return nullptr;
  }

  if (auto flow_file = session->create(nullptr))
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
    throw PyException();
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
    throw PyException();
  }
  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  session->remove(flow_file);
  Py_RETURN_NONE;
}

PyObject* PyProcessSessionObject::read(PyProcessSessionObject* self, PyObject* args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    Py_RETURN_NONE;
  }

  PyObject* script_flow_file = nullptr;
  PyObject* callback = nullptr;
  if (!PyArg_ParseTuple(args, "O!O", PyScriptFlowFile::typeObject(), &script_flow_file, &callback)) {
    throw PyException();
  }

  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    Py_RETURN_NONE;
  }
  session->read(flow_file, BorrowedObject(callback));
  Py_RETURN_NONE;
}

PyObject* PyProcessSessionObject::write(PyProcessSessionObject* self, PyObject* args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    Py_RETURN_NONE;
  }

  PyObject* script_flow_file = nullptr;
  PyObject* callback = nullptr;
  if (!PyArg_ParseTuple(args, "O!O", PyScriptFlowFile::typeObject(), &script_flow_file, &callback)) {
    throw PyException();
  }

  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    Py_RETURN_NONE;
  }
  session->write(flow_file, BorrowedObject(callback));
  Py_RETURN_NONE;
}

PyObject* PyProcessSessionObject::transfer(PyProcessSessionObject* self, PyObject* args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    Py_RETURN_NONE;
  }

  PyObject* script_flow_file = nullptr;
  PyObject* relationship = nullptr;
  if (!PyArg_ParseTuple(args, "O!O!", PyScriptFlowFile::typeObject(), &script_flow_file, PyRelationship::typeObject(), &relationship)) {
    throw PyException();
  }

  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    Py_RETURN_NONE;
  }
  session->transfer(flow_file, reinterpret_cast<PyRelationship*>(relationship)->relationship_);
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
    throw PyException();
  }
  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  auto content = session->getContentsAsString(flow_file);

  return PyBytes_FromStringAndSize(content.c_str(), gsl::narrow<Py_ssize_t>(content.size()));
}

PyTypeObject* PyProcessSessionObject::typeObject() {
  static OwnedObject PyProcessSessionObjectType{PyType_FromSpec(&PyProcessSessionObjectTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyProcessSessionObjectType.get());
}
}  // extern "C"

}  // namespace org::apache::nifi::minifi::extensions::python
