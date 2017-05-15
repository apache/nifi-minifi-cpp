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

#include "core/HttpConfigurationListener.h"
#include <curl/curlbuild.h>
#include <curl/easy.h>
#include <uuid/uuid.h>
#include <memory>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "io/DataStream.h"
#include "io/StreamFactory.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

const char *HttpConfigurationListener::ProcessorName = "HttpConfigurationListener";

const char* HttpConfigurationListener::STATUS_CODE = "invokehttp.status.code";
const char* HttpConfigurationListener::STATUS_MESSAGE = "invokehttp.status.message";
const char* HttpConfigurationListener::RESPONSE_BODY = "invokehttp.response.body";
const char* HttpConfigurationListener::REQUEST_URL = "invokehttp.request.url";
const char* HttpConfigurationListener::TRANSACTION_ID = "invokehttp.tx.id";
const char* HttpConfigurationListener::REMOTE_DN = "invokehttp.remote.dn";
const char* HttpConfigurationListener::EXCEPTION_CLASS = "invokehttp.java.exception.class";
const char* HttpConfigurationListener::EXCEPTION_MESSAGE = "invokehttp.java.exception.message";

void HttpConfigurationListener::set_request_method(CURL *curl, const std::string &method) {
  std::string my_method = method;
  std::transform(my_method.begin(), my_method.end(), my_method.begin(), ::toupper);
  if (my_method == "POST") {
    curl_easy_setopt(curl, CURLOPT_POST, 1);
  } else if (my_method == "PUT") {
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
  } else if (my_method == "GET") {
  } else {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, my_method.c_str());
  }
}

void HttpConfigurationListener::initialize() {
  logger_->log_info("Initializing HttpConfigurationListener");

}

void HttpConfigurationListener::onSchedule(core::ProcessContext *context,
                            core::ProcessSessionFactory *sessionFactory) {

}

HttpConfigurationListener::~HttpConfigurationListener() {
  curl_global_cleanup();
}

std::string HttpConfigurationListener::generateId() {
  uuid_t txId;
  uuid_generate(txId);
  char uuidStr[37];
  uuid_unparse_lower(txId, uuidStr);
  return uuidStr;
}

void HttpConfigurationListener::onTrigger(core::ProcessContext *context,
                           core::ProcessSession *session) {
  std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<
      FlowFileRecord>(session->get());

  logger_->log_info("onTrigger HttpConfigurationListener with  %s", method_.c_str());

  // create a transaction id
  std::string tx_id = generateId();

  CURL *http_session = curl_easy_init();
  // set the HTTP request method from libCURL
  set_request_method(http_session, method_);
  curl_easy_setopt(http_session, CURLOPT_URL, url_.c_str());

  if (connect_timeout_ > 0) {
    curl_easy_setopt(http_session, CURLOPT_TIMEOUT, connect_timeout_);
  }

  if (read_timeout_ > 0) {
    curl_easy_setopt(http_session, CURLOPT_TIMEOUT, read_timeout_);
  }
  HTTPRequestResponse content;
  curl_easy_setopt(http_session, CURLOPT_WRITEFUNCTION,
                   &HTTPRequestResponse::recieve_write);

  curl_easy_setopt(http_session, CURLOPT_WRITEDATA,
                   static_cast<void*>(&content));

  logger_->log_info("HttpConfigurationListener -- curl performed");
  res = curl_easy_perform(http_session);

  if (res == CURLE_OK) {
    logger_->log_info("HttpConfigurationListener -- curl successful");

    std::string response_body(content.data.begin(), content.data.end());
    int64_t http_code = 0;
    curl_easy_getinfo(http_session, CURLINFO_RESPONSE_CODE, &http_code);
    char *content_type;
    /* ask for the content-type */
    curl_easy_getinfo(http_session, CURLINFO_CONTENT_TYPE, &content_type);

    bool isSuccess = ((int32_t) (http_code / 100)) == 2
        && res != CURLE_ABORTED_BY_CALLBACK;
    bool output_body_to_content = isSuccess;
    bool body_empty = IsNullOrEmpty(content.data);

    logger_->log_info("isSuccess: %d", isSuccess);

    if (output_body_to_content) {
    } else {
      logger_->log_info("Cannot output body to content");
    }
  } else {
    logger_->log_error("HttpConfigurationListener -- curl_easy_perform() failed %s\n",
                       curl_easy_strerror(res));
  }
  curl_easy_cleanup(http_session);
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
