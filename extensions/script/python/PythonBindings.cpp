#include "PythonBindings.h"
#include "PyLogger.h"
#include "PyProcessSession.h"
#include "PyProcessContext.h"
#include "PyProcessor.h"
#include "PyScriptFlowFile.h"
#include "PyRelationship.h"
#include "PyInputStream.h"
#include "PyOutputStream.h"

namespace org::apache::nifi::minifi::python {
extern "C" {

struct PyModuleDef minifiModule = {
  PyModuleDef_HEAD_INIT,
  .m_name = "minifi_native",   /* name of module */
  .m_doc = nullptr, /* module documentation, may be NULL */
  .m_size = -1,       /* size of per-interpreter state of the module, or -1 if the module keeps state in global variables. */
};

PyMODINIT_FUNC
PyInit_minifi_native(void) {
  const std::array<std::pair<PyTypeObject*, std::string_view>, 8> types = {
    std::make_pair(PyLogger::typeObject(), "Logger"),
    std::make_pair(PyProcessSessionObject::typeObject(), "ProcessSession"),
    std::make_pair(PyProcessContext::typeObject(), "ProcessContext"),
    std::make_pair(PyProcessor::typeObject(), "Processor"),
    std::make_pair(PyScriptFlowFile::typeObject(), "FlowFile"),
    std::make_pair(PyRelationship::typeObject(), "Relationship"),
    std::make_pair(PyInputStream::typeObject(), "InputStream"),
    std::make_pair(PyOutputStream::typeObject(), "OutputStream")
  };

  for (auto type : types) {
    if (PyType_Ready(type.first) < 0) {
      return nullptr;
    }
  }

  auto minifiModuleInstance = PyModule_Create(&minifiModule);
  if (minifiModuleInstance == nullptr) {
      return nullptr;
  }

  for (auto type : types) {
    Py_INCREF(type.first);
  }
  const auto result = std::all_of(std::begin(types), std::end(types), [&](std::pair<PyTypeObject*, std::string_view> type) {
    return PyModule_AddObject(minifiModuleInstance, type.second.data(), reinterpret_cast<PyObject*>(type.first)) == 0;
  });

  if (!result) {
    for (auto type : types) {
      Py_DECREF(type.first);
    }
    Py_DECREF(minifiModuleInstance);
    return nullptr;
  }

  return minifiModuleInstance;
}

} // extern "C"
} // namespace org::apache::nifi::minifi::python

// PYBIND11_EMBEDDED_MODULE(minifi_native, m) { // NOLINT
  // namespace py = pybind11;
//   namespace python = org::apache::nifi::minifi::python;
//   namespace script = org::apache::nifi::minifi::script;
//   typedef org::apache::nifi::minifi::core::logging::Logger Logger;

  // py::class_<Logger, std::shared_ptr<Logger>>(m, "Logger")
//       .def("error", &Logger::log_error<>)
//       .def("warn", &Logger::log_warn<>)
//       .def("info", &Logger::log_info<>)
//       .def("debug", &Logger::log_debug<>)
//       .def("trace", &Logger::log_trace<>);

//   py::class_<python::PyProcessSession, std::shared_ptr<python::PyProcessSession>>(m, "ProcessSession")
//       .def("get", &python::PyProcessSession::get, py::return_value_policy::reference)
//       // .def("create",
//           //  static_cast<std::shared_ptr<script::ScriptFlowFile> (python::PyProcessSession::*)()>(&python::PyProcessSession::create))
//       .def("create",
//            static_cast<std::shared_ptr<script::ScriptFlowFile> (python::PyProcessSession::*)(const std::shared_ptr<script::ScriptFlowFile>&)>(&python::PyProcessSession::create))
//       .def("read", &python::PyProcessSession::read)
//       .def("write", &python::PyProcessSession::write)
//       .def("transfer", &python::PyProcessSession::transfer);

//   py::class_<python::PythonProcessor, std::shared_ptr<python::PythonProcessor>>(m, "Processor")
//         .def("setSupportsDynamicProperties", &python::PythonProcessor::setSupportsDynamicProperties)
//         .def("setDescription", &python::PythonProcessor::setDecription)
//         .def("addProperty", &python::PythonProcessor::addProperty);

//   py::class_<script::ScriptProcessContext, std::shared_ptr<script::ScriptProcessContext>>(m, "ProcessContext")
//       .def("getProperty", &script::ScriptProcessContext::getProperty);

//   py::class_<script::ScriptFlowFile, std::shared_ptr<script::ScriptFlowFile>>(m, "FlowFile")
//       .def("getAttribute", &script::ScriptFlowFile::getAttribute)
//       .def("addAttribute", &script::ScriptFlowFile::addAttribute)
//       .def("updateAttribute", &script::ScriptFlowFile::updateAttribute)
//       .def("removeAttribute", &script::ScriptFlowFile::removeAttribute)
//       .def("setAttribute", &script::ScriptFlowFile::setAttribute);

//   py::class_<org::apache::nifi::minifi::core::Relationship>(m, "Relationship")
//       .def("getName", &org::apache::nifi::minifi::core::Relationship::getName)
//       .def("getDescription", &org::apache::nifi::minifi::core::Relationship::getDescription);

//   py::class_<python::PyInputStream, std::shared_ptr<python::PyInputStream>>(m, "InputStream")
//       .def("read", static_cast<py::bytes (python::PyInputStream::*)()>(&python::PyInputStream::read))
//       .def("read", static_cast<py::bytes (python::PyInputStream::*)(size_t)>(&python::PyInputStream::read));

//   py::class_<python::PyOutputStream, std::shared_ptr<python::PyOutputStream>>(m, "OutputStream")
//       .def("write", &python::PyOutputStream::write);
// }
