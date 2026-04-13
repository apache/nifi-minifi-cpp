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

#include "PyDataConverter.h"

#include "core/TypedValues.h"

namespace org::apache::nifi::minifi::extensions::python {

PyObject* timePeriodStringToMilliseconds(PyObject* /*self*/, PyObject* args) {
  const char* time_period_str = nullptr;
  if (!PyArg_ParseTuple(args, "s", &time_period_str)) {
    return nullptr;
  }

  auto milliseconds = core::TimePeriodValue(std::string(time_period_str)).getMilliseconds().count();

  return object::returnReference(milliseconds);
}

PyObject* dataSizeStringToBytes(PyObject* /*self*/, PyObject* args) {
  const char* data_size_str = nullptr;
  if (!PyArg_ParseTuple(args, "s", &data_size_str)) {
    return nullptr;
  }

  uint64_t bytes = core::DataSizeValue(std::string(data_size_str)).getValue();

  return object::returnReference(bytes);
}

}  // namespace org::apache::nifi::minifi::extensions::python
