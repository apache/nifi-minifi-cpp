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

#ifndef NIFI_MINIFI_CPP_PYTHONPROCESSOR_H
#define NIFI_MINIFI_CPP_PYTHONPROCESSOR_H

#include <pybind11/embed.h>
#include <memory>

#include <core/ProcessSession.h>
#include <core/Processor.h>

#include "PyBaseStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace python {

namespace py = pybind11;

/**
 * Defines a reference to the processor.
 */
class PythonProcessor {
 public:
  explicit PythonProcessor(std::shared_ptr<core::Processor> proc);

  void setSupportsDynamicProperties();

  void setDecription(const std::string &desc);

  void addProperty(const std::string &name, const std::string &description, const std::string &defaultvalue, bool required, bool el);
  /**
   * Sometimes we want to release shared pointers to core resources when
   * we know they are no longer in need. This method is for those times.
   *
   * For example, we do not want to hold on to shared pointers to FlowFiles
   * after an onTrigger call, because doing so can be very expensive in terms
   * of repository resources.
   */
  void releaseCoreResources();

 private:

  std::shared_ptr<core::Processor> processor_;
};

} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_PYTHONPROCESSOR_H
