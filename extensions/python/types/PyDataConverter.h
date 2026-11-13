// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "../PythonBindings.h"

extern "C" {
namespace org::apache::nifi::minifi::extensions::python {

PyObject* timePeriodStringToMilliseconds(PyObject* self, PyObject* args);
PyObject* dataSizeStringToBytes(PyObject* self, PyObject* args);

}  // namespace org::apache::nifi::minifi::extensions::python
}  // extern "C"
