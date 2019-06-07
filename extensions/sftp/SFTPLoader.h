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
#ifndef EXTENSION_SFTPLOADER_H
#define EXTENSION_SFTPLOADER_H

#include "core/ClassLoader.h"
#include "processors/PutSFTP.h"
#include "processors/FetchSFTP.h"
#include "processors/ListSFTP.h"

class SFTPFactoryInitializer : public core::ObjectFactoryInitializer {
 public:
  virtual bool initialize();
  virtual void deinitialize();
};

class SFTPFactory : public core::ObjectFactory {
 public:
  SFTPFactory() {

  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() {
    return "SFTPFactory";
  }

  virtual std::string getClassName() {
    return "SFTPFactory";
  }
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() {
    std::vector<std::string> class_names;
    class_names.push_back("PutSFTP");
    return class_names;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) {
    if (utils::StringUtils::equalsIgnoreCase(class_name, "PutSFTP")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::PutSFTP>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "FetchSFTP")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::FetchSFTP>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "ListSFTP")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::ListSFTP>());
    } else {
      return nullptr;
    }
  }

  virtual std::unique_ptr<core::ObjectFactoryInitializer> getInitializer() {
    return std::unique_ptr<core::ObjectFactoryInitializer>(new SFTPFactoryInitializer());
  }

  static bool added;

};

extern "C" {
DLL_EXPORT void *createSFTPFactory(void);
}
#endif /* EXTENSION_SFTPLOADER_H */
