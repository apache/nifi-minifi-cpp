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
#ifndef EXTENSION_MQTTLOADER_H
#define EXTENSION_MQTTLOADER_H

#include "controllerservice/MQTTControllerService.h"
#include "processors/PublishMQTT.h"
#include "processors/ConsumeMQTT.h"
#include "MQTTC2Protocol.h"
#include "core/ClassLoader.h"
#include "ConvertHeartBeat.h"
#include "ConvertJSONAck.h"
#include "ConvertUpdate.h"
class MQTTFactory : public core::ObjectFactory {
 public:
  MQTTFactory() {

  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() {
    return "MQTTFactory";
  }

  virtual std::string getClassName() {
    return "MQTTFactory";
  }
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() {
    std::vector<std::string> class_names;
    class_names.push_back("PublishMQTT");
    class_names.push_back("ConsumeMQTT");
    class_names.push_back("MQTTContextService");
    class_names.push_back("MQTTC2Protocol");
    class_names.push_back("ConvertHeartBeat");
    class_names.push_back("ConvertJSONAck");
    class_names.push_back("ConvertUpdate");
    return class_names;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) {
    if (utils::StringUtils::equalsIgnoreCase(class_name, "PublishMQTT")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::PublishMQTT>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "ConsumeMQTT")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::ConsumeMQTT>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "MQTTContextService")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::controllers::MQTTControllerService>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "MQTTC2Protocol")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::c2::MQTTC2Protocol>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "ConvertHeartBeat")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::ConvertHeartBeat>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "ConvertJSONAck")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::ConvertJSONAck>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "ConvertUpdate")) {
          return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::processors::ConvertUpdate>());
    } else {
      return nullptr;
    }
  }

  static bool added;

};

extern "C" {
DLL_EXPORT void *createMQTTFactory(void);
}
#endif /* EXTENSION_MQTTLOADER_H */
