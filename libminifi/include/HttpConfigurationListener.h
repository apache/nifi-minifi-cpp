/**
 * HttpConfigurationListener class declaration
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
#ifndef __HTTP_CONFIGURATION_LISTENER__
#define __HTTP_CONFIGURATION_LISTENER__

#include <curl/curl.h>
#include "core/Core.h"
#include "core/Property.h"
#include "ConfigurationListener.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
/**
 * HTTP Response object
 */
struct HTTPRequestResponse {
  std::vector<char> data;

  /**
   * Receive HTTP Response.
   */
  static size_t recieve_write(char * data, size_t size, size_t nmemb,
      void * p) {
    return static_cast<HTTPRequestResponse*>(p)->write_content(data, size,
        nmemb);
  }

  size_t write_content(char* ptr, size_t size, size_t nmemb) {
    data.insert(data.end(), ptr, ptr + size * nmemb);
    return size * nmemb;
  }

};

// HttpConfigurationListener Class
class HttpConfigurationListener: public ConfigurationListener {
public:

  // Constructor
  /*!
   * Create a new processor
   */
  HttpConfigurationListener(FlowController *controller,
      std::shared_ptr<Configure> configure) :
      minifi::ConfigurationListener(controller, configure, "http") {
      std::string value;

      if (configure->get(Configure::nifi_configuration_listener_http_url, value)) {
        url_ = value;
        logger_->log_info("Http configuration listener URL %s", url_.c_str());
      } else {
        url_ = "";
      }

      curl_global_init(CURL_GLOBAL_DEFAULT);
      this->start();
  }

  bool pullConfiguration(std::string &configuration);

  /**
    * Configures a secure connection
    */
  void configureSecureConnection(CURL *http_session);

  static CURLcode configureSSLContext(CURL *curl, void *ctx, void *param);
  static int pemPassWordCb(char *buf, int size, int rwflag, void *param);

  // Destructor
  virtual ~HttpConfigurationListener() {
    this->stop();
    curl_global_cleanup();
  }

protected:

};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
