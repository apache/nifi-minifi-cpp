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
#ifndef EXTENSIONS_HTTPCURLLOADER_H_
#define EXTENSIONS_HTTPCURLLOADER_H_

#ifdef WIN32
#pragma comment(lib, "wldap32.lib" )
#pragma comment(lib, "crypt32.lib" )
#pragma comment(lib, "Ws2_32.lib")

#define CURL_STATICLIB 
#include <curl/curl.h>
#endif

#include "c2/protocols/RESTProtocol.h"
#include "protocols/RESTSender.h"
#include "processors/InvokeHTTP.h"
#include "client/HTTPClient.h"
#include "core/ClassLoader.h"
#include "sitetosite/HTTPProtocol.h"
#include "utils/StringUtils.h"

class HttpCurlObjectFactory : public core::ObjectFactory {
 public:
  HttpCurlObjectFactory() {

  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() override{
    return "HttpCurlObjectFactory";
  }

  virtual std::string getClassName() override{
    return "HttpCurlObjectFactory";
  }
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() override{
    std::vector<std::string> class_names;
    class_names.push_back("HttpProtocol");
    class_names.push_back("RESTSender");
    class_names.push_back("InvokeHTTP");
    class_names.push_back("HTTPClient");
    class_names.push_back("HttpSiteToSiteClient");
    return class_names;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) override{
    if (utils::StringUtils::equalsIgnoreCase(class_name, "RESTSender")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::c2::RESTSender>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "InvokeHTTP")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<processors::InvokeHTTP>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "HTTPClient")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<utils::HTTPClient>());
    } else if (utils::StringUtils::equalsIgnoreCase(class_name, "HttpProtocol") || utils::StringUtils::equalsIgnoreCase(class_name, "HttpSiteToSiteClient")) {
      return std::unique_ptr<ObjectFactory>(new core::DefautObjectFactory<minifi::sitetosite::HttpSiteToSiteClient>());
    } else {
      return nullptr;
    }
  }

  static bool added;

};

extern "C" {
	DLL_EXPORT void *createHttpCurlFactory(void);
}
#endif /* EXTENSIONS_HTTPCURLLOADER_H_ */
