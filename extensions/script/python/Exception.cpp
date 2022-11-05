#include <string>

#include "Exception.h"
#include "Types.h"

namespace org::apache::nifi::minifi::python {

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
  auto format_exc = OwnedModule::import("traceback").getFunction("format_exception");
  auto exception_string_list = List(format_exc(type, value, traceback));
  std::string error_string;
  if (exception_string_list) {
    for (size_t i = 0; i < exception_string_list.length(); ++i) {
      error_string += Str(exception_string_list[i]).toUtf8String();
    }
  }

  PyErr_Restore(type.get(), value.get(), traceback.get());
  return error_string;
}

} // namespace org::apache::nifi::minifi::python