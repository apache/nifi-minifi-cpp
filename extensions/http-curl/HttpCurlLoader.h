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
#ifndef EXTENSIONS_HTTPCURLLOADER_H
#define EXTENSIONS_HTTPCURLLOADER_H

#include "protocols/RESTProtocol.h"
#include "protocols/RESTSender.h"
#include "protocols/RESTReceiver.h"
#include "processors/InvokeHTTP.h"
#include "client/HTTPClient.h"
#include "core/ClassLoader.h"

class __attribute__((visibility("default"))) HttpCurlObjectFactory : public core::ObjectFactory {
 public:
  HttpCurlObjectFactory() {

  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() {
    return "HttpCurlObjectFactory";
  }

  virtual std::string getClassName() {
    return "HttpCurlObjectFactory";
  }
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() {
    std::vector<std::string> class_names;
    class_names.push_back("RESTProtocol");
    class_names.push_back("RESTReceiver");
    class_names.push_back("RESTSender");
    class_names.push_back("InvokeHTTP");
    class_names.push_back("HTTPClient");
    return class_names;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) {
    if (class_name == "RESTReceiver") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::c2::RESTReceiver>());
    } else if (class_name == "RESTSender") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::c2::RESTSender>());
    } else if (class_name == "InvokeHTTP") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<processors::InvokeHTTP>());
    } else if (class_name == "HTTPClient") {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<utils::HTTPClient>());
    } else {
      return nullptr;
    }
  }

  static bool added;



};

extern "C" {
void *createHttpCurlFactory(void);
}
#endif /* EXTENSIONS_HTTPCURLLOADER_H */
