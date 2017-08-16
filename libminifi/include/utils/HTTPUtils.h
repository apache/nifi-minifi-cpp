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
#include <iostream>
#include <string>
#include <curl/easy.h>
#include <openssl/ssl.h>
#include "ByteInputCallBack.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "properties/Configure.h"
#include "io/validation.h"

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

static void parse_url(std::string &url, std::string &host, int &port, std::string &protocol) {

  std::string http("http://");
  std::string https("https://");

  if (url.compare(0, http.size(), http) == 0)
    protocol = http;

  if (url.compare(0, https.size(), https) == 0)
    protocol = https;

  if (!protocol.empty()) {
    size_t pos = url.find_first_of(":", protocol.size());

    if (pos == std::string::npos) {
      pos = url.size();
    }

    host = url.substr(protocol.size(), pos - protocol.size());

    if (pos < url.size() && url[pos] == ':') {
      size_t ppos = url.find_first_of("/", pos);
      if (ppos == std::string::npos) {
        ppos = url.size();
      }
      std::string portStr(url.substr(pos + 1, ppos - pos - 1));
      if (portStr.size() > 0) {
        port = std::stoi(portStr);
      }
    }
  }
}

// HTTPSecurityConfiguration
class HTTPSecurityConfiguration {
public:

  // Constructor
  /*!
   * Create a new HTTPSecurityConfiguration
   */
  HTTPSecurityConfiguration(bool need_client_certificate, std::string certificate,
      std::string private_key, std::string passphrase, std::string ca_certificate) :
        need_client_certificate_(need_client_certificate), certificate_(certificate),
        private_key_(private_key), passphrase_(passphrase), ca_certificate_(ca_certificate) {
    logger_ = logging::LoggerFactory<HTTPSecurityConfiguration>::getLogger();
  }
  // Destructor
  virtual ~HTTPSecurityConfiguration() {
  }

  HTTPSecurityConfiguration(std::shared_ptr<Configure> configure) {
    logger_ = logging::LoggerFactory<HTTPSecurityConfiguration>::getLogger();
    need_client_certificate_ = false;
    std::string clientAuthStr;
    if (configure->get(Configure::nifi_https_need_ClientAuth, clientAuthStr)) {
      org::apache::nifi::minifi::utils::StringUtils::StringToBool(clientAuthStr, this->need_client_certificate_);
    }

    if (configure->get(Configure::nifi_https_client_ca_certificate, this->ca_certificate_)) {
      logger_->log_info("HTTPSecurityConfiguration CA certificates: [%s]", this->ca_certificate_);
    }

    if (this->need_client_certificate_) {
      std::string passphrase_file;

      if (!(configure->get(Configure::nifi_https_client_certificate, this->certificate_) && configure->get(Configure::nifi_https_client_private_key, this->private_key_))) {
        logger_->log_error("Certificate and Private Key PEM file not configured for HTTPSecurityConfiguration, error: %s.", std::strerror(errno));
      }

      if (configure->get(Configure::nifi_https_client_pass_phrase, passphrase_file)) {
        // load the passphase from file
        std::ifstream file(passphrase_file.c_str(), std::ifstream::in);
        if (file.good()) {
          this->passphrase_.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
          file.close();
        }
      }

      logger_->log_info("HTTPSecurityConfiguration certificate: [%s], private key: [%s], passphrase file: [%s]", this->certificate_, this->private_key_, passphrase_file);
    }
  }

