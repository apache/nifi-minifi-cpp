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

#include "PyRecordSetReader.h"
#include "rapidjson/writer.h"
#include "rapidjson/stream.h"

#include "PyProcessSession.h"
#include "PyScriptFlowFile.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PyRecordSetReader_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"read", (PyCFunction) PyRecordSetReader::read, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PyRecordSetReaderTypeSpecSlots[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PyRecordSetReader>)},
    {Py_tp_init, reinterpret_cast<void*>(PyRecordSetReader::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PyRecordSetReader_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PyRecordSetReader>)},
    {}  /* Sentinel */
};

static PyType_Spec PyRecordSetReaderTypeSpec{
    .name = "minifi_native.RecordSetReader",
    .basicsize = sizeof(PyRecordSetReader),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PyRecordSetReaderTypeSpecSlots
};

int PyRecordSetReader::init(PyRecordSetReader* self, PyObject* args, PyObject*) {
  gsl_Expects(self && args);
  PyObject* weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto record_set_reader = PyCapsule_GetPointer(weak_ptr_capsule, HeldTypeName);
  if (!record_set_reader)
    return -1;
  self->record_set_reader_ = *static_cast<HeldType*>(record_set_reader);
  return 0;
}

PyObject* PyRecordSetReader::read(PyRecordSetReader* self, PyObject* args) {
  gsl_Expects(self && args);
  auto record_set_reader = self->record_set_reader_.lock();
  if (!record_set_reader) {
    PyErr_SetString(PyExc_AttributeError, "tried reading record set reader outside 'on_trigger'");
    return nullptr;
  }

  PyObject* script_flow_file = nullptr;
  PyObject* py_session = nullptr;
  if (!PyArg_ParseTuple(args, "O!O!", PyScriptFlowFile::typeObject(), &script_flow_file, PyProcessSessionObject::typeObject(), &py_session)) {
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

  nonstd::expected<core::RecordSet, std::error_code> read_result;
  process_session->getSession().read(flow_file, [&record_set_reader, &read_result](const std::shared_ptr<io::InputStream>& input_stream) {
    read_result = record_set_reader->read(*input_stream);
    return gsl::narrow<int64_t>(input_stream->size());
  });

  if  (!read_result) {
    std::string error_message = "failed to read record set with the following error: " + read_result.error().message();
    PyErr_SetString(PyExc_RuntimeError, error_message.c_str());
    return nullptr;
  }

  auto records = OwnedList::create();
  for (const auto& record : read_result.value()) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    record.toJson().Accept(writer);
    records.append(std::string{buffer.GetString(), buffer.GetSize()});
  }
  return object::returnReference(records);
}

PyTypeObject* PyRecordSetReader::typeObject() {
  static OwnedObject PyRecordSetReaderType{PyType_FromSpec(&PyRecordSetReaderTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PyRecordSetReaderType.get());
}

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
