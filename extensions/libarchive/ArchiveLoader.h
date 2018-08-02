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
#include "ManipulateArchive.h"
#include "core/ClassLoader.h"

class ArchiveFactory : public core::ObjectFactory {
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
    class_names.push_back("ManipulateArchive");
    return class_names;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) {
    if (utils::StringUtils::equalsIgnoreCase(class_name, "MergeContent")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::MergeContent>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "CompressContent")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::CompressContent>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name,"FocusArchiveEntry")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::FocusArchiveEntry>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name,"UnfocusArchiveEntry")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::UnfocusArchiveEntry>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name,"ManipulateArchive")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::ManipulateArchive>());
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
