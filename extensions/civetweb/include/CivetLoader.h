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
#ifndef EXTENSION_CIVETLOADER_H
#define EXTENSION_CIVETLOADER_H

#include "core/ClassLoader.h"
#include "c2/protocols/RESTReceiver.h"
#include "processors/ListenHTTP.h"

class __attribute__((visibility("default"))) CivetFactory : public core::ObjectFactory {
 public:
  CivetFactory() {

  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() {
    return "CivetFactory";
  }

  virtual std::string getClassName() {
    return "CivetFactory";
  }
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() {
    std::vector<std::string> class_names;
    class_names.push_back("RESTReceiver");
    class_names.push_back("ListenHTTP");
    return class_names;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) {
    if (utils::StringUtils::equalsIgnoreCase(class_name, "RESTReceiver")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::c2::RESTReceiver>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "ListenHTTP")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<processors::ListenHTTP>());
    } else {
      return nullptr;
    }
  }

  static bool added;

};

extern "C" {
void *createCivetFactory(void);
}
#endif /* EXTENSION_CIVETLOADER_H */
