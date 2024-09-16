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

#include "PyRecordSetWriter.h"

#include "PyScriptFlowFile.h"
#include "PyProcessSession.h"
#include "minifi-cpp/core/Record.h"
#include "rapidjson/document.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyRecordSetWriter_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"write", (PyCFunction) PyRecordSetWriter::write, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyRecordSetWriterTypeSpecSlots[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PyRecordSetWriter>)},
    {Py_tp_init, reinterpret_cast<void*>(PyRecordSetWriter::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyRecordSetWriter_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PyRecordSetWriter>)},
    {}  /* Sentinel */
};

static PyType_Spec PyRecordSetWriterTypeSpec{
    .name = "minifi_native.RecordSetWriter",
    .basicsize = sizeof(PyRecordSetWriter),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyRecordSetWriterTypeSpecSlots
};

int PyRecordSetWriter::init(PyRecordSetWriter* self, PyObject* args, PyObject*) {
  gsl_Expects(self && args);
  PyObject* weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto record_set_writer = PyCapsule_GetPointer(weak_ptr_capsule, HeldTypeName);
  if (!record_set_writer)
    return -1;
  self->record_set_writer_ = *static_cast<HeldType*>(record_set_writer);
  return 0;
}

PyObject* PyRecordSetWriter::write(PyRecordSetWriter* self, PyObject* args) {
  gsl_Expects(self && args);
  auto record_set_writer = self->record_set_writer_.lock();
  if (!record_set_writer) {
    PyErr_SetString(PyExc_AttributeError, "tried reading record set writer outside 'on_trigger'");
    return nullptr;
  }

  PyObject* py_recordset = nullptr;
  PyObject* script_flow_file = nullptr;
  PyObject* py_session = nullptr;
  if (!PyArg_ParseTuple(args, "OO!O!", &py_recordset, PyScriptFlowFile::typeObject(), &script_flow_file, PyProcessSessionObject::typeObject(), &py_session)) {
    return nullptr;
  }

  if (!py_recordset) {
    PyErr_SetString(PyExc_AttributeError, "Recordset is invalid!");
    return nullptr;
  }

  const auto flow_file = reinterpret_cast<PyScriptFlowFile*>(script_flow_file)->script_flow_file_.lock();
  if (!flow_file) {
    PyErr_SetString(PyExc_AttributeError, "tried reading FlowFile outside 'on_trigger'");
    return nullptr;
  }

  const auto process_session = reinterpret_cast<PyProcessSessionObject*>(py_session)->process_session_.lock();
  if (!process_session) {
    PyErr_SetString(PyExc_AttributeError, "tried reading ProcessSession outside 'on_trigger'");
    return nullptr;
  }

  auto record_list = BorrowedList(py_recordset);
  std::vector<core::Record> record_set;
  for (size_t i = 0; i < record_list.length(); ++i) {
    auto record_json_str = BorrowedStr(record_list[i].get()).toUtf8String();
    rapidjson::Document document;
    document.Parse<0>(record_json_str.c_str());
    record_set.push_back(core::Record::fromJson(document));
  }

  record_set_writer->write(record_set, flow_file, process_session->getSession());
  Py_RETURN_NONE;
}

PyTypeObject* PyRecordSetWriter::typeObject() {
  static OwnedObject PyRecordSetWriterType{PyType_FromSpec(&PyRecordSetWriterTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyRecordSetWriterType.get());
}

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
