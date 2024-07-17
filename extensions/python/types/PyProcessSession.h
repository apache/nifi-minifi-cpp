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

#pragma once
#include <vector>
#include <memory>

#include "core/ProcessSession.h"
#include "../PythonBindings.h"

namespace org::apache::nifi::minifi::extensions::python {

class PyProcessSession {
 public:
  explicit PyProcessSession(gsl::not_null<core::ProcessSession*> session);

  std::shared_ptr<core::FlowFile> get();
  std::shared_ptr<core::FlowFile> create(const std::shared_ptr<core::FlowFile>& flow_file = nullptr);
  std::shared_ptr<core::FlowFile> clone(const std::shared_ptr<core::FlowFile>& flow_file);
  void transfer(const std::shared_ptr<core::FlowFile>& flow_file, const core::Relationship& relationship);
  void transferToCustomRelationship(const std::shared_ptr<core::FlowFile>& flow_file, const std::string& relationship_name);
  void read(const std::shared_ptr<core::FlowFile>& flow_file, BorrowedObject input_stream_callback);
  void write(const std::shared_ptr<core::FlowFile>& flow_file, BorrowedObject output_stream_callback);
  void remove(const std::shared_ptr<core::FlowFile>& flow_file);
  std::string getContentsAsString(const std::shared_ptr<core::FlowFile>& flow_file);

 private:
  std::vector<std::shared_ptr<core::FlowFile>> flow_files_;
  gsl::not_null<core::ProcessSession*> session_;
};

struct PyProcessSessionObject {
  PyProcessSessionObject() {}
  using HeldType = std::weak_ptr<PyProcessSession>;
  static constexpr const char* HeldTypeName = "PyProcessSessionObject::HeldType";

  PyObject_HEAD
  HeldType process_session_;

  static int init(PyProcessSessionObject* self, PyObject* args, PyObject* kwds);

  static PyObject* get(PyProcessSessionObject* self, PyObject* args);
  static PyObject* create(PyProcessSessionObject* self, PyObject* args);
  static PyObject* clone(PyProcessSessionObject* self, PyObject* args);
  static PyObject* read(PyProcessSessionObject* self, PyObject* args);
  static PyObject* write(PyProcessSessionObject* self, PyObject* args);
  static PyObject* transfer(PyProcessSessionObject* self, PyObject* args);
  static PyObject* transferToCustomRelationship(PyProcessSessionObject* self, PyObject* args);
  static PyObject* remove(PyProcessSessionObject* self, PyObject* args);
  static PyObject* getContentsAsBytes(PyProcessSessionObject* self, PyObject* args);

  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyProcessSessionObject::HeldType> : public HolderTypeConverter<PyProcessSessionObject> {};
}  // namespace object
}  // namespace org::apache::nifi::minifi::extensions::python
