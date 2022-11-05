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
#include "Exception.h"
#include "PyScriptFlowFile.h"
#include "PyRelationship.h"
#include "PyOutputStream.h"
#include "PyInputStream.h"

namespace org::apache::nifi::minifi::python {

namespace core = org::apache::nifi::minifi::core;

PyProcessSession::PyProcessSession(std::shared_ptr<core::ProcessSession> session)
    : session_(std::move(session)) {
}

std::shared_ptr<script::ScriptFlowFile> PyProcessSession::get() {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = session_->get();

  if (flow_file == nullptr) {
    return nullptr;
  }

  auto result = std::make_shared<script::ScriptFlowFile>(flow_file);
  flow_files_.push_back(result);

  return result;
}

void PyProcessSession::transfer(const std::shared_ptr<script::ScriptFlowFile>& script_flow_file,
                                const core::Relationship& relationship) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_->transfer(flow_file, relationship);
}

void PyProcessSession::read(const std::shared_ptr<script::ScriptFlowFile>& script_flow_file, BorrowedObject input_stream_callback) {
    if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_->read(flow_file, [&input_stream_callback](const std::shared_ptr<io::InputStream>& input_stream) -> int64_t {
    return Long(Callable(input_stream_callback.getAttribute("process"))(std::weak_ptr(input_stream))).asInt64();
  });
}

void PyProcessSession::write(const std::shared_ptr<script::ScriptFlowFile>& script_flow_file, BorrowedObject output_stream_callback) {
    if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_->write(flow_file, [&output_stream_callback](const std::shared_ptr<io::OutputStream>& output_stream) -> int64_t {
    return Long(Callable(output_stream_callback.getAttribute("process"))(std::weak_ptr(output_stream))).asInt64();
  });
}

std::shared_ptr<script::ScriptFlowFile> PyProcessSession::create(const std::shared_ptr<script::ScriptFlowFile>& flow_file) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  std::shared_ptr<script::ScriptFlowFile> result;

  if (flow_file == nullptr) {
    result = std::make_shared<script::ScriptFlowFile>(session_->create());
  } else {
    result = std::make_shared<script::ScriptFlowFile>(session_->create(flow_file->getFlowFile()));
  }

  flow_files_.push_back(result);
  return result;
}

void PyProcessSession::releaseCoreResources() {
  for (const auto &flow_file : flow_files_) {
    if (flow_file) {
      flow_file->releaseFlowFile();
    }
  }

  session_.reset();
}

extern "C" {

static PyMethodDef PyProcessSessionObject_methods[] = {
  { "get", (PyCFunction) PyProcessSessionObject::get, METH_VARARGS, nullptr },
  { "create", (PyCFunction) PyProcessSessionObject::create, METH_VARARGS, nullptr },
  { "read", (PyCFunction) PyProcessSessionObject::read, METH_VARARGS, nullptr },
  { "write", (PyCFunction) PyProcessSessionObject::write, METH_VARARGS, nullptr },
  { "transfer", (PyCFunction) PyProcessSessionObject::transfer, METH_VARARGS, nullptr },
  { nullptr }  /* Sentinel */
};

static PyTypeObject PyProcessSessionObjectType = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  .tp_name = "minifi_native.ProcessSession",
  .tp_doc = nullptr,
  .tp_basicsize = sizeof(PyProcessSessionObject),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_new = PyProcessSessionObject::newInstance,
  .tp_init = reinterpret_cast<initproc>(PyProcessSessionObject::init),
  .tp_dealloc = reinterpret_cast<destructor>(PyProcessSessionObject::dealloc),
  .tp_methods = PyProcessSessionObject_methods
};

PyObject *PyProcessSessionObject::newInstance(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  auto self = reinterpret_cast<PyProcessSessionObject*>(type->tp_alloc(type, 0));
  if (self == nullptr) {
    return nullptr;
  }

  self->process_session_.reset();
  return reinterpret_cast<PyObject*>(self);
}

int PyProcessSessionObject::init(PyProcessSessionObject *self, PyObject *args, PyObject *kwds) {
  PyObject *weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto process_session = static_cast<std::weak_ptr<PyProcessSession>*>(PyCapsule_GetPointer(weak_ptr_capsule, nullptr));
  // Py_DECREF(weak_ptr_capsule);
  self->process_session_ = *process_session;
  return 0;
}

void PyProcessSessionObject::dealloc(PyProcessSessionObject *self) {
  self->process_session_.reset();
}

PyObject *PyProcessSessionObject::get(PyProcessSessionObject *self) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    return nullptr;
  }
  return object::returnReference(std::weak_ptr(session->get()));
}

PyObject *PyProcessSessionObject::create(PyProcessSessionObject *self, PyObject *args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    return nullptr;
  };
  return object::returnReference(std::weak_ptr(session->create(nullptr)));
}

PyObject *PyProcessSessionObject::read(PyProcessSessionObject *self, PyObject *args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    Py_RETURN_NONE;
  }

  PyObject *script_flow_file;
  PyObject *callback;
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

PyObject *PyProcessSessionObject::write(PyProcessSessionObject *self, PyObject *args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    Py_RETURN_NONE;
  }

  PyObject *script_flow_file;
  PyObject *callback;
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

PyObject *PyProcessSessionObject::transfer(PyProcessSessionObject* self, PyObject *args) {
  auto session = self->process_session_.lock();
  if (!session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading process session outside 'on_trigger'");
    Py_RETURN_NONE;
  }

  PyObject *script_flow_file;
  PyObject *relationship;
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

PyTypeObject *PyProcessSessionObject::typeObject() {
  return &PyProcessSessionObjectType;
}

/* TODO(mzink)
 * void PyProcessSession::remove(const std::shared_ptr<script::ScriptFlowFile>& script_flow_file) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_->remove(flow_file);
}
 *
 *
 *
 *
 *
 *
 * */
} // extern "C"

}  // namespace org::apache::nifi::minifi::python
