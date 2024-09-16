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
#include "SFTPClient.h"
#include <algorithm>
#include <exception>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "utils/StringUtils.h"
#include "utils/gsl.h"
#include "minifi-cpp/io/OutputStream.h"
#include "minifi-cpp/io/InputStream.h"

namespace org::apache::nifi::minifi::utils {

static const char* sftp_strerror(unsigned long err) {  // NOLINT(runtime/int,google-runtime-int) unsigned long comes from libssh2 API
  switch (err) {
    case LIBSSH2_FX_OK:
      return "LIBSSH2_FX_OK";
    case LIBSSH2_FX_EOF:
      return "LIBSSH2_FX_EOF";
    case LIBSSH2_FX_NO_SUCH_FILE:
      return "LIBSSH2_FX_NO_SUCH_FILE";
    case LIBSSH2_FX_PERMISSION_DENIED:
      return "LIBSSH2_FX_PERMISSION_DENIED";
    case LIBSSH2_FX_FAILURE:
      return "LIBSSH2_FX_FAILURE";
    case LIBSSH2_FX_BAD_MESSAGE:
      return "LIBSSH2_FX_BAD_MESSAGE";
    case LIBSSH2_FX_NO_CONNECTION:
      return "LIBSSH2_FX_NO_CONNECTION";
    case LIBSSH2_FX_CONNECTION_LOST:
      return "LIBSSH2_FX_CONNECTION_LOST";
    case LIBSSH2_FX_OP_UNSUPPORTED:
      return "LIBSSH2_FX_OP_UNSUPPORTED";
    case LIBSSH2_FX_INVALID_HANDLE:
      return "LIBSSH2_FX_INVALID_HANDLE";
    case LIBSSH2_FX_NO_SUCH_PATH:
      return "LIBSSH2_FX_NO_SUCH_PATH";
    case LIBSSH2_FX_FILE_ALREADY_EXISTS:
      return "LIBSSH2_FX_FILE_ALREADY_EXISTS";
    case LIBSSH2_FX_WRITE_PROTECT:
      return "LIBSSH2_FX_WRITE_PROTECT";
    case LIBSSH2_FX_NO_MEDIA:
      return "LIBSSH2_FX_NO_MEDIA";
    case LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM:
      return "LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM";
    case LIBSSH2_FX_QUOTA_EXCEEDED:
      return "LIBSSH2_FX_QUOTA_EXCEEDED";
    case LIBSSH2_FX_UNKNOWN_PRINCIPAL:
      return "LIBSSH2_FX_UNKNOWN_PRINCIPAL";
    case LIBSSH2_FX_LOCK_CONFLICT:
      return "LIBSSH2_FX_LOCK_CONFLICT";
    case LIBSSH2_FX_DIR_NOT_EMPTY:
      return "LIBSSH2_FX_DIR_NOT_EMPTY";
    case LIBSSH2_FX_NOT_A_DIRECTORY:
      return "LIBSSH2_FX_NOT_A_DIRECTORY";
    case LIBSSH2_FX_INVALID_FILENAME:
      return "LIBSSH2_FX_INVALID_FILENAME";
    case LIBSSH2_FX_LINK_LOOP:
      return "LIBSSH2_FX_LINK_LOOP";
    default:
      return "Unknown error";
  }
}

static SFTPError libssh2_sftp_error_to_sftp_error(unsigned long libssh2_sftp_error) {  // NOLINT(runtime/int,google-runtime-int) unsigned long comes from libssh2 API
  switch (libssh2_sftp_error) {
    case LIBSSH2_FX_OK:
      return SFTPError::Ok;
    case LIBSSH2_FX_NO_SUCH_FILE:
    case LIBSSH2_FX_NO_SUCH_PATH:
      return SFTPError::FileDoesNotExist;
    case LIBSSH2_FX_FILE_ALREADY_EXISTS:
      return SFTPError::FileAlreadyExists;
    case LIBSSH2_FX_PERMISSION_DENIED:
    case LIBSSH2_FX_WRITE_PROTECT:
    case LIBSSH2_FX_LOCK_CONFLICT:
      return SFTPError::PermissionDenied;
    case LIBSSH2_FX_NO_CONNECTION:
    case LIBSSH2_FX_CONNECTION_LOST:
      return SFTPError::CommunicationFailure;
    case LIBSSH2_FX_EOF:
    case LIBSSH2_FX_FAILURE:
    case LIBSSH2_FX_BAD_MESSAGE:
    case LIBSSH2_FX_OP_UNSUPPORTED:
    case LIBSSH2_FX_INVALID_HANDLE:
    case LIBSSH2_FX_NO_MEDIA:
    case LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM:
    case LIBSSH2_FX_QUOTA_EXCEEDED:
    case LIBSSH2_FX_UNKNOWN_PRINCIPAL:
    case LIBSSH2_FX_DIR_NOT_EMPTY:
    case LIBSSH2_FX_NOT_A_DIRECTORY:
    case LIBSSH2_FX_INVALID_FILENAME:
    case LIBSSH2_FX_LINK_LOOP:
    default:
      return SFTPError::Unexpected;
  }
}

LastSFTPError::LastSFTPError()
    : sftp_error_set_(false)
    , libssh2_sftp_error_(LIBSSH2_FX_OK)
    , sftp_error_(SFTPError::Ok) {
}

LastSFTPError& LastSFTPError::setLibssh2Error(unsigned long libssh2_sftp_error) {  // NOLINT(runtime/int,google-runtime-int) unsigned long comes from libssh2 API
  sftp_error_set_ = false;
  libssh2_sftp_error_ = libssh2_sftp_error;
  return *this;
}

LastSFTPError& LastSFTPError::setSftpError(const SFTPError& sftp_error) {
  sftp_error_set_ = true;
  sftp_error_ = sftp_error;
  return *this;
}

LastSFTPError::operator unsigned long() const {  // NOLINT(runtime/int,google-runtime-int) unsigned long comes from libssh2 API
  if (sftp_error_set_) {
    return LIBSSH2_FX_OK;
  } else {
    return libssh2_sftp_error_;
  }
}

LastSFTPError::operator SFTPError() const {
  if (sftp_error_set_) {
    return sftp_error_;
  } else {
    return libssh2_sftp_error_to_sftp_error(libssh2_sftp_error_);
  }
}


SFTPClient::SFTPClient(std::string hostname, uint16_t port, std::string username)
    : logger_(core::logging::LoggerFactory<SFTPClient>::getLogger()),
      hostname_(std::move(hostname)),
      port_(port),
      username_(std::move(username)),
      curl_errorbuffer_(CURL_ERROR_SIZE, '\0'),
      easy_(curl_easy_init()) {
  if (easy_ == nullptr) {
    throw std::runtime_error("Cannot create curl easy handle");
  }
  ssh_session_ = libssh2_session_init();
  if (ssh_session_ == nullptr) {
    curl_easy_cleanup(easy_);
    throw std::runtime_error("Cannot create ssh session handler");
  }
}

SFTPClient::~SFTPClient() {
    if (sftp_session_ != nullptr) {
      libssh2_sftp_shutdown(sftp_session_);
    }
    if (ssh_known_hosts_ != nullptr) {
      libssh2_knownhost_free(ssh_known_hosts_);
    }
    if (ssh_session_ != nullptr) {
      libssh2_session_disconnect(ssh_session_, "Normal Shutdown");
      libssh2_session_free(ssh_session_);
    }
    if (easy_ != nullptr) {
      curl_easy_cleanup(easy_);
    }
  logger_->log_trace("Closing SFTPClient for {}:{}", hostname_, port_);
}

bool SFTPClient::setVerbose() {
  return curl_easy_setopt(easy_, CURLOPT_VERBOSE, 1L) == CURLE_OK;
}

bool SFTPClient::setHostKeyFile(const std::string& host_key_file_path, bool strict_host_checking) {
  if (ssh_known_hosts_ != nullptr) {
    return false;
  }
  ssh_known_hosts_ = libssh2_knownhost_init(ssh_session_);
  if (ssh_known_hosts_ == nullptr) {
    char *err_msg = nullptr;
    libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
    logger_->log_error("Failed to init knownhost structure, error: {}", err_msg);
    return false;
  }
  if (libssh2_knownhost_readfile(ssh_known_hosts_, host_key_file_path.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH) <= 0) {
    char *err_msg = nullptr;
    libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
    logger_->log_error("Failed to read host file {}, error: {}", host_key_file_path.c_str(), err_msg);
    return false;
  }
  strict_host_checking_ = strict_host_checking;
  return true;
}

void SFTPClient::setPasswordAuthenticationCredentials(const std::string& password) {
  password_authentication_enabled_ = true;
  password_ = password;
}

void SFTPClient::setPublicKeyAuthenticationCredentials(const std::string& private_key_file_path, const std::string& private_key_passphrase) {
  public_key_authentication_enabled_ = true;
  private_key_file_path_ = private_key_file_path;
  private_key_passphrase_ = private_key_passphrase;
}

bool SFTPClient::setProxy(ProxyType type, const http::HTTPProxy& proxy) {
  switch (type) {
    case ProxyType::Http:
      if (curl_easy_setopt(easy_, CURLOPT_PROXYTYPE, CURLPROXY_HTTP) != CURLE_OK) {
        return false;
      }
      if (curl_easy_setopt(easy_, CURLOPT_HTTPPROXYTUNNEL, 1L) != CURLE_OK) {
        return false;
      }
      break;
    case ProxyType::Socks:
      if (curl_easy_setopt(easy_, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5) != CURLE_OK) {
        return false;
      }
      break;
  }
  std::stringstream proxy_string;
  proxy_string << proxy.host << ":" << proxy.port;
  return curl_easy_setopt(easy_, CURLOPT_PROXY, proxy_string.str().c_str()) == CURLE_OK;
}

bool SFTPClient::setConnectionTimeout(std::chrono::milliseconds timeout) {
  return curl_easy_setopt(easy_, CURLOPT_CONNECTTIMEOUT_MS, timeout.count()) == CURLE_OK;
}

void SFTPClient::setDataTimeout(std::chrono::milliseconds timeout) {
  libssh2_session_set_timeout(ssh_session_, gsl::narrow<long>(timeout.count()));  // NOLINT(runtime/int,google-runtime-int) long due to libssh2 API
}

void SFTPClient::setSendKeepAlive(bool send_keepalive) {
  /*
   * Some SSH servers don't like if we send keepalives before we're connected,
   * but libssh2 sends keepalives before that, so we will set up keepalives after
   * a successful connection.
   */
  send_keepalive_ = send_keepalive;
}

bool SFTPClient::setUseCompression(bool /*use_compression*/) {
  return libssh2_session_flag(ssh_session_, LIBSSH2_FLAG_COMPRESS, 1) == 0;
}

bool SFTPClient::connect() {
  if (connected_) {
    return true;
  }

  /* Setting up curl request */
  std::stringstream uri_ss;
  uri_ss << hostname_ << ":" << port_;
  auto uri = uri_ss.str();
  if (curl_easy_setopt(easy_, CURLOPT_URL, uri.c_str()) != CURLE_OK) {
    return false;
  }
  if (curl_easy_setopt(easy_, CURLOPT_ERRORBUFFER, curl_errorbuffer_.data()) != CURLE_OK) {
    return false;
  }
  if (curl_easy_setopt(easy_, CURLOPT_NOSIGNAL, 1L) != CURLE_OK) {
    return false;
  }
  if (curl_easy_setopt(easy_, CURLOPT_CONNECT_ONLY, 1L) != CURLE_OK) {
    return false;
  }

  /* Connecting to proxy, if needed, then to the host */
  CURLcode curl_res = curl_easy_perform(easy_);
  if (curl_res != CURLE_OK) {
    logger_->log_error("Failed to connect to {}, curl error code: {}, detailed error message: {}",
        uri.c_str(),
        curl_easy_strerror(curl_res),
        curl_errorbuffer_.data());
    return false;
  }

  /* Getting socket from curl */
#ifdef WIN32
  curl_socket_t sockfd;
#else
  long sockfd = 0;  // NOLINT(runtime/int,google-runtime-int) long due to libcurl API
#endif
  curl_res = curl_easy_getinfo(easy_, CURLINFO_ACTIVESOCKET, &sockfd);
  if (curl_res != CURLE_OK) {
    return false;
  }

  /* Establishing SSH connection */
  if (libssh2_session_handshake(ssh_session_, gsl::narrow<libssh2_socket_t>(sockfd)) != 0) {
    char *err_msg = nullptr;
    libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
    logger_->log_info("Failed to establish SSH connection, error: {}", err_msg);
    return false;
  }

  /* Checking remote host */
  if (ssh_known_hosts_ != nullptr) {
    auto hostkey_opt = [this]() -> std::optional<std::tuple<std::string_view, int>> {
      size_t hostkey_len = 0U;
      int type = LIBSSH2_HOSTKEY_TYPE_UNKNOWN;
      const char *hostkey_ptr = libssh2_session_hostkey(ssh_session_, &hostkey_len, &type);
      if (hostkey_ptr == nullptr) {
        char *err_msg = nullptr;
        libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
        logger_->log_info("Failed to get session hostkey, error: {}", err_msg);
        return std::nullopt;
      }
      const auto hostkey = std::string_view{hostkey_ptr, hostkey_len};
      return std::make_optional(std::make_tuple(hostkey, type));
    }();
    if (!hostkey_opt) return false;
    const auto [hostkey, hostkey_type] = *std::move(hostkey_opt);
    int keybit = 0;
    switch (hostkey_type) {
      case LIBSSH2_HOSTKEY_TYPE_RSA:
        keybit = LIBSSH2_KNOWNHOST_KEY_SSHRSA;
        break;
      case LIBSSH2_HOSTKEY_TYPE_DSS:
        keybit = LIBSSH2_KNOWNHOST_KEY_SSHDSS;
        break;
      default:
        logger_->log_error("Unknown host key type: {}", hostkey_type);
        return false;
    }
    struct libssh2_knownhost* known_host = nullptr;
    int keycheck_result = libssh2_knownhost_checkp(ssh_known_hosts_,
                            hostname_.c_str(),
                            -1 /*port*/,
                            hostkey.data(), hostkey.size(),
                            LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                            LIBSSH2_KNOWNHOST_KEYENC_RAW |
                            keybit,
                            &known_host);
    bool host_match = false;
    switch (keycheck_result) {
      case LIBSSH2_KNOWNHOST_CHECK_FAILURE:
        logger_->log_warn("Failed to verify host key for {}", hostname_.c_str());
        break;
      case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND:
        logger_->log_warn("Host {} not found in the host key file", hostname_.c_str());
        break;
      case LIBSSH2_KNOWNHOST_CHECK_MISMATCH: {
        auto hostkey_b64 = utils::string::to_base64(hostkey);
        logger_->log_warn("Host key mismatch for {}, expected: {}, actual: {}", hostname_.c_str(),
                          known_host == nullptr ? "" : known_host->key, hostkey_b64.c_str());
        break;
      }
      case LIBSSH2_KNOWNHOST_CHECK_MATCH:
        host_match = true;
        logger_->log_debug("Host key verification succeeded for {}", hostname_.c_str());
        break;
      default:
        logger_->log_error("Unknown libssh2_knownhost_checkp result: {}", keycheck_result);
        break;
    }
    if (strict_host_checking_ && !host_match) {
      return false;
    }
  } else {
    const char* fingerprint = libssh2_hostkey_hash(ssh_session_, LIBSSH2_HOSTKEY_HASH_SHA1);
    const auto fingerprint_span = gsl::make_span(fingerprint, 20);
    if (fingerprint == nullptr) {
      logger_->log_warn("Cannot get remote server fingerprint");
    } else {
      auto fingerprint_hex = utils::string::to_hex(fingerprint_span.as_span<const std::byte>());
      std::stringstream fingerprint_hex_colon;
      for (size_t i = 0; i < 20; i++) {
        fingerprint_hex_colon << fingerprint_hex.substr(i * 2, 2);
        if (i != 19) {
          fingerprint_hex_colon << ":";
        }
      }
      logger_->log_debug("SHA1 host key fingerprint for {} is {}", hostname_.c_str(), fingerprint_hex_colon.str().c_str());
    }
  }

  /* Getting possible authentication methods */
  bool authenticated = false;
  std::set<std::string> auth_methods;
  char* userauthlist = libssh2_userauth_list(ssh_session_, username_.c_str(), gsl::narrow<unsigned int>(username_.size()));
  if (userauthlist == nullptr) {
    if (libssh2_userauth_authenticated(ssh_session_) == 1) {
      authenticated = true;
      logger_->log_warn("SSH server authenticated with SSH_USERAUTH_NONE - this is unusual");
    } else {
      logger_->log_error("Failed to get supported SSH authentication methods");
      return false;
    }
  } else {
    auto methods_split = utils::string::split(userauthlist, ",");
    auth_methods.insert(std::make_move_iterator(methods_split.begin()), std::make_move_iterator(methods_split.end()));
  }

  /* Authenticating */
  if (!authenticated && public_key_authentication_enabled_ && auth_methods.count("publickey") == 1) {
    if (libssh2_userauth_publickey_fromfile_ex(ssh_session_,
                                               username_.c_str(),
                                               gsl::narrow<unsigned int>(username_.length()),
                                               nullptr /*publickey*/,
                                               private_key_file_path_.c_str(),
                                               private_key_passphrase_.c_str()) == 0) {
      authenticated = true;
      logger_->log_debug("Successfully authenticated with publickey");
    } else {
      char *err_msg = nullptr;
      libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
      logger_->log_info("Failed to authenticate with publickey, error: {}", err_msg);
    }
  }
  if (!authenticated && password_authentication_enabled_ && auth_methods.count("password") == 1) {
    if (libssh2_userauth_password(ssh_session_, username_.c_str(), password_.c_str()) == 0) {
      authenticated = true;
      logger_->log_debug("Successfully authenticated with password");
    } else {
      char *err_msg = nullptr;
      libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
      logger_->log_info("Failed to authenticate with password, error: {}", err_msg);
    }
  }
  if (!authenticated) {
    logger_->log_error("Could not authenticate with any available method");
    return false;
  }

  /* Initializing SFTP session */
  sftp_session_ = libssh2_sftp_init(ssh_session_);
  if (sftp_session_ == nullptr) {
    char *err_msg = nullptr;
    libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
    logger_->log_error("Failed to initialize SFTP session, error: {}", err_msg);
    return false;
  }

  connected_ = true;

  /* Set up keepalive config if needed */
  if (send_keepalive_) {
    libssh2_keepalive_config(ssh_session_, 0 /*want_reply*/, 10U /*interval*/);
  }

  return true;
}

bool SFTPClient::sendKeepAliveIfNeeded(int &seconds_to_next) {
  if (libssh2_keepalive_send(ssh_session_, &seconds_to_next) != 0) {
    char *err_msg = nullptr;
    libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
    logger_->log_error("Failed to send keepalive to {}@{}:{}, error: {}",
        username_,
        hostname_,
        port_,
        err_msg);
    return false;
  }
  return true;
}

SFTPError SFTPClient::getLastError() const {
  return last_error_;
}

std::optional<uint64_t> SFTPClient::getFile(const std::string& path, io::OutputStream& output, int64_t expected_size /*= -1*/) {
  /**
   * SFTP servers should not set the mode of an existing file on open
   * (see https://tools.ietf.org/html/draft-ietf-secsh-filexfer-13, Page 33
   * "The 'attrs' field is ignored if an existing file is opened."
   * Unfortunately this is a later SFTP version specification than implemented by most servers.)
   * But because this is the intuitively correct behaviour (especially when opening a file for read only),
   * most servers (OpenSSH for example) implement it this way.
   * mina-sshd, the server we use for testing, however did not until recently,
   * causing all files we read to be set to 0000.
   * The fix to make it behave correctly has been merged back to master, but not yet released:
   * https://github.com/apache/mina-sshd/commit/19adb39e4706929b6e5a1b2df056a2b2a29fac4d
   * If we encounter real servers that behave like this, a workaround would be to stat before opening the file
   * and "re-setting" the mode we read earlier on open.
   * An another option would be to patch libssh2 to not send permissions in attrs when opening a file for read only.
   */
  LIBSSH2_SFTP_HANDLE *file_handle = libssh2_sftp_open_ex(sftp_session_, path.c_str(), gsl::narrow<unsigned int>(path.size()), LIBSSH2_FXF_READ, 0 /*mode*/, LIBSSH2_SFTP_OPENFILE);
  if (file_handle == nullptr) {
    int ssh_errno = libssh2_session_last_errno(ssh_session_);
    /* We can only get the sftp error in this case if the ssh error is a protocol error */
    if (ssh_errno == LIBSSH2_ERROR_SFTP_PROTOCOL) {
      last_error_.setLibssh2Error(libssh2_sftp_last_error(sftp_session_));
      logger_->log_error("Failed to open remote file \"{}\", error: {}", path.c_str(), sftp_strerror(last_error_));
    } else {
      last_error_.setSftpError(SFTPError::IoError);
      char *err_msg = nullptr;
      libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
      logger_->log_error("Failed to open remote file \"{}\" due to an underlying SSH error: {}", path.c_str(), err_msg);
    }
    return std::nullopt;
  }
  const auto guard = gsl::finally([&file_handle]() {
    libssh2_sftp_close(file_handle);
  });

  const size_t buf_size = expected_size < 0 ? MAX_BUFFER_SIZE : std::min<size_t>(expected_size, MAX_BUFFER_SIZE);
  std::vector<uint8_t> buf(buf_size);
  uint64_t total_read = 0U;
  do {
    ssize_t read_ret = libssh2_sftp_read(file_handle, reinterpret_cast<char*>(buf.data()), buf.size());
    if (read_ret < 0) {
      last_error_.setSftpError(SFTPError::IoError);
      logger_->log_error("Failed to read remote file \"{}\"", path.c_str());
      return std::nullopt;
    } else if (read_ret == 0) {
      logger_->log_trace("EOF while reading remote file \"{}\"", path.c_str());
      break;
    }
    logger_->log_trace("Read {} bytes from remote file \"{}\"", read_ret, path.c_str());
    total_read += gsl::narrow<uint64_t>(read_ret);
    auto remaining = read_ret;
    while (remaining > 0) {
      const auto write_ret = output.write(buf.data() + (read_ret - remaining), gsl::narrow<size_t>(remaining));
      if (io::isError(write_ret)) {
        last_error_.setLibssh2Error(LIBSSH2_FX_OK);
        logger_->log_error("Failed to write output");
        return std::nullopt;
      }
      remaining -= gsl::narrow<decltype(remaining)>(write_ret);
    }
  } while (true);

  if (expected_size >= 0 && total_read != gsl::narrow<uint64_t>(expected_size)) {
    last_error_.setLibssh2Error(LIBSSH2_FX_OK);
    logger_->log_error("Remote file \"{}\" has unexpected size, expected: {}, actual: {}", path.c_str(), expected_size, total_read);
    return std::nullopt;
  }

  return total_read;
}

std::optional<uint64_t> SFTPClient::putFile(const std::string& path, io::InputStream& input, bool overwrite, int64_t expected_size /*= -1*/) {
  int flags = LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | (overwrite ? LIBSSH2_FXF_TRUNC : LIBSSH2_FXF_EXCL);
  logger_->log_trace("Opening remote file \"{}\"", path.c_str());
  LIBSSH2_SFTP_HANDLE *file_handle = libssh2_sftp_open_ex(sftp_session_, path.c_str(), gsl::narrow<unsigned int>(path.size()), flags, 0644, LIBSSH2_SFTP_OPENFILE);
  if (file_handle == nullptr) {
    int ssh_errno = libssh2_session_last_errno(ssh_session_);
    /* We can only get the sftp error in this case if the ssh error is a protocol error */
    if (ssh_errno == LIBSSH2_ERROR_SFTP_PROTOCOL) {
      last_error_.setLibssh2Error(libssh2_sftp_last_error(sftp_session_));
      logger_->log_error("Failed to open remote file \"{}\", error: {}", path.c_str(), sftp_strerror(last_error_));
    } else {
      last_error_.setSftpError(SFTPError::IoError);
      char *err_msg = nullptr;
      libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
      logger_->log_error("Failed to open remote file \"{}\" due to an underlying SSH error: {}", path.c_str(), err_msg);
    }
  }
  const auto guard = gsl::finally([this, &file_handle, &path]() {
    logger_->log_trace("Closing remote file \"{}\"", path.c_str());
    libssh2_sftp_close(file_handle);
  });

  /* If they just want a zero byte file, we are done */
  if (expected_size == 0) {
    return 0;
  }

  const size_t buf_size = expected_size < 0 ? MAX_BUFFER_SIZE : std::min(gsl::narrow<size_t>(expected_size), MAX_BUFFER_SIZE);
  std::vector<std::byte> buf(buf_size);
  uint64_t total_read = 0U;
  do {
    const auto read_ret = input.read(buf);
    if (io::isError(read_ret)) {
      last_error_.setLibssh2Error(LIBSSH2_FX_OK);
      logger_->log_error("Error while reading input");
      return std::nullopt;
    } else if (read_ret == 0) {
      logger_->log_trace("EOF while reading input");
      break;
    }
    logger_->log_trace("Read {} bytes", read_ret);
    total_read += read_ret;
    auto remaining = read_ret;
    while (remaining > 0) {
      const auto write_ret = libssh2_sftp_write(file_handle, reinterpret_cast<char*>(buf.data() + (read_ret - remaining)), remaining);
      if (write_ret < 0) {
        last_error_.setSftpError(SFTPError::IoError);
        logger_->log_error("Failed to write remote file \"{}\"", path.c_str());
        return std::nullopt;
      }
      logger_->log_trace("Wrote {} bytes to remote file \"{}\"", write_ret, path.c_str());
      remaining -= gsl::narrow<size_t>(write_ret);
    }
  } while (true);

  if (expected_size >= 0 && total_read != gsl::narrow<size_t>(expected_size)) {
    last_error_.setLibssh2Error(LIBSSH2_FX_OK);
    logger_->log_error("Input has unexpected size, expected: {}, actual: {}", path.c_str(), expected_size, total_read);
    return std::nullopt;
  }

  return total_read;
}

bool SFTPClient::rename(const std::string& source_path, const std::string& target_path, bool overwrite) {
  int flags = 0;
  if (overwrite) {
    flags = LIBSSH2_SFTP_RENAME_ATOMIC | LIBSSH2_SFTP_RENAME_NATIVE | LIBSSH2_SFTP_RENAME_OVERWRITE;
  } else {
    flags = LIBSSH2_SFTP_RENAME_ATOMIC | LIBSSH2_SFTP_RENAME_NATIVE;
  }
  bool tried_deleting = false;
  while (libssh2_sftp_rename_ex(sftp_session_,
                              source_path.c_str(),
                              gsl::narrow<unsigned int>(source_path.length()),
                              target_path.c_str(),
                              gsl::narrow<unsigned int>(target_path.length()),
                              flags) != 0) {
    auto err = libssh2_sftp_last_error(sftp_session_);
    if (overwrite && err == LIBSSH2_FX_FILE_ALREADY_EXISTS && !tried_deleting) {
      /* It couldn't overwrite the file, let's delete it and try again */
      logger_->log_debug("Failed to overwrite file \"{}\" with rename, deleting instead", target_path.c_str());
      tried_deleting = true;
      if (!this->removeFile(target_path)) {
        return false;
      }
      continue;
    }
    last_error_.setLibssh2Error(libssh2_sftp_last_error(sftp_session_));
    logger_->log_error(R"(Failed to rename remote file "{}" to "{}", error: {})",
        source_path.c_str(),
        target_path.c_str(),
        sftp_strerror(last_error_));
    return false;
  }
  return true;
}

bool SFTPClient::createDirectoryHierarchy(const std::string& path) {
  if (path.empty()) {
    last_error_.setLibssh2Error(LIBSSH2_FX_OK);
    return false;
  }
  bool absolute = path[0] == '/';
  auto elements = utils::string::split(path, "/");
  std::stringstream dir;
  if (absolute) {
    dir << "/";
  }
  for (const auto& element : elements) {
    dir << element << "/";
    auto current_dir = dir.str();
    int res = libssh2_sftp_mkdir_ex(sftp_session_, current_dir.c_str(), gsl::narrow<unsigned int>(current_dir.length()), 0755);
    if (res < 0) {
      auto err = libssh2_sftp_last_error(sftp_session_);
      if (err != LIBSSH2_FX_FILE_ALREADY_EXISTS &&
          err != LIBSSH2_FX_FAILURE &&
          err != LIBSSH2_FX_PERMISSION_DENIED) {
        last_error_.setLibssh2Error(err);
        logger_->log_error("Failed to create remote directory \"{}\", error: {}", current_dir.c_str(), sftp_strerror(last_error_));
        return false;
      } else {
        logger_->log_debug("Non-fatal failure to create remote directory \"{}\", error: {}", current_dir.c_str(), sftp_strerror(err));
      }
    }
  }
  return true;
}

bool SFTPClient::removeFile(const std::string& path) {
  if (libssh2_sftp_unlink_ex(sftp_session_, path.c_str(), gsl::narrow<unsigned int>(path.size())) != 0) {
    last_error_.setLibssh2Error(libssh2_sftp_last_error(sftp_session_));
    logger_->log_error("Failed to remove remote file \"{}\", error: {}", path.c_str(), sftp_strerror(last_error_));
    return false;
  }
  return true;
}

bool SFTPClient::removeDirectory(const std::string& path) {
  if (libssh2_sftp_rmdir_ex(sftp_session_, path.c_str(), gsl::narrow<unsigned int>(path.size())) != 0) {
    last_error_.setLibssh2Error(libssh2_sftp_last_error(sftp_session_));
    logger_->log_error("Failed to remove remote directory \"{}\", error: {}", path.c_str(), sftp_strerror(last_error_));
    return false;
  }
  return true;
}

bool SFTPClient::listDirectory(const std::string& path, bool follow_symlinks,
    std::vector<std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>>& children_result) {
  LIBSSH2_SFTP_HANDLE *dir_handle = libssh2_sftp_open_ex(sftp_session_,
                                                          path.c_str(),
                                                          gsl::narrow<unsigned int>(path.length()),
                                                          0 /* flags */,
                                                          0 /* mode */,
                                                          LIBSSH2_SFTP_OPENDIR);
  if (dir_handle == nullptr) {
    last_error_.setLibssh2Error(libssh2_sftp_last_error(sftp_session_));
    logger_->log_error("Failed to open remote directory \"{}\", error: {}", path.c_str(), sftp_strerror(last_error_));
    return false;
  }
  const auto guard = gsl::finally([&dir_handle]() {
    libssh2_sftp_close(dir_handle);
  });

  LIBSSH2_SFTP_ATTRIBUTES attrs;
  std::vector<char> filename(4096U);
  std::vector<char> longentry(4096U);
  do {
    int ret = libssh2_sftp_readdir_ex(dir_handle,
                                      filename.data(),
                                      filename.size(),
                                      longentry.data(),
                                      longentry.size(),
                                      &attrs);
    if (ret < 0) {
      last_error_.setLibssh2Error(libssh2_sftp_last_error(sftp_session_));
      logger_->log_error("Failed to read remote directory \"{}\", error: {}", path.c_str(), sftp_strerror(last_error_));
      return false;
    } else if (ret == 0) {
      break;
    }
    if (follow_symlinks && attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS && LIBSSH2_SFTP_S_ISLNK(attrs.permissions)) {
      std::stringstream new_path;
      new_path << path << "/" << filename.data();
      auto orig_attrs = attrs;
      if (!this->stat(new_path.str(), true /*follow_symlinks*/, attrs)) {
        attrs = orig_attrs;
      }
    }
    children_result.emplace_back(std::string(filename.data()), std::string(longentry.data()), attrs);
  } while (true);
  return true;
}

bool SFTPClient::stat(const std::string& path, bool follow_symlinks, LIBSSH2_SFTP_ATTRIBUTES& result) {
  if (libssh2_sftp_stat_ex(sftp_session_,
                            path.c_str(),
                            gsl::narrow<unsigned int>(path.length()),
                            follow_symlinks ? LIBSSH2_SFTP_STAT : LIBSSH2_SFTP_LSTAT,
                            &result) != 0) {
    last_error_.setLibssh2Error(libssh2_sftp_last_error(sftp_session_));
    logger_->log_debug("Failed to stat remote path \"{}\", error: {}", path.c_str(), sftp_strerror(last_error_));
    return false;
  }
  return true;
}

bool SFTPClient::setAttributes(const std::string& path, const SFTPAttributes& input) {
  LIBSSH2_SFTP_ATTRIBUTES attrs;
  memset(&attrs, 0x00, sizeof(attrs));
  if ((!!(input.flags & SFTP_ATTRIBUTE_UID) != !!(input.flags & SFTP_ATTRIBUTE_GID)) ||
      (!!(input.flags & SFTP_ATTRIBUTE_MTIME) != !!(input.flags & SFTP_ATTRIBUTE_ATIME))) {
    /* Because we can only set these attributes in pairs, we must stat first to learn the other */
    if (!this->stat(path, false /*follow_symlinks*/, attrs)) {
      return false;
    }
  }
  attrs.flags = 0U;
  if (input.flags & SFTP_ATTRIBUTE_PERMISSIONS) {
    attrs.flags |= LIBSSH2_SFTP_ATTR_PERMISSIONS;
    attrs.permissions = input.permissions;
  }
  if (input.flags & SFTP_ATTRIBUTE_UID) {
    attrs.flags |= LIBSSH2_SFTP_ATTR_UIDGID;
    attrs.uid = gsl::narrow<unsigned long>(input.uid);  // NOLINT(runtime/int,google-runtime-int) unsigned long comes from libssh2 API
  }
  if (input.flags & SFTP_ATTRIBUTE_GID) {
    attrs.flags |= LIBSSH2_SFTP_ATTR_UIDGID;
    attrs.gid = gsl::narrow<unsigned long>(input.gid);  // NOLINT(runtime/int,google-runtime-int) unsigned long comes from libssh2 API
  }
  if (input.flags & SFTP_ATTRIBUTE_MTIME) {
    attrs.flags |= LIBSSH2_SFTP_ATTR_ACMODTIME;
    attrs.mtime = gsl::narrow<unsigned long>(input.mtime);  // NOLINT(runtime/int,google-runtime-int) unsigned long comes from libssh2 API
  }
  if (input.flags & SFTP_ATTRIBUTE_ATIME) {
    attrs.flags |= LIBSSH2_SFTP_ATTR_ACMODTIME;
    attrs.atime = gsl::narrow<unsigned long>(input.atime);  // NOLINT(runtime/int,google-runtime-int) unsigned long comes from libssh2 API
  }

  if (libssh2_sftp_stat_ex(sftp_session_,
                           path.c_str(),
                           gsl::narrow<unsigned int>(path.length()),
                           LIBSSH2_SFTP_SETSTAT,
                           &attrs) != 0) {
    last_error_.setLibssh2Error(libssh2_sftp_last_error(sftp_session_));
    logger_->log_debug("Failed to setstat on remote path \"{}\", error: {}", path.c_str(), sftp_strerror(last_error_));
    return false;
  }

  return true;
}

}  // namespace org::apache::nifi::minifi::utils
