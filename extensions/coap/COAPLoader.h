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
#ifndef EXTENSIONS_COAPLOADER_H_
#define EXTENSIONS_COAPLOADER_H_

#ifdef WIN32
#pragma comment(lib, "wldap32.lib" )
#pragma comment(lib, "crypt32.lib" )
#pragma comment(lib, "Ws2_32.lib")
#endif

#include "core/ClassLoader.h"
#include "utils/StringUtils.h"
#include "protocols/CoapC2Protocol.h"

/**
 * Object factory class loader for this extension.
 * Can add extensions to the default class loader through REGISTER_RESOURCE,
 * but we want to ensure this factory is used specifically for CoAP and not the default loader.
 */
class COAPObjectFactory : public core::ObjectFactory {
 public:
  COAPObjectFactory() {

  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() override{
    return "COAPObjectFactory";
  }

  virtual std::string getClassName() override{
    return "COAPObjectFactory";
  }
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() override{
    std::vector<std::string> class_names;
    class_names.push_back("CoapProtocol");
    class_names.push_back("CoapConnectorService");
    return class_names;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) override{
    if (utils::StringUtils::equalsIgnoreCase(class_name, "CoapProtocol")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::coap::c2::CoapProtocol>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "CoapConnectorService")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::coap::controllers::CoapConnectorService>());
    } else {
      return nullptr;
    }
  }

  static bool added;

};

extern "C" {
	DLL_EXPORT void *createCOAPFactory(void);
}
#endif /* EXTENSIONS_COAPLOADER_H_ */
