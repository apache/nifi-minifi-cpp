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

#include "HttpConfigurationListener.h"
#include "FlowController.h"
#include <curl/curlbuild.h>
#include <curl/easy.h>
#include <iostream>
#include <iterator>
#include <string>
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


bool HttpConfigurationListener::pullConfiguration(std::string &configuration) {

  if (url_.empty())
    return false;

  bool ret = false;

  std::string fullUrl = url_ + "?" + "flowName=" + controller_->getName() + "&flowVersion=" +
      std::to_string(controller_->getVersion()) + "&serialNumber=" + controller_->getSerialNumberStr();

  CURL *http_session = curl_easy_init();

  curl_easy_setopt(http_session, CURLOPT_URL, fullUrl.c_str());

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

  CURLcode res = curl_easy_perform(http_session);

  if (res == CURLE_OK) {
    logger_->log_debug("HttpConfigurationListener -- curl successful to %s", fullUrl.c_str());

    std::string response_body(content.data.begin(), content.data.end());
    int64_t http_code = 0;
    curl_easy_getinfo(http_session, CURLINFO_RESPONSE_CODE, &http_code);
    char *content_type;
    /* ask for the content-type */
    curl_easy_getinfo(http_session, CURLINFO_CONTENT_TYPE, &content_type);

    bool isSuccess = ((int32_t) (http_code / 100)) == 2
        && res != CURLE_ABORTED_BY_CALLBACK;
    bool body_empty = IsNullOrEmpty(content.data);

    if (isSuccess && !body_empty) {
      configuration = std::move(response_body);
      ret = true;
    } else {
      logger_->log_error("Cannot output body to content");
    }
  } else {
    logger_->log_error("HttpConfigurationListener -- curl_easy_perform() failed %s\n",
                       curl_easy_strerror(res));
  }
  curl_easy_cleanup(http_session);

  return ret;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