  /**
    * Configures a secure connection
    */
  void configureSecureConnection(CURL *http_session) {
    curl_easy_setopt(http_session, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(http_session, CURLOPT_CAINFO, this->ca_certificate_.c_str());
    curl_easy_setopt(http_session, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(http_session, CURLOPT_SSL_VERIFYPEER, 1L);
    if (this->need_client_certificate_) {
      CURLcode ret;
      ret = curl_easy_setopt(http_session, CURLOPT_SSL_CTX_FUNCTION,
          &HTTPSecurityConfiguration::configureSSLContext);
      if (ret != CURLE_OK)
        logger_->log_error("CURLOPT_SSL_CTX_FUNCTION not supported %d", ret);
      curl_easy_setopt(http_session, CURLOPT_SSL_CTX_DATA,
          static_cast<void*>(this));
      curl_easy_setopt(http_session, CURLOPT_SSLKEYTYPE, "PEM");
    }
  }

  static CURLcode configureSSLContext(CURL *curl, void *ctx, void *param) {
    minifi::utils::HTTPSecurityConfiguration *config =
        static_cast<minifi::utils::HTTPSecurityConfiguration *>(param);
    SSL_CTX* sslCtx = static_cast<SSL_CTX*>(ctx);

    SSL_CTX_load_verify_locations(sslCtx, config->ca_certificate_.c_str(), 0);
    SSL_CTX_use_certificate_file(sslCtx, config->certificate_.c_str(),
        SSL_FILETYPE_PEM);
    SSL_CTX_set_default_passwd_cb(sslCtx,
        HTTPSecurityConfiguration::pemPassWordCb);
    SSL_CTX_set_default_passwd_cb_userdata(sslCtx, param);
    SSL_CTX_use_PrivateKey_file(sslCtx, config->private_key_.c_str(),
        SSL_FILETYPE_PEM);
    // verify private key
    if (!SSL_CTX_check_private_key(sslCtx)) {
      config->logger_->log_error(
          "Private key does not match the public certificate, error : %s",
          std::strerror(errno));
      return CURLE_FAILED_INIT;
    }

    config->logger_->log_debug(
        "HTTPSecurityConfiguration load Client Certificates OK");
    return CURLE_OK;
  }

  static int pemPassWordCb(char *buf, int size, int rwflag, void *param) {
    minifi::utils::HTTPSecurityConfiguration *config =
        static_cast<minifi::utils::HTTPSecurityConfiguration *>(param);

    if (config->passphrase_.length() > 0) {
      memset(buf, 0x00, size);
      memcpy(buf, config->passphrase_.c_str(),
          config->passphrase_.length() - 1);
      return config->passphrase_.length() - 1;
    }
    return 0;
  }

protected:
  bool need_client_certificate_;
  std::string certificate_;
  std::string private_key_;
  std::string passphrase_;
  std::string ca_certificate_;

private:
  std::shared_ptr<logging::Logger> logger_;
};

static std::string get_token(std::string loginUrl, std::string username, std::string password, HTTPSecurityConfiguration &securityConfig) {
  utils::HTTPRequestResponse content;
  std::string token;
  CURL *login_session = curl_easy_init();
  if (loginUrl.find("https") != std::string::npos) {
     securityConfig.configureSecureConnection(login_session);
   }
  curl_easy_setopt(login_session, CURLOPT_URL, loginUrl.c_str());
  struct curl_slist *list = NULL;
  list = curl_slist_append(list, "Content-Type: application/x-www-form-urlencoded");
  list = curl_slist_append(list, "Accept: text/plain");
  curl_easy_setopt(login_session, CURLOPT_HTTPHEADER, list);
  std::string payload = "username=" + username + "&" + "password=" + password;
  char *output = curl_easy_escape(login_session, payload.c_str(), payload.length());
  curl_easy_setopt(login_session, CURLOPT_WRITEFUNCTION,
      &utils::HTTPRequestResponse::recieve_write);
  curl_easy_setopt(login_session, CURLOPT_WRITEDATA,
      static_cast<void*>(&content));
  curl_easy_setopt(login_session, CURLOPT_POSTFIELDSIZE, strlen(output));
  curl_easy_setopt(login_session, CURLOPT_POSTFIELDS, output);
  curl_easy_setopt(login_session, CURLOPT_POST, 1);
  CURLcode res = curl_easy_perform(login_session);
  curl_slist_free_all(list);
  curl_free(output);
  if (res == CURLE_OK) {
    std::string response_body(content.data.begin(), content.data.end());
    int64_t http_code = 0;
    curl_easy_getinfo(login_session, CURLINFO_RESPONSE_CODE, &http_code);
    char *content_type;
    /* ask for the content-type */
    curl_easy_getinfo(login_session, CURLINFO_CONTENT_TYPE, &content_type);

    bool isSuccess = ((int32_t) (http_code / 100)) == 2
        && res != CURLE_ABORTED_BY_CALLBACK;
    bool body_empty = IsNullOrEmpty(content.data);

    if (isSuccess && !body_empty) {
      token = "Bearer " + response_body;
    }
  }
  curl_easy_cleanup(login_session);

  return token;
}


} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
