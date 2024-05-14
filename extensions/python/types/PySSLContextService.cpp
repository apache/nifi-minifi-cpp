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

#include "PySSLContextService.h"
#include <string>
#include "PyException.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

static PyMethodDef PySSLContextService_methods[] = {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    {"getCertificateFile", (PyCFunction) PySSLContextService::getCertificateFile, METH_VARARGS, nullptr},
    {"getPassphrase", (PyCFunction) PySSLContextService::getPassphrase, METH_VARARGS, nullptr},
    {"getPrivateKeyFile", (PyCFunction) PySSLContextService::getPrivateKeyFile, METH_VARARGS, nullptr},
    {"getCACertificate", (PyCFunction) PySSLContextService::getCACertificate, METH_VARARGS, nullptr},
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
    throw PyException();
  self->ssl_context_service_ = *static_cast<HeldType*>(ssl_context_service);
  return 0;
}

PyObject* PySSLContextService::getCertificateFile(PySSLContextService* self, PyObject* /*args*/) {
  auto ssl_context_service = self->ssl_context_service_.lock();
  if (!ssl_context_service) {
    PyErr_SetString(PyExc_AttributeError, "tried reading ssl context service outside 'on_trigger'");
    Py_RETURN_NONE;
  }
  return object::returnReference(ssl_context_service->getCertificateFile().string());
}

PyObject* PySSLContextService::getPassphrase(PySSLContextService* self, PyObject* /*args*/) {
  auto ssl_context_service = self->ssl_context_service_.lock();
  if (!ssl_context_service) {
    PyErr_SetString(PyExc_AttributeError, "tried reading ssl context service outside 'on_trigger'");
    Py_RETURN_NONE;
  }
  return object::returnReference(ssl_context_service->getPassphrase());
}

PyObject* PySSLContextService::getPrivateKeyFile(PySSLContextService* self, PyObject* /*args*/) {
  auto ssl_context_service = self->ssl_context_service_.lock();
  if (!ssl_context_service) {
    PyErr_SetString(PyExc_AttributeError, "tried reading ssl context service outside 'on_trigger'");
    Py_RETURN_NONE;
  }
  return object::returnReference(ssl_context_service->getPrivateKeyFile().string());
}

PyObject* PySSLContextService::getCACertificate(PySSLContextService* self, PyObject* /*args*/) {
  auto ssl_context_service = self->ssl_context_service_.lock();
  if (!ssl_context_service) {
    PyErr_SetString(PyExc_AttributeError, "tried reading ssl context service outside 'on_trigger'");
    Py_RETURN_NONE;
  }
  return object::returnReference(ssl_context_service->getCACertificate().string());
}

PyTypeObject* PySSLContextService::typeObject() {
  static OwnedObject PySSLContextServiceType{PyType_FromSpec(&PySSLContextServiceTypeSpec)};
  return reinterpret_cast<PyTypeObject*>(PySSLContextServiceType.get());
}

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
