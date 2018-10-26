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

#include "controllers/SSLContextService.h"

#ifdef OPENSSL_SUPPORT
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif
#include <string>
#include <memory>
#include <set>
#include "core/Property.h"
#include "io/validation.h"
#include "properties/Configure.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

void SSLContextService::initialize() {
  if (initialized_)
    return;

  std::lock_guard<std::mutex> lock(initialization_mutex_);

  ControllerService::initialize();

  initializeTLS();

  initialized_ = true;
}

/**
 * If OpenSSL is not installed we may still continue operations. Nullptr will
 * be returned and it will be up to the caller to determien if this failure is
 * recoverable.
 */
std::unique_ptr<SSLContext> SSLContextService::createSSLContext() {
#ifdef OPENSSL_SUPPORT
  SSL_library_init();
  const SSL_METHOD *method;

  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  method = TLSv1_2_client_method();
  SSL_CTX *ctx = SSL_CTX_new(method);

  if (!IsNullOrEmpty(certificate)) {
    if (SSL_CTX_use_certificate_file(ctx, certificate.c_str(), SSL_FILETYPE_PEM) <= 0) {
      logger_->log_error("Could not create load certificate, error : %s", std::strerror(errno));
      return nullptr;
    }
    if (!IsNullOrEmpty(passphrase_)) {
      SSL_CTX_set_default_passwd_cb_userdata(ctx, &passphrase_);
      SSL_CTX_set_default_passwd_cb(ctx, pemPassWordCb);
    }
  }

  if (!IsNullOrEmpty(private_key_)) {
    int retp = SSL_CTX_use_PrivateKey_file(ctx, private_key_.c_str(), SSL_FILETYPE_PEM);
    if (retp != 1) {
      logger_->log_error("Could not create load private key,%i on %s error : %s", retp, private_key_, std::strerror(errno));
      return nullptr;
    }

    if (!SSL_CTX_check_private_key(ctx)) {
      logger_->log_error("Private key does not match the public certificate, error : %s", std::strerror(errno));
      return nullptr;
    }
  }

  int retp = SSL_CTX_load_verify_locations(ctx, ca_certificate_.c_str(), 0);
  if (retp == 0) {
    logger_->log_error("Can not load CA certificate %s, Exiting, error : %s", ca_certificate_, std::strerror(errno));
  }
  return std::unique_ptr<SSLContext>(new SSLContext(ctx));
#else
  return nullptr;
#endif
}

const std::string &SSLContextService::getCertificateFile() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return certificate;
}

const std::string &SSLContextService::getPassphrase() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return passphrase_;
}

const std::string &SSLContextService::getPassphraseFile() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return passphrase_file_;
}

const std::string &SSLContextService::getPrivateKeyFile() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return private_key_;
}

const std::string &SSLContextService::getCACertificate() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return ca_certificate_;
}

void SSLContextService::onEnable() {
  valid_ = true;
  core::Property property("Client Certificate", "Client Certificate");
  core::Property privKey("Private Key", "Private Key file");
  core::Property passphrase_prop("Passphrase", "Client passphrase. Either a file or unencrypted text");
  core::Property caCert("CA Certificate", "CA certificate file");
  std::string default_dir;
  if (nullptr != configuration_)
    configuration_->get(Configure::nifi_default_directory, default_dir);

  logger_->log_trace("onEnable()");

  if (getProperty(property.getName(), certificate) && getProperty(privKey.getName(), private_key_)) {
    std::ifstream cert_file(certificate);
    std::ifstream priv_file(private_key_);
    if (!cert_file.good()) {
      logger_->log_info("%s not good", certificate);
      std::string test_cert = default_dir + certificate;
      std::ifstream cert_file_test(test_cert);
      if (cert_file_test.good()) {
        certificate = test_cert;
        logger_->log_debug("%s now good", certificate);
      } else {
        logger_->log_warn("%s still not good", test_cert);
        valid_ = false;
      }
      cert_file_test.close();
    }

    if (!priv_file.good()) {
      std::string test_priv = default_dir + private_key_;
      std::ifstream private_file_test(test_priv);
      if (private_file_test.good()) {
        private_key_ = test_priv;
      } else {
        valid_ = false;
      }
      private_file_test.close();
    }
    cert_file.close();
    priv_file.close();

  } else {
    logger_->log_debug("Certificate empty");
  }
  if (!getProperty(passphrase_prop.getName(), passphrase_)) {
    logger_->log_debug("No pass phrase for %s", certificate);
  } else {
    std::ifstream passphrase_file(passphrase_);
    if (passphrase_file.good()) {
      passphrase_file_ = passphrase_;
      // we should read it from the file
      passphrase_.assign((std::istreambuf_iterator<char>(passphrase_file)), std::istreambuf_iterator<char>());
    } else {
      std::string test_passphrase = default_dir + passphrase_;
      std::ifstream passphrase_file_test(test_passphrase);
      if (passphrase_file_test.good()) {
        passphrase_ = test_passphrase;
        passphrase_file_ = test_passphrase;
        passphrase_.assign((std::istreambuf_iterator<char>(passphrase_file_test)), std::istreambuf_iterator<char>());
      } else {
        // not an invalid file since we support a passphrase of unencrypted text
      }
      passphrase_file_test.close();
    }
    passphrase_file.close();
  }
  // load CA certificates
  if (!getProperty(caCert.getName(), ca_certificate_)) {
    logger_->log_error("Can not load CA certificate.");
  } else {
    std::ifstream cert_file(ca_certificate_);
    if (!cert_file.good()) {
      std::string test_ca_cert = default_dir + ca_certificate_;
      std::ifstream ca_cert_file_file_test(test_ca_cert);
      if (ca_cert_file_file_test.good()) {
        ca_certificate_ = test_ca_cert;
      } else {
        valid_ = false;
      }
      ca_cert_file_file_test.close();
    }
    cert_file.close();
  }
}

void SSLContextService::initializeTLS() {
  core::Property property("Client Certificate", "Client Certificate");
  core::Property privKey("Private Key", "Private Key file");
  core::Property passphrase_prop("Passphrase", "Client passphrase. Either a file or unencrypted text");
  core::Property caCert("CA Certificate", "CA certificate file");
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(property);
  supportedProperties.insert(privKey);
  supportedProperties.insert(passphrase_prop);
  supportedProperties.insert(caCert);
  setSupportedProperties(supportedProperties);
}

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
