// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "PyDataConverter.h"

#include "core/TypedValues.h"

namespace org::apache::nifi::minifi::extensions::python {

PyObject* timePeriodStringToMilliseconds(PyObject* /*self*/, PyObject* args) {
  const char* time_period_str = nullptr;
  if (!PyArg_ParseTuple(args, "s", &time_period_str)) {
    return nullptr;
  }

  auto milliseconds = core::TimePeriodValue(std::string(time_period_str)).getMilliseconds().count();

  return object::returnReference(milliseconds);
}

PyObject* dataSizeStringToBytes(PyObject* /*self*/, PyObject* args) {
  const char* data_size_str = nullptr;
  if (!PyArg_ParseTuple(args, "s", &data_size_str)) {
    return nullptr;
  }

  uint64_t bytes = core::DataSizeValue(std::string(data_size_str)).getValue();

  return object::returnReference(bytes);
}

}  // namespace org::apache::nifi::minifi::extensions::python
