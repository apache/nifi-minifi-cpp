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
#ifndef EXTENSION_ARCHIVELOADER_H
#define EXTENSION_ARCHIVELOADER_H

#include "MergeContent.h"
#include "CompressContent.h"
#include "FocusArchiveEntry.h"
#include "UnfocusArchiveEntry.h"
#include "core/ClassLoader.h"

class __attribute__((visibility("default"))) ArchiveFactory : public core::ObjectFactory {
 public:
  ArchiveFactory() {

  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() {
    return "ArchiveFactory";
  }

  virtual std::string getClassName() {
    return "ArchiveFactory";
  }
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() {
    std::vector<std::string> class_names;
    class_names.push_back("MergeContent");
    class_names.push_back("CompressContent");
    class_names.push_back("FocusArchiveEntry");
    class_names.push_back("UnfocusArchiveEntry");
    return class_names;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) {
    std::string name = class_name;
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    if (name == "mergecontent") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::MergeContent>());
    } else if (name == "compresscontent") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::CompressContent>());
    } else if (name == "focusarchiveentry") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::FocusArchiveEntry>());
    } else if (name == "unfocusarchiveentry") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::UnfocusArchiveEntry>());
    } else {
      return nullptr;
    }
  }

  static bool added;

};

extern "C" {
void *createArchiveFactory(void);
}
#endif /* EXTENSION_ARCHIVELOADER_H */
