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
#include "PythonInterpreter.h"

#include "PythonBindings.h"

namespace org::apache::nifi::minifi::extensions::python {

Interpreter* Interpreter::getInterpreter() {
  static Interpreter interpreter;
  return &interpreter;
}

GlobalInterpreterLock::GlobalInterpreterLock()
    : gil_state_(PyGILState_Ensure()) {
}

GlobalInterpreterLock::~GlobalInterpreterLock() {
  PyGILState_Release(gil_state_);
}

Interpreter::Interpreter() {
  Py_Initialize();
  PyInit_minifi_native();
  saved_thread_state_ = PyEval_SaveThread();  // NOLINT(cppcoreguidelines-prefer-member-initializer)
}

Interpreter::~Interpreter() {
  PyEval_RestoreThread(saved_thread_state_);
  Py_Finalize();
}

}  // namespace org::apache::nifi::minifi::extensions::python
