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
#ifndef BUILD_DESCRPTION_H
#define BUILD_DESCRPTION_H

#include <vector>
#include "capi/expect.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {



struct ClassDescription {
  explicit ClassDescription(std::string name)
      : class_name_(name),
        support_dynamic_(false) {
  }
  explicit ClassDescription(std::string name, std::map<std::string, std::string> props, bool dyn)
      : class_name_(name),
        class_properties_(props),
        support_dynamic_(dyn) {

  }
  std::string class_name_;
  std::map<std::string, std::string> class_properties_;
  bool support_dynamic_;
};

struct Components {
  std::vector<ClassDescription> processors_;
  std::vector<ClassDescription> controller_services_;
  std::vector<ClassDescription> other_components_;
};

class BuildDescription {
 public:

  static struct Components getClassDescriptions() {
    static struct Components classes;
    if (UNLIKELY(IsNullOrEmpty(classes.processors_) && IsNullOrEmpty(classes.controller_services_))) {
      for (auto clazz : core::ClassLoader::getDefaultClassLoader().getClasses()) {

        auto lastOfIdx = clazz.find_last_of("::");
        if (lastOfIdx != std::string::npos) {
          lastOfIdx++;  // if a value is found, increment to move beyond the .
          int nameLength = clazz.length() - lastOfIdx;
          std::string class_name = clazz.substr(lastOfIdx, nameLength);

          auto obj = core::ClassLoader::getDefaultClassLoader().instantiate(class_name, class_name);

          std::shared_ptr<core::ConfigurableComponent> component = std::dynamic_pointer_cast<core::ConfigurableComponent>(obj);

          ClassDescription description(clazz);
          if (nullptr != component) {

            bool is_processor = std::dynamic_pointer_cast<core::Processor>(obj) != nullptr;
            bool is_controller_service = LIKELY(is_processor == true) ? false : std::dynamic_pointer_cast<core::controller::ControllerService>(obj) != nullptr;

            component->initialize();
            description.class_properties_ = component->getProperties();
            description.support_dynamic_ = component->supportsDynamicProperties();
            if (is_processor) {
              classes.processors_.emplace_back(description);
            } else if (is_controller_service) {
              classes.controller_services_.emplace_back(description);
            } else {
              classes.other_components_.emplace_back(description);
            }
          }
        }
      }
    }
    return classes;
  }

};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* BUILD_DESCRPTION_H */
