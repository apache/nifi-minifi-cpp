// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "../PythonBindings.h"
#include "minifi-cpp/controllers/SSLContextServiceInterface.h"

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
