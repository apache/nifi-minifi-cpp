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

#ifndef WIN32
#if !defined(__APPLE__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE  // NOLINT: for RTLD_DEFAULT (source: `man dlsym` on linux)
#endif  // !__APPLE__ && !_GNU_SOURCE
// on Apple, RTLD_DEFAULT is defined without needing any macros (source: `man dlsym` on macOS)
#include <dlfcn.h>
#endif  // !WIN32

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

namespace {
// PyEval_InitThreads might be marked deprecated (depending on the version of Python.h)
// Python <= 3.6: This needs to be called manually after Py_Initialize to initialize threads (python < 3.6 is unsupported by us)
// Python >= 3.7: Noop function since its functionality is included in Py_Initialize
// Python >= 3.9: Marked as deprecated (still noop)
// Python >= 3.11: removed
// This can be removed if we drop the support for Python 3.6
void initThreads() {
  using namespace std::literals;
  // ignoring versions 3.60+, hopefully we can remove this hack by the time they are relevant
  // Example version string: "3.0a5+ (py3k:63103M, May 12 2008, 00:53:55) \n[GCC 4.2.3]"
  if (const auto version = Py_GetVersion(); std::string_view{version}.starts_with("3.6")) { return; }
#if defined(__clang__)
  #pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#ifndef WIN32  // dlsym hack, doesn't work on windows
  // dlsym hack: allows us to build with python 3.11+, where these were removed (so no header declarations), and run with python 3.6 (e.g. RHEL8)
  // the dlsym hack doesn't work on Windows, we'll drop python 3.6 support there
  // lowercase, to avoid name conflicts with the header declaration, in case we're using an old enough python to build
  const auto pyeval_threads_initialized = (int (*)())dlsym(RTLD_DEFAULT, "PyEval_ThreadsInitialized");  // NOLINT: C-style cast for POSIX-guaranteed dataptr -> funcptr conversion in dlsym
  const auto pyeval_initthreads = (void (*)())dlsym(RTLD_DEFAULT, "PyEval_InitThreads");  // NOLINT: C-style cast for POSIX-guaranteed dataptr -> funcptr conversion in dlsym
  gsl_Assert(pyeval_threads_initialized && pyeval_initthreads && "We're on python 3.6, yet we couldn't load PyEval_ThreadsInitialized and/or PyEval_InitThreads");
  if (!pyeval_threads_initialized())
    pyeval_initthreads();
#endif  // !WIN32
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
}

}  // namespace

Interpreter::Interpreter() {
  Py_Initialize();
  initThreads();
  PyInit_minifi_native();
  saved_thread_state_ = PyEval_SaveThread();  // NOLINT(cppcoreguidelines-prefer-member-initializer)
}

Interpreter::~Interpreter() {
  PyEval_RestoreThread(saved_thread_state_);
  Py_Finalize();
}

}  // namespace org::apache::nifi::minifi::extensions::python
