// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <string>

#include "PyException.h"
#include "types/Types.h"

namespace org::apache::nifi::minifi::extensions::python {

PyException::PyException()
    : std::runtime_error(exceptionString()) {
  PyErr_Fetch(type_.asOutParameter(), value_.asOutParameter(), traceback_.asOutParameter());
}

std::string PyException::exceptionString() {
  BorrowedReference type;
  BorrowedReference value;
  BorrowedReference traceback;
  PyErr_Fetch(type.asOutParameter(), value.asOutParameter(), traceback.asOutParameter());
  PyErr_NormalizeException(type.asOutParameter(), value.asOutParameter(), traceback.asOutParameter());
  auto format_exception = OwnedModule::import("traceback").getFunction("format_exception");
  auto exception_string_list = List(format_exception(type, value, traceback));
  std::string error_string;
  if (exception_string_list) {
    for (size_t i = 0; i < exception_string_list.length(); ++i) {
      error_string += Str(exception_string_list[i]).toUtf8String();
    }
  } else {
    PyErr_Clear();
    error_string += BorrowedStr(PyObject_GetAttrString(type.get(), "__name__")).toUtf8String();
    error_string += ": ";
    error_string += BorrowedStr(PyObject_Str(value.get())).toUtf8String();
  }

  PyErr_Restore(type.get(), value.get(), traceback.get());
  return error_string;
}

}  // namespace org::apache::nifi::minifi::extensions::python
