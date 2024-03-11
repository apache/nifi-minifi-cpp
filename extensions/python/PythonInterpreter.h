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
