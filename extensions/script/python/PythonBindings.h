/**
 * @file PythonBindings.h

 * Python C++ type bindings
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

#pragma once

#include <memory>

#include "pybind11/embed.h"
#include "core/ProcessSession.h"

#include "../ScriptProcessContext.h"

#include "PyProcessSession.h"
#include "PythonProcessor.h"
#include "PyInputStream.h"
#include "PyOutputStream.h"

PYBIND11_EMBEDDED_MODULE(minifi_native, m) { // NOLINT
  namespace py = pybind11;
  namespace python = org::apache::nifi::minifi::python;
  namespace script = org::apache::nifi::minifi::script;
  typedef org::apache::nifi::minifi::core::logging::Logger Logger;

  py::class_<Logger, std::shared_ptr<Logger>>(m, "Logger")
      .def("error", &Logger::log_error<>)
      .def("warn", &Logger::log_warn<>)
      .def("info", &Logger::log_info<>)
      .def("debug", &Logger::log_debug<>)
      .def("trace", &Logger::log_trace<>);

  py::class_<python::PyProcessSession, std::shared_ptr<python::PyProcessSession>>(m, "ProcessSession")
      .def("get", &python::PyProcessSession::get, py::return_value_policy::reference)
      .def("create",
           static_cast<std::shared_ptr<script::ScriptFlowFile> (python::PyProcessSession::*)()>(&python::PyProcessSession::create))
      .def("create",
           static_cast<std::shared_ptr<script::ScriptFlowFile> (python::PyProcessSession::*)(const std::shared_ptr<script::ScriptFlowFile>&)>(&python::PyProcessSession::create))
      .def("read", &python::PyProcessSession::read)
      .def("write", &python::PyProcessSession::write)
      .def("transfer", &python::PyProcessSession::transfer)
      .def("remove", &python::PyProcessSession::remove);

  py::class_<python::PythonProcessor, std::shared_ptr<python::PythonProcessor>>(m, "Processor")
        .def("setSupportsDynamicProperties", &python::PythonProcessor::setSupportsDynamicProperties)
        .def("setDescription", &python::PythonProcessor::setDecription)
        .def("addProperty", &python::PythonProcessor::addProperty);

  py::class_<script::ScriptProcessContext, std::shared_ptr<script::ScriptProcessContext>>(m, "ProcessContext")
      .def("getProperty", &script::ScriptProcessContext::getProperty);

  py::class_<script::ScriptFlowFile, std::shared_ptr<script::ScriptFlowFile>>(m, "FlowFile")
      .def("getAttribute", &script::ScriptFlowFile::getAttribute)
      .def("addAttribute", &script::ScriptFlowFile::addAttribute)
      .def("updateAttribute", &script::ScriptFlowFile::updateAttribute)
      .def("removeAttribute", &script::ScriptFlowFile::removeAttribute)
      .def("setAttribute", &script::ScriptFlowFile::setAttribute);

  py::class_<org::apache::nifi::minifi::core::Relationship>(m, "Relationship")
      .def("getName", &org::apache::nifi::minifi::core::Relationship::getName)
      .def("getDescription", &org::apache::nifi::minifi::core::Relationship::getDescription);

  py::class_<python::PyInputStream, std::shared_ptr<python::PyInputStream>>(m, "InputStream")
      .def("read", static_cast<py::bytes (python::PyInputStream::*)()>(&python::PyInputStream::read))
      .def("read", static_cast<py::bytes (python::PyInputStream::*)(size_t)>(&python::PyInputStream::read));

  py::class_<python::PyOutputStream, std::shared_ptr<python::PyOutputStream>>(m, "OutputStream")
      .def("write", &python::PyOutputStream::write);
}
