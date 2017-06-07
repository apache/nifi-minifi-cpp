/**
 * HTTPUtils class declaration
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
#ifndef __HTTP_UTILS_H__
#define __HTTP_UTILS_H__

#include <curl/curl.h>
#include <vector>
#include "ByteInputCallBack.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

struct CallBackPosition {
  ByteInputCallBack *ptr;
  size_t pos;
};

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

  /**
   * Callback for post, put, and patch operations
   * @param buffer
   * @param size size of buffer
   * @param nitems items to add
   * @param insteam input stream object.
   */

  static size_t send_write(char * data, size_t size, size_t nmemb, void * p) {
    if (p != 0) {
      CallBackPosition *callback = (CallBackPosition*) p;
      if (callback->pos <= callback->ptr->getBufferSize()) {
        char *ptr = callback->ptr->getBuffer();
        int len = callback->ptr->getBufferSize() - callback->pos;
        if (len <= 0) {
          delete callback->ptr;
          delete callback;
          return 0;
        }
        if (len > size * nmemb)
          len = size * nmemb;
        memcpy(data, callback->ptr->getBuffer() + callback->pos, len);
        callback->pos += len;
        return len;
      }
    } else {
      return CURL_READFUNC_ABORT;
    }

    return 0;
  }

  size_t write_content(char* ptr, size_t size, size_t nmemb) {
    data.insert(data.end(), ptr, ptr + size * nmemb);
    return size * nmemb;
  }

};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
