// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "Python.h"

namespace org::apache::nifi::minifi::extensions::python {

class GlobalInterpreterLock {
 public:
  GlobalInterpreterLock();
  ~GlobalInterpreterLock();

 private:
  PyGILState_STATE gil_state_;
};

class Interpreter {
  Interpreter();
  ~Interpreter();

 public:
  static Interpreter* getInterpreter();

  Interpreter(const Interpreter& other) = delete;
  Interpreter(Interpreter&& other) = delete;
  Interpreter& operator=(const Interpreter& other) = delete;
  Interpreter& operator=(Interpreter&& other) = delete;

 public:
  PyThreadState* saved_thread_state_ = nullptr;
};

}  // namespace org::apache::nifi::minifi::extensions::python
