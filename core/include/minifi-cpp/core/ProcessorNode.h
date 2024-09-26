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

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ConfigurableComponent.h"
#include "Connectable.h"
#include "Property.h"

namespace org::apache::nifi::minifi::core {

/**
 * Processor node functions as a pass through to the implementing Connectables
 */
class ProcessorNode : public virtual ConfigurableComponent, public virtual Connectable {
 public:
  virtual Connectable* getProcessor() const = 0;

  template<typename T>
  bool getProperty(const std::string &name, T &value) {
    const auto processor_cast = dynamic_cast<ConfigurableComponent*>(getProcessor());
    if (nullptr != processor_cast) {
      return processor_cast->getProperty<T>(name, value);
    } else {
      return ConfigurableComponent::getProperty<T>(name, value);
    }
  }
};

}  // namespace org::apache::nifi::minifi::core
