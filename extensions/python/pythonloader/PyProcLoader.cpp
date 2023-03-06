/**
 *
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
#include "core/extension/Extension.h"
#include "PythonCreator.h"
#include "PythonBindings.h"

namespace minifi = org::apache::nifi::minifi;

static minifi::extensions::python::PythonCreator& getPythonCreator() {
  static minifi::extensions::python::PythonCreator instance("PythonCreator");
  return instance;
}

static bool init(const std::shared_ptr<minifi::Configure>& config) {
  getPythonCreator().configure(config);
  return true;
}

static void deinit() {
  // TODO(adebreceni): deinitialization is not implemented
}

// request this module (shared library) to be opened into the global namespace, exposing
// the symbols of the python library
extern "C" const int LOAD_MODULE_AS_GLOBAL = 1;

REGISTER_EXTENSION("PythonExtension", init, deinit);
