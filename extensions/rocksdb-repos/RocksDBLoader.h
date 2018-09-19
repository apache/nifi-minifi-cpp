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
#ifndef EXTENSIONS_ROCKSDBLOADER_H
#define EXTENSIONS_ROCKSDBLOADER_H

#include "DatabaseContentRepository.h"
#include "FlowFileRepository.h"
#include "ProvenanceRepository.h"
#include "RocksDbStream.h"
#include "core/ClassLoader.h"

class RocksDBFactory : public core::ObjectFactory {
 public:
  RocksDBFactory() {

  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() {
    return "RocksDBFactory";
  }

  virtual std::string getClassName() {
    return "RocksDBFactory";
  }
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() {
    std::vector<std::string> class_names;
    class_names.push_back("DatabaseContentRepository");
    class_names.push_back("FlowFileRepository");
    class_names.push_back("ProvenanceRepository");
    class_names.push_back("databasecontentrepository");
    class_names.push_back("flowfilerepository");
    class_names.push_back("provenancerepository");
    return class_names;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) {
    std::string name = class_name;
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    if (name == "databasecontentrepository") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<core::repository::DatabaseContentRepository>());
    } else if (name == "flowfilerepository") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<core::repository::FlowFileRepository>());
    } else if (name == "provenancerepository") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::provenance::ProvenanceRepository>());
    } else {
      return nullptr;
    }
  }

  static bool added;

};

extern "C" {
DLL_EXPORT void *createRocksDBFactory(void);
}
#endif /* EXTENSIONS_ROCKSDBLOADER_H */
