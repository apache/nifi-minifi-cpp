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

#ifndef NIFI_MINIFI_CPP_PYTHONBINDINGS_H
#define NIFI_MINIFI_CPP_PYTHONBINDINGS_H

#include <pybind11/embed.h>

#include <core/ProcessSession.h>
#include <core/logging/LoggerConfiguration.h>

#include "../ScriptProcessContext.h"

#include "PyProcessSession.h"
#include "PythonProcessor.h"
#include "PyBaseStream.h"

PYBIND11_EMBEDDED_MODULE(minifi_native, m) { // NOLINT
  namespace py = pybind11;
  namespace python = org::apache::nifi::minifi::python;
  namespace script = org::apache::nifi::minifi::script;
  typedef core::logging::Logger Logger;

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
           static_cast<std::shared_ptr<script::ScriptFlowFile> (python::PyProcessSession::*)(std::shared_ptr<script::ScriptFlowFile>)>(&python::PyProcessSession::create))
      .def("read", &python::PyProcessSession::read)
      .def("write", &python::PyProcessSession::write)
      .def("transfer", &python::PyProcessSession::transfer);

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
      .def("removeAttribute", &script::ScriptFlowFile::removeAttribute);

  py::class_<core::Relationship>(m, "Relationship")
      .def("getName", &core::Relationship::getName)
      .def("getDescription", &core::Relationship::getDescription);

  py::class_<python::PyBaseStream, std::shared_ptr<python::PyBaseStream>>(m, "BaseStream")
      .def("read", static_cast<py::bytes (python::PyBaseStream::*)()>(&python::PyBaseStream::read))
      .def("read", static_cast<py::bytes (python::PyBaseStream::*)(size_t)>(&python::PyBaseStream::read))
      .def("write", &python::PyBaseStream::write);
}

#endif //NIFI_MINIFI_CPP_PYTHONBINDINGS_H
