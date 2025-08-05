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
#pragma once

#include <memory>

#include "../PythonBindings.h"
#include "controllers/SSLContextServiceInterface.h"

namespace org::apache::nifi::minifi::extensions::python {

struct PySSLContextService {
  PySSLContextService() {}
  using HeldType = std::weak_ptr<controllers::SSLContextServiceInterface>;
  static constexpr const char* HeldTypeName = "PySSLContextService::HeldType";

  PyObject_HEAD
  HeldType ssl_context_service_;

  static int init(PySSLContextService* self, PyObject* args, PyObject* kwds);

  static PyObject* getCertificateFile(PySSLContextService* self, PyObject* args);
  static PyObject* getPassphrase(PySSLContextService* self, PyObject* args);
  static PyObject* getPrivateKeyFile(PySSLContextService* self, PyObject* args);
  static PyObject* getCACertificate(PySSLContextService* self, PyObject* args);

  static PyTypeObject* typeObject();
};

namespace object {
template<>
struct Converter<PySSLContextService::HeldType> : public HolderTypeConverter<PySSLContextService> {};
}  // namespace object
}  // namespace org::apache::nifi::minifi::extensions::python
