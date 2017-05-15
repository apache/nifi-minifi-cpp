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

#include <memory>

#include <curl/curl.h>
#include "FlowController.h"
#include "core/Processor.h"
#include "core/Core.h"
#include "core/Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

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
class HttpConfigurationListener : public core::Processor {
 public:

  // Constructor
  /*!
   * Create a new processor
   */
  HttpConfigurationListener(std::string name, uuid_t uuid = NULL)
      : Processor(name, uuid),
        connect_timeout_(20000),
        read_timeout_(20000),
        use_etag_(false) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
  }
  // Destructor
  virtual ~HttpConfigurationListener();
  // Processor Name
  static const char *ProcessorName;

  static const char* STATUS_CODE;
  static const char* STATUS_MESSAGE;
  static const char* RESPONSE_BODY;
  static const char* REQUEST_URL;
  static const char* TRANSACTION_ID;
  static const char* REMOTE_DN;
  static const char* EXCEPTION_CLASS;
  static const char* EXCEPTION_MESSAGE;

  void onTrigger(core::ProcessContext *context, core::ProcessSession *session);
  void initialize();
  void onSchedule(core::ProcessContext *context,
                  core::ProcessSessionFactory *sessionFactory);

 protected:

  /**
   * Generate a transaction ID
   * @return transaction ID string.
   */
  std::string generateId();
  /**
   * Set the request method on the curl struct.
   * @param curl pointer to this instance.
   * @param string request method
   */
  void set_request_method(CURL *curl, const std::string &);

  CURLcode res;
  // http method
  std::string method_;
  // url
  std::string url_;
  // connection timeout
  int64_t connect_timeout_;
  // read timeout.
  int64_t read_timeout_;
  bool use_etag_;
  std::string last_etag_;
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
