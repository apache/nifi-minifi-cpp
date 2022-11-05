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

#include "PythonBindings.h"
#include "../core/Relationship.h"

namespace org::apache::nifi::minifi::python {

struct PyRelationship {
  using Relationship = org::apache::nifi::minifi::core::Relationship;
  using HeldType = Relationship;

  PyObject_HEAD
  HeldType relationship_;

  static PyObject *newInstance(PyTypeObject *type, PyObject *args, PyObject *kwds);
  static int init(PyRelationship *self, PyObject *args, PyObject *kwds);
  static void dealloc(PyRelationship *self);

  static PyObject *getName(PyRelationship *self, PyObject *args);
  static PyObject *getDescription(PyRelationship *self, PyObject *args);

  static PyTypeObject *typeObject();
};

namespace object {
template <>
struct Converter<PyRelationship::HeldType> : public HolderTypeConverter<PyRelationship> {};
}
}  // namespace org::apache::nifi::minifi::python
