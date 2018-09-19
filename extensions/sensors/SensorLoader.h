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

#include "core/ClassLoader.h"
#include "GetEnvironmentalSensors.h"
#include "GetMovementSensors.h"
#include "utils/StringUtils.h"

class SensorFactory : public core::ObjectFactory {
 public:
  SensorFactory() {

  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() {
    return "SensorFactory";
  }

  virtual std::string getClassName() {
    return "SensorFactory";
  }
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() {
    std::vector<std::string> class_names;
    class_names.push_back("EnvironmentalSensors");
    return class_names;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) {
    if (minifi::utils::StringUtils::equalsIgnoreCase(class_name, "GetEnvironmentalSensors")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::GetEnvironmentalSensors>());
    } else if (minifi::utils::StringUtils::equalsIgnoreCase(class_name, "GetMovementSensors")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::GetMovementSensors>());
    } else {
      return nullptr;
    }
  }

  static bool added;

}
;

extern "C" {
DLL_EXPORT void *createSensorFactory(void);
}
#endif /* EXTENSIONS_ROCKSDBLOADER_H */
