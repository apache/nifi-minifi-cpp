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

#include <memory>

#include <pybind11/embed.h>
#include "ExecutePythonProcessor.h"
#include "PythonProcessor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace python {

namespace py = pybind11;
namespace core = org::apache::nifi::minifi::core;

PythonProcessor::PythonProcessor(std::shared_ptr<core::Processor> proc) {
  processor_ = std::dynamic_pointer_cast<python::processors::ExecutePythonProcessor>(proc);
}

void PythonProcessor::setSupportsDynamicProperties() {
  if (!processor_) {
    throw std::runtime_error("Access of Processor after it has been released");
  }

  std::dynamic_pointer_cast<python::processors::ExecutePythonProcessor>(processor_)->setSupportsDynamicProperties();
}

void PythonProcessor::setDecription(const std::string &desc) {
  if (!processor_) {
    throw std::runtime_error("Access of Processor after it has been released");
  }

  std::dynamic_pointer_cast<python::processors::ExecutePythonProcessor>(processor_)->setDescription(desc);
}

void PythonProcessor::addProperty(const std::string &name, const std::string &description, const std::string &defaultvalue, bool required, bool el) {
  std::dynamic_pointer_cast<python::processors::ExecutePythonProcessor>(processor_)->addProperty(name, description, defaultvalue, required, el);
}

void PythonProcessor::releaseCoreResources() {
  processor_.reset();
}
} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
