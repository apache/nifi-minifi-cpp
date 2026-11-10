// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "PySSLContextService.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PySSLContextService_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"getCertificateFile", safePyFunction<PySSLContextService::getCertificateFile>, METH_VARARGS, nullptr},
    {"getPassphrase", safePyFunction<PySSLContextService::getPassphrase>, METH_VARARGS, nullptr},
    {"getPrivateKeyFile", safePyFunction<PySSLContextService::getPrivateKeyFile>, METH_VARARGS, nullptr},
    {"getCACertificate", safePyFunction<PySSLContextService::getCACertificate>, METH_VARARGS, nullptr},
    {}  /* Sentinel */
};

static PyType_Slot PySSLContextServiceTypeSpecSlots[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {Py_tp_dealloc, reinterpret_cast<void*>(pythonAllocatedInstanceDealloc<PySSLContextService>)},
    {Py_tp_init, reinterpret_cast<void*>(PySSLContextService::init)},
    {Py_tp_methods, reinterpret_cast<void*>(PySSLContextService_methods)},
    {Py_tp_new, reinterpret_cast<void*>(newPythonAllocatedInstance<PySSLContextService>)},
    {}  /* Sentinel */
};

static PyType_Spec PySSLContextServiceTypeSpec{
    .name = "minifi_native.SSLContextService",
    .basicsize = sizeof(PySSLContextService),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = PySSLContextServiceTypeSpecSlots
};

int PySSLContextService::init(PySSLContextService* self, PyObject* args, PyObject*) {
  PyObject* weak_ptr_capsule = nullptr;
  if (!PyArg_ParseTuple(args, "O", &weak_ptr_capsule)) {
    return -1;
  }

  auto ssl_context_service = PyCapsule_GetPointer(weak_ptr_capsule, HeldTypeName);
  if (!ssl_context_service)
    return -1;
  self->ssl_context_service_ = *static_cast<HeldType*>(ssl_context_service);
  return 0;
}

PyObject* PySSLContextService::getCertificateFile(PySSLContextService* self, PyObject* /*args*/) {
  auto ssl_context_service = self->ssl_context_service_.lock();
  if (!ssl_context_service) {
    PyErr_SetString(PyExc_AttributeError, "tried reading ssl context service outside 'on_trigger'");
    return nullptr;
  }
  return object::returnReference(ssl_context_service->getCertificateFile().string());
}

PyObject* PySSLContextService::getPassphrase(PySSLContextService* self, PyObject* /*args*/) {
  auto ssl_context_service = self->ssl_context_service_.lock();
  if (!ssl_context_service) {
    PyErr_SetString(PyExc_AttributeError, "tried reading ssl context service outside 'on_trigger'");
    return nullptr;
  }
  return object::returnReference(ssl_context_service->getPassphrase());
}

PyObject* PySSLContextService::getPrivateKeyFile(PySSLContextService* self, PyObject* /*args*/) {
  auto ssl_context_service = self->ssl_context_service_.lock();
  if (!ssl_context_service) {
    PyErr_SetString(PyExc_AttributeError, "tried reading ssl context service outside 'on_trigger'");
    return nullptr;
  }
  return object::returnReference(ssl_context_service->getPrivateKeyFile().string());
}

PyObject* PySSLContextService::getCACertificate(PySSLContextService* self, PyObject* /*args*/) {
  auto ssl_context_service = self->ssl_context_service_.lock();
  if (!ssl_context_service) {
    PyErr_SetString(PyExc_AttributeError, "tried reading ssl context service outside 'on_trigger'");
    return nullptr;
  }
  return object::returnReference(ssl_context_service->getCACertificate().string());
}

PyTypeObject* PySSLContextService::typeObject() {
  static OwnedObject PySSLContextServiceType{PyType_FromSpec(&PySSLContextServiceTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PySSLContextServiceType.get());
}

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
