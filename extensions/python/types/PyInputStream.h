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
#include <memory>

#include "../PythonBindings.h"
#include "minifi-cpp/io/InputStream.h"

namespace org::apache::nifi::minifi::extensions::python {

struct PyInputStream {
  PyInputStream() {}
  using HeldType = std::weak_ptr<io::InputStream>;
  static constexpr const char* HeldTypeName = "PyInputStream::HeldType";

  PyObject_HEAD
  HeldType input_stream_;

  static int init(PyInputStream* self, PyObject* args, PyObject* kwds);
  static PyObject* read(PyInputStream* self, PyObject* args);
  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PyInputStream::HeldType> : public HolderTypeConverter<PyInputStream> {};
}
}  // namespace org::apache::nifi::minifi::extensions::python
